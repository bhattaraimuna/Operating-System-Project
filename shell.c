#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define BUFFER 1024
#define DELIMITERS " \t\r\n\a"
#define history 15



void exit_command(char **tokens)
{
	if (strcmp(tokens[0], "exit") == 0 || strcmp(tokens[0], "quit") == 0)
		exit(0);
}

void test_blank(char **tokens)
{
	if (strcmp(tokens[0], "\n") == 0)
       {
         printf("Muna_shell$");
       }
}

void test_path(char **tokens)
{
	int i;
	int status;
	char *pathname[10];
	char *pathvariable = getenv("PATH");

	pathname[0] = strtok(pathvariable, ":");
	i = 1;
	while (pathname[i - 1] != NULL)
	{
		pathname[i] = strtok(NULL, ":");
		i++;
	}

	for (i = 0; i < 10; i++)
	{
		if (pathname[i] == NULL)
			break;

		char cmd_path[100];
		strcpy(cmd_path, pathname[i]);
		strcat(cmd_path, "/");
		strcat(cmd_path, tokens[0]);
		status = access(cmd_path, X_OK);

		if (status == 0)
		{
			tokens[0] = cmd_path;
			break;
		}
	}
}

void test_cd(char **tokens)
{

	if (strcmp(tokens[0], "cd") == 0) 
    {
        if (tokens[1] == NULL) 
        {
            fprintf(stderr, "Muna_shell: error");
        } 
        else 
        {
            if (chdir(tokens[1]) != 0) 
            {
                perror("Muna_shell");
            }
        }
    }
}

void test_history(char **tokens)
{
	if (strcmp(tokens[0], "history") == 0)
    {
        int i;
        int j = 0;
        for (i = 0; i < 15; i++) 
        {
            if (history) 
            {
                printf("%d %s\n", j, history);
                j++;
            }
        }
    }
}

void test_history_pid(char **tokens)
{
	pid_t pid[15];
	if (strcmp(tokens[0], "history") == 0 && strcmp(tokens[1], "-p") == 0)
    {
        int i;
        int j = 0;
        for (i = 0; i < 15; i++) 
        {
            if (history) 
            {
                printf("%d %s %d\n", j, history, pid[i]);
                j++;
            }
        }
    }
}


void print_user()
{
	printf("Muna_Shell$ ");
} 

char *read_user()
{
	char *input = NULL;
	size_t input_size = 0;
	getline(&input, &input_size, stdin);
	return input;
}

char **evaluate_input(char *input)
{
	int buffer_size = BUFFER;
	int position = 0;
	char **tokens = malloc (buffer_size * sizeof(char*));
	char* token;
	if (!tokens)
	{
		fprintf(stderr, "Error in allocating\n");
		exit(EXIT_FAILURE);
	}

	token = strtok (input, DELIMITERS);
	while (token)
	{
		tokens[position] = token;
		position++;
		if (position >= buffer_size)
		{
			buffer_size += BUFFER;g
			tokens = realloc( tokens, buffer_size * sizeof(char*));
			if(!tokens)
			{
				fprintf(stderr,"Error in allocating\n");
				free(tokens);
				exit(EXIT_FAILURE);
			}
		}
	}
		tokens[position] = NULL;
		return tokens;	
}

int execute_input(char **tokens)
{
	exit_command(tokens);
	test_blank(tokens);
	test_path(tokens);
	test_cd(tokens);
	test_history(tokens);
	test_history_pid(tokens);

	pid_t pid = fork();
	pid_t wpid;
	int status;
	
	if (pid == 0)
	{
		if (execvp(tokens[0], tokens) == -1)
		{
			perror("Shell");
		}
		exit(EXIT_FAILURE);
		return 0;
	}
	if (pid <0)
	{
		perror("Shell");
		return 0;
	}
	else{
	do 
	{
		wpid = waitpid(pid, &status, WUNTRACED);
	}while (!WIFEXITED(status)&& !WIFSIGNALED(status));
	}
	return 1;
}

int main(int argc, char **argv)
{
	char *input;
	char **tokens;
	int status;
	
	do
	{
		print_user();
		input = read_user();
		tokens = evaluate_input(input);
		status = execute_input(tokens);
		
		free(input);
		free(tokens);
	}while (status);
	
	return 0;
}
