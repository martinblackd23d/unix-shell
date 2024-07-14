// simple unix shell

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void clean_whitespace(char *input);
int count_char(char *input, char c);
int execute_command(char *command, char **path);

int main(int argc, char *argv[]) {
	FILE *source;
	int interactive = 0;
	char **path;
	char error_message[30] = "An error has occurred\n";

	// init path
	path = malloc(2 * sizeof(char *));
	path[0] = "/bin";
	path[1] = NULL;

	// check for input file
	if (argc == 1) {
		source = stdin;
		interactive = 1;
	} else if (argc == 2) {
		source = fopen(argv[1], "r");
		if (source == NULL) {
			write(STDERR_FILENO, error_message, strlen(error_message));
			exit(1);
		}
	} else {
		write(STDERR_FILENO, error_message, strlen(error_message));
		exit(1);
	}

	// main loop
	while (1) {
		// print prompt
		if (interactive) {
			printf("wish> ");
		}
		
		// read input
		char *input = NULL;
		size_t len = 0;
		getline(&input, &len, source);

		// check for EOF
		if (feof(source)) {
			break;
		}

		// count parallel commands
		int num_commands = count_char(input, '&') + 1;
		int *pids = malloc(num_commands * sizeof(int));

		// execute commands
		for (int i = 0; i < num_commands; i++) {
			char *command = strsep(&input, "&");
			pids[i] = execute_command(command, path);
		}

		// wait for all commands to finish
		for (int i = 0; i < num_commands; i++) {
			if (pids[i] != 0) {
				waitpid(pids[i], NULL, 0);
			}
		}
		// free memory
		free(pids);
	}

	exit(0);
}

// remove leading, trailing, and extra whitespace
void clean_whitespace(char *input) {
	int i = 0;
	int j = 0;
	// skip leading whitespace
	while (input[i] == ' ' || input[i] == '\t' || input[i] == '\n') {
		i++;
	}
	// iterate through string
	while (input[i] != '\0') {
		// replace tabs and newlines with spaces
		if (input[i] == '\t' || input[i] == '\n') {
			input[i] = ' ';
		}
		// skip extra spaces
		if (input[i] != ' ' || (i > 0 && input[i-1] != ' ')) {
			input[j] = input[i];
			j++;
		}
		i++;
	}
	// remove trailing whitespace
	while (j > 0 && input[j-1] == ' ') {
		j--;
	}
	input[j] = '\0';
}

// count the number of occurrences of a character in a string
int count_char(char *input, char c) {
	int count = 0;
	while (*input != '\0') {
		if (*input == c) {
			count++;
		}
		input++;
	}
	return count;
}

// execute a command with args
int execute_command(char *command, char **path) {
	char **args;
	int argc;
	char *command_name;
	FILE *output;
	char *output_file;
	char error_message[30] = "An error has occurred\n";

	// check for redirection
	output_file = command;
	command = strsep(&output_file, ">");

	// open output file
	if (output_file != NULL) {
		// clean whitespace
		clean_whitespace(output_file);
		// check for invalid characters
		if (strchr(output_file, '>') != NULL || strchr(output_file, ' ') != NULL){
			write(STDERR_FILENO, error_message, strlen(error_message));
			return 0;
		}
		output = fopen(output_file, "w");
		// check for invalid file
		if (output == NULL) {
			write(STDERR_FILENO, error_message, strlen(error_message));
			return 0;
		}
	} else {
		output = stdout;
	}

	// clean whitespace
	clean_whitespace(command);
	if (command[0] == '\0') {
		if (output != stdout) {
			fclose(output);
			write(STDERR_FILENO, error_message, strlen(error_message));
		}
		return 0;
	}

	// parse arguments
	argc = count_char(command, ' ') + 1;
	command_name = strsep(&command, " ");

	args = malloc((argc + 1) * sizeof(char *));
	args[0] = command_name;

	for (int i = 1; i < argc; i++) {
		args[i] = strsep(&command, " ");
	}
	args[argc] = NULL;


	// check for built-in commands
	int flag = -1;
	if (strcmp(command_name, "exit") == 0) {
		// exit
		flag = 0;
		if (argc != 1) {
			flag = 1;
		} else {
			exit(0);
		}
	} else if (strcmp(command_name, "cd") == 0) {
		// cd
		flag = 0;
		if (argc != 2) {
			flag = 1;
		}
		if (chdir(args[1]) != 0) {
			flag = 1;
		}
	} else if (strcmp(command_name, "path") == 0) {
		// path
		flag = 0;
		path = realloc(path, (argc) * sizeof(char *));
		for (int i = 0; i < argc; i++) {
			path[i] = args[i + 1];
		}
	}

	// cleanup after built-in commands
	if (flag != -1) {
		if (flag == 1) {
			write(STDERR_FILENO, error_message, strlen(error_message));
		}
		free(args);
		if (output != stdout) {
			fclose(output);
		}
		return 0;
	}

	// execute command
	char *full_path;
	for (int i = 0; path[i] != NULL; i++) {
		// create full path
		full_path = malloc((strlen(path[i]) + strlen(command_name) + 2) * sizeof(char));
		strcpy(full_path, path[i]);
		strcat(full_path, "/");
		strcat(full_path, command_name);

		// check if file is executable
		if (access(full_path, X_OK) == 0) {
			// execute command
			int pid = fork();
			if (pid == 0) {
				if (output != stdout) {
					dup2(fileno(output), STDOUT_FILENO);
					dup2(fileno(output), STDERR_FILENO);
				}
				execv(full_path, args);
				write(STDERR_FILENO, error_message, strlen(error_message));
				exit(1);
			} else {
				// free memory
				free(full_path);
				free(args);
				if (output != stdout) {
					fclose(output);
				}
				return pid;
			}
		}
		free(full_path);
	}

	// command not found
	write(STDERR_FILENO, error_message, strlen(error_message));
	free(args);
	if (output != stdout) {
		fclose(output);
	}
	return 0;
}