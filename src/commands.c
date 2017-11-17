#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdio_ext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>

#include "commands.h"
#include "built_in.h"

//used in socket programming
#define UNIX_PATH_MAX 108
#define SERVER_PATH "tpf_unix_sock.server"
#define SOCK_PATH "tpf_unix_sock.server"
#define CLIENT_PATH "tpf_unix_sock.client"

int pipe_flag = 0;

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}

void process_creation(int n_commands, struct single_command (*commands)[512]){
	int pid;
	int status;	
	pid = fork();
	char* token;
	struct single_command* com;
	if(!pipe_flag) com = (*commands);
	else	 com = (*commands) + 1;
	pipe_flag = 0;
	char* path = com->argv[0];

	//tokenize exe file name
	token = strrchr(com->argv[0], '/') + 1;
	char* arr[] = {token, com->argv[1], NULL};
	
	//if there is fork error		
	if(pid == -1){
		fprintf(stderr, "fork() fails..\n");
	}
	//if process is a child
	else if(pid == 0){
		execv(path, arr);
	}
	//if process is a parent
	else{
		wait(&status);
	}
 	return;
}

/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512], char* buf)
{
  if (n_commands > 0) {
    struct single_command* com = (*commands);

    assert(com->argc != 0);

    int built_in_pos = is_built_in_command(com->argv[0]);
    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) {
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
        }
      } else {
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return -1;
      }
    } else if(strchr(buf, '&')){	//it's about background, but I couldn't.
	int pid;
	char* token;
	if(daemon(1,1) == -1){
		fprintf(stderr, "daemon() fails..\n");
		return -1;
	}
	if((pid =fork()) == -1){
		fprintf(stderr, "fork() fails..\n");
		return -1;
	}
	else if(pid == 0){
		token = strrchr(com->argv[0], '/') + 1;
		char* arr[] = {token, NULL};
		sleep(1);
		if((pid =fork()) == -1){
			fprintf(stderr, "fork() fails..\n");
			return -1;
		}
		else if(pid != 0){
			exit(0);
		}		
		process_creation(n_commands, commands);
		printf("%d\n", getpid());
		execv(com->argv[0], arr);
	}
    } else if(strchr(buf, '/') || strchr(buf, '.')){
	if(n_commands > 1){
		struct sockaddr_un{
		unsigned short int sun_family;
		char sun_path[UNIX_PATH_MAX];
		};
		int server_sock, client_sock, len, rc;
		int bytes_rec = 0;
		struct sockaddr_un server_sockaddr;
		struct sockaddr_un client_sockaddr;
		char socket_buf[256];
		char socket_buf_client[256];	
		int backlog = 10;
		memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
		memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));
		memset(socket_buf, 0, 256);

		int pid;
	
		pid = fork();
		//if there is fork error		
		if(pid == -1){
			fprintf(stderr, "fork() fails..\n");
			return -1;
		}
		//if process is a child, it is the client.
		else if(pid == 0){	
			pipe_flag = 0;

			client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
			if(client_sock == -1){
//				printf("SOCKET ERROR: %d\n", sock_errno());
				exit(1);
			}
			
			client_sockaddr.sun_family = AF_UNIX;
			strcpy(client_sockaddr.sun_path, CLIENT_PATH);
			len = sizeof(client_sockaddr);		
	
			unlink(CLIENT_PATH);
			rc = bind(client_sock, (struct sockaddr*) &client_sockaddr, len);
			if(rc == -1){
//				printf("BIND ERROR: %d\n", sock_errno());
				close(client_sock);
				exit(1);
			}
			
			server_sockaddr.sun_family = AF_UNIX;
			strcpy(server_sockaddr.sun_path, SERVER_PATH);
			rc = connect(client_sock, (struct sockaddr*) &server_sockaddr, len);
			if(rc == -1){
//			printf("CONNECT ERROR: %d\n", sock_errno());
				close(client_sock);
				exit(1);
			}		
			int saved_stdout = dup(1);
			dup2(client_sock, STDOUT_FILENO);
			close(client_sock);
			process_creation(n_commands, commands);	
			read(client_sock, socket_buf_client, 256);
			//redirect stdout to terminal.
			dup2(saved_stdout, STDOUT_FILENO);
			close(saved_stdout);
			rc = send(client_sock, socket_buf_client, strlen(socket_buf_client), 0);
			if(rc == -1){
//				printf("SEND ERROR: %d\n", sock_errno());
				close(client_sock);
				exit(1);
			}
			close(client_sock);
		}
		//if process is a parent, it is the server.
		else{
			pipe_flag = 1;	

			server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
			if(server_sock == -1){
//				printf("SOCKET ERROR: %d\n", sock_errno());
				exit(1);
			}		
			
			server_sockaddr.sun_family = AF_UNIX;
			strcpy(server_sockaddr.sun_path, SOCK_PATH);
			len = sizeof(server_sockaddr);
	
			unlink(SOCK_PATH);
			rc = bind(server_sock, (struct sockaddr*) &server_sockaddr, len);
			if(rc == -1){
//				printf("BIND ERROR: %d\n", sock_errno());
				close(server_sock);
				exit(1);
			}
	
			rc = listen(server_sock, backlog);
			if(rc == -1){
//				printf("LISTEN ERROR: %d\n", sock_errno());
				close(server_sock);
				exit(1);
			}
	
			client_sock = accept(server_sock, (struct sockaddr*) &client_sockaddr, &len);	
			if(client_sock == -1){
//				printf("ACCEPT ERROR: %d\n", sock_errno());
				close(server_sock);
				close(client_sock);
				exit(1);
			}
			
			bytes_rec = recv(client_sock, socket_buf, sizeof(socket_buf), 0);
			if(bytes_rec == -1){
//				printf("RECV ERROR: %d\n", sock_errno());
				close(server_sock);
				close(client_sock);
				exit(1);
			}
			else	printf("Data recieved = \n%s\n", socket_buf);
			//server_sock = dup(STDIN_FILENO);			
			write(server_sock, socket_buf, strlen(socket_buf) + 1);	
			close(client_sock);
			
			int saved_stdin = dup(0);
			dup2(server_sock, STDIN_FILENO);
			close(server_sock);
			process_creation(n_commands, commands);	
			
			//redirect stdin to terminal			
			dup2(saved_stdin, STDIN_FILENO);
			close(saved_stdin);			
		}
	}
	else{	
		pipe_flag = 0;
		process_creation(n_commands, commands);
	}	
    } else if (strcmp(com->argv[0], "") == 0) {
      return 0;
    } else if (strcmp(com->argv[0], "exit") == 0) {
      return 1;
    } else {
      fprintf(stderr, "%s: command not found\n", com->argv[0]);
      return -1;
    }
  }
  return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
    for(int i = 0; i < n_commands; ++i){
	struct single_command *com = (*commands) + i;
	int argc = com->argc;
	char** argv = com->argv;
	
	for (int j = 0; j < argc; ++j) {
     	 free(argv[j]);
    	}
	free(argv);
    }

    memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
