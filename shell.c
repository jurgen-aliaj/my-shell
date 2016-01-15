#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcheck.h>

#include "parser.h"
#include "shell.h"

/**
 * Program that simulates a simple shell.
 * The shell covers basic commands, including builtin commands 
 * (cd and exit only), standard I/O redirection and piping (|). 
 */

#define MAX_DIRNAME 100
#define MAX_COMMAND 1024
#define MAX_TOKEN 128

/* Functions to implement, see below after main */
int execute_cd(char** words);
int execute_nonbuiltin(simple_command *s);
int execute_simple_command(simple_command *cmd);
int execute_complex_command(command *cmd);


int main(int argc, char** argv) {
	
	char cwd[MAX_DIRNAME];           /* Current working directory */
	char command_line[MAX_COMMAND];  /* The command */
	char *tokens[MAX_TOKEN];         /* Command tokens (program name, 
					  * parameters, pipe, etc.) */

	while (1) {

		/* Display prompt */		
		getcwd(cwd, MAX_DIRNAME-1);
		printf("%s> ", cwd);
		
		/* Read the command line */
		fgets(command_line, MAX_COMMAND, stdin);
		/* Strip the new line character */
		if (command_line[strlen(command_line) - 1] == '\n') {
			command_line[strlen(command_line) - 1] = '\0';
		}
		
		/* Parse the command into tokens */
		parse_line(command_line, tokens);

		/* Check for empty command */
		if (!(*tokens)) {
			continue;
		}
		
		/* Construct chain of commands, if multiple commands */
		command *cmd = construct_command(tokens);
		//print_command(cmd, 0);
    
		int exitcode = 0;
		if (cmd->scmd) {
			exitcode = execute_simple_command(cmd->scmd);
			if (exitcode == -1) {
				break;
			}
		}
		else {
			exitcode = execute_complex_command(cmd);
			if (exitcode == -1) {
				break;
			}
		}
		release_command(cmd);
	}
    
	return 0;
}


/**
 * Changes directory to a path specified in the words argument;
 * For example: words[0] = "cd"
 *              words[1] = "csc209/assignment3/"
 */
int execute_cd(char** words) {
	
	/** 
	 * The first word contains the "cd" string, the second one contains 
	 * the path.
	 * Check possible errors:
	 * - The words pointer could be NULL, the first string or the second 
	 *   string could be NULL, or the first string is not a cd command
	 * - If so, return an EXIT_FAILURE status to indicate something is 
	 *   wrong.
	 */

	 /* Check if words or first/second word is NULL, also make sure first word is cd */
	 if (!words || !words[0] || !words[1] || strcmp(words[0], "cd"))
	 	return EXIT_FAILURE;

	/**
	 * First determine if the path is relative 
	 * or absolute (see is_relative function provided).
	 * - If it's not relative, then simply change the directory to the path 
	 * specified in the second word in the array.
	 * - If it's relative, then make sure to get the current working 
	 * directory, append the path in the second word to the current working
	 * directory and change the directory to this path.
	 * Hints: see chdir and getcwd man pages.
	 * Return the success/error code obtained when changing the directory.
	 */
	 if (!is_relative(words[1])) {
	 	if (chdir(words[1]) == -1) /* Just change directory to the absolute path */
	 		return EXIT_FAILURE;
	 }
	 else {
	 	char dir[MAX_DIRNAME];
	 	if (!getcwd(dir, MAX_DIRNAME-2)) /* Take the current directory */
	 		return EXIT_FAILURE;
	 	strncat(dir, "/", 1); 
	 	/* Concatenate the relative path to get an absolute path */
	 	strncat(dir, words[1], MAX_DIRNAME-strlen(dir)-1);
	 	if (chdir(dir) == -1) /* Finally just change directories like usual */
	 		return EXIT_FAILURE;
	 }
	 return EXIT_SUCCESS;
}


/**
 * Executes a program, based on the tokens provided as 
 * an argument.
 * For example, "ls -l" is represented in the tokens array by 
 * 2 strings "ls" and "-l", followed by a NULL token.
 * The command "ls -l | wc -l" will contain 5 tokens, 
 * followed by a NULL token. 
 */
int execute_command(char **tokens) {
	
	/**
	 * Execute command referred to by tokens.
	 * The first token is the command name, the rest are the arguments 
	 * for the command. 
	 * Function returns only in case of a failure (EXIT_FAILURE).
	 */
	if (execvp(tokens[0], tokens) == -1) {
		perror(tokens[0]);
		exit(1);
		/* Note: I choose to exit here because execute_command 
		 * will only be called on a child process for the purpose of 
		 * executing a program. If this program fails, there is no 
		 * reason to keep the process around any longer, just kill it.
		 * The parent process (the main shell) will continue to run,
		 * which is precisely the behaviour we want.
		 */
	}
	return EXIT_FAILURE; /* This will never happen, I've included it to suppress warnings */
}


/**
 * Executes a non-builtin command.
 */
int execute_nonbuiltin(simple_command *s) {
	/**
	 * Check if the in, out, and err fields are set (not NULL),
	 * and, IN EACH CASE:
	 * - Open a new file descriptor (make sure you have the correct flags,
	 *   and permissions);
	 * - redirect stdin/stdout/stderr to the corresponding file.
	 *   (hint: see dup2 man pages).
	 * - close the newly opened file descriptor in the parent as well. 
	 *   (Avoid leaving the file descriptor open across an exec!) 
	 * - finally, execute the command using the tokens (see execute_command
	 *   function above).
	 * This function returns only if the execution of the program fails.
	 */

	if (s->in) {
		/* Open the file in read only mode, without changing permissions.
		 * If we can't find the file or if reading from it fails, then
		 * there is nothing to do, so print an error and exit.
		 */
	 	int in = open(s->in, O_RDONLY);
	 	if (in == -1) {
	 		perror(s->in);
	 		exit(1);
	 	}
	 	/* Redirect stdin to read from the file we just opened.
	 	 * If this fails, then there is not much we can do.
	 	 * So print an error and exit.
	 	 */
		if (dup2(in, fileno(stdin)) == -1) {
			perror("dup2");
			exit(1);
		}
		/* Close the newly opened file descriptor, printing an error
		 * and exiting on failure.
		 */
		if (close(in) == -1) {
			perror("close");
			exit(1);
		}
	}
	if (s->out) {
		/* Here we open the file descriptor in read/write mode, truncating
		 * the contents of the file (if there are any) or creating a new
		 * file if said file does not exist. We set the permissions to 0664.
		 * That is, set read/write permissions for owner and group, but only
		 * read permissions for everyone else. Print an error and exit on
		 * failure.
		 */
		int out = open(s->out, O_CREAT | O_RDWR | O_TRUNC, 0664);
		if (out == -1) {
			perror(s->out);
			exit(1);
		}
		/* As explained above, we redirect stout, printing an error and 
		 * exiting on failure.
		 */
		if (dup2(out, fileno(stdout)) == -1) {
			perror("dup2");
			exit(1);
		}
		/* Close the newly opened file descriptor, printing an error
		 * and exiting on failure.
		 */
		if (close(out) == -1) {
			perror("close");
			exit(1);
		}
	}
	if (s->err) {
		/* Open the file descriptor as in the case for redirecting stdout.
		 * Print an error and exit on failure.
		 */
		int err = open(s->in, O_CREAT | O_RDWR | O_TRUNC, 0664);
		if (err == -1) {
			perror(s->err);
			exit(1);
		}
		/* Redirect stderr, printing an error and exiting
		 * on failure.
		 */
		if (dup2(err, fileno(stderr)) == -1) {
			perror("dup2");
			exit(1);
		}
		/* Close the newly opened file descriptor, printing an error
		 * and exiting on failure.
		 */
		if (close(err) == -1) {
			perror("close");
			exit(1);
		}
	}
	return execute_command(s->tokens); // This should never return.
}


/**
 * Executes a simple command (no pipes).
 */
int execute_simple_command(simple_command *cmd) {

	/**
	 * Check if the command is builtin.
	 * 1. If it is, then handle BUILTIN_CD (see execute_cd function provided) 
	 *    and BUILTIN_EXIT (simply exit with an appropriate exit status).
	 * 2. If it isn't, then you must execute the non-builtin command. 
	 * - Fork a process to execute the nonbuiltin command 
	 *   (see execute_nonbuiltin function above).
	 * - The parent should wait for the child.
	 *   (see wait man pages).
	 */
	if (cmd->builtin == BUILTIN_EXIT)
		return -1;
		/* I choose to return -1 here instead of doing exit(0) as
		 * it produces the same results. It also makes more sense
		 * to exit through main, since we are searching for special
		 * values (namely -1) in our main loop. Checking for these
		 * values would be pointless if they are never returned
		 * from execute_simple_command or execute_complex_command.
		 */
	else if (cmd->builtin == BUILTIN_CD) {
		/* Change directories and check for errors.
		 * If an error occurred, this means the given
		 * path was invalid, so print an error and continue
		 * the main loop by returning 0.
		 */
		if (execute_cd(cmd->tokens) == EXIT_FAILURE) {
			if (!cmd->tokens[1])
				fprintf(stderr, "Path expected after cd\n");
			else
				perror(cmd->tokens[1]);
		}
		return 0;
	}

	/* If the command is not builtin, then start a new process
	 * and call execute_nonbuiltin within this new process.
	 * If an error occurs, return to the main loop.
	 */
	int pid = fork();

	if (pid == -1)
		perror("fork");
	else if (pid == 0)
		execute_nonbuiltin(cmd);
	else {
		int status;
		/* If wait fails, continue the main loop */
		if (wait(&status) == -1)
			perror("wait");
	}
	return 0;
}


/**
 * Executes a complex command.  A complex command is two commands chained 
 * together with a pipe operator.
 */
int execute_complex_command(command *c) {

	/**
	 * Check if this is a simple command, using the scmd field.
	 * Remember that this will be called recursively, so when you encounter
	 * a simple command you should act accordingly.
	 * Execute nonbuiltin commands only. If it's exit or cd, you should not 
	 * execute these in a piped context, so simply ignore builtin commands. 
	 */
	if (c->scmd)
		execute_nonbuiltin(c->scmd);

	/** 
	 * Optional: if you wish to handle more than just the 
	 * pipe operator '|' (the '&&', ';' etc. operators), then 
	 * you can add more options here. 
	 */
	if (!strcmp(c->oper, "|")) {
		/**
		 * TODO: Create a pipe "pfd" that generates a pair of file 
		 * descriptors, to be used for communication between the 
		 * parent and the child. Make sure to check any errors in 
		 * creating the pipe.
		 */
		int pfd[2];
		if (pipe(pfd) == -1) {
			perror("pipe");
			exit(1); // There is little we can do if pipe fails.
					 // So just exit.
		}

		/**
		 * In the child:
		 *  - close one end of the pipe pfd and close the stdout 
		 * file descriptor.
		 *  - connect the stdout to the other end of the pipe (the 
		 * one you didn't close).
		 *  - execute complex command cmd1 recursively. 
		 * In the parent: 
		 *  - fork a new process to execute cmd2 recursively.
		 *  - In child 2:
		 *     - close one end of the pipe pfd (the other one than 
		 *       the first child), and close the standard input file 
		 *       descriptor.
		 *     - connect the stdin to the other end of the pipe (the 
		 *       one you didn't close).
		 *     - execute complex command cmd2 recursively. 
		 *  - In the parent:
		 *     - close both ends of the pipe. 
		 *     - wait for both children to finish.
		 */
		int pid1 = fork();

		if (pid1 == -1) {
			perror("fork");
			exit(1);
		}
		else if (pid1 == 0) {
			/* Close the reading end of the pipe,
			 * printing an error and exiting on failure.
			 */
			if (close(pfd[0]) == -1) {
				perror("close");
				exit(1);
			}
			/* Redirect stdout to the writing end of the pipe, 
			 * printing an error and exiting on failure.
			 */
			if (dup2(pfd[1], fileno(stdout)) == -1) {
				perror("dup2");
				exit(1);
			}
			/* Close the other end of the pipe,
			 * printing an error and exiting on failure.
			 */
			if (close(pfd[1]) == -1) {
				perror("close");
				exit(1);
			}
			execute_complex_command(c->cmd1);
			exit(0); // We must destroy the child process
					 // after it executes the command, since
					 // this was its only purpose.
		}
		else {
			int pid2 = fork();

			if (pid2 == -1) {
				perror("fork");
				exit(1);
			}
			else if (pid2 == 0) {
				/* Close the writing end of the pipe, 
				 * printing an error and exiting on failure.
				 */
				if (close(pfd[1]) == -1) {
					perror("close");
					exit(1);
				}
				/* Redirect stdin to the reading end of the pipe,
				 * printing an error and exiting on failure.
				 */
				if (dup2(pfd[0], fileno(stdin)) == -1) {
					perror("dup2");
					exit(1);
				}
				/* Close the other end of the pipe,
				 * printing an error and exiting on failure.
				 */
				if (close(pfd[0]) == -1) {
					perror("close");
					exit(1);
				}
				execute_complex_command(c->cmd2);
				exit(0); // Again, destroy the child process.
			}
			else {
				/* Close both ends of the pipe, printing
				 * an error and exiting on failure.
				 */
				if (close(pfd[0]) == -1) {
					perror("close");
					exit(1);
				}
				if (close(pfd[1]) == -1) {
					perror("close");
					exit(1);
				}
				int status1, status2;
				/* Wait for both of the child processes. It is not 
				 * necessary to know if the children exited abnormally, 
				 * or what their exit statuses were. So there is no
				 * need to do anything with status1 and status2. We
				 * just need to know that the children exited.
				 */
				if (waitpid(pid1, &status1, 0) == -1) {
					perror("waitpid");
					exit(1);
				}
				if (waitpid(pid2, &status2, 0) == -1) {
					perror("waitpid");
					exit(1);
				}
			}
		}
	}
	return 0;
}
