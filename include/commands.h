#ifndef MYSH_COMMANDS_H_
#define MYSH_COMMANDS_H_

struct single_command
{
  int argc;
  char** argv;
};

void process_creation(int n_commands, struct single_command (*commands)[512]);

int evaluate_command(int n_commands, struct single_command (*commands)[512], char* buf);

void free_commands(int n_commands, struct single_command (*commands)[512]);

#endif // MYSH_COMMANDS_H_
