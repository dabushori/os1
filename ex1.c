// Ori Dabush 212945760
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// some defines for constants, signs and command names
#define MAX_COMMAND_LEN 100
#define COMMANDS_LIMIT 100
#define MAX_PATH_LEN 100

#define PROMPT "$ "
#define SPACE " "
#define ECHO_DELIM "\""

#define JOBS_COMMAND "jobs"
#define HISTORY_COMMAND "history"
#define CD_COMMAND "cd"
#define EXIT_COMMAND "exit"
#define ECHO_COMMAND "echo"

#define BACKGROUND "&"

#define HOME_ENV "HOME"

#define SLASH "/"
#define BACK_TOKEN ".."
#define HOME_TOKEN "~"
#define LAST_TOKEN "-"

#define EXEC_ERROR_MESSAGE "exec failed\n"
#define FORK_ERROR_MESSAGE "fork failed\n"
#define CHDIR_ERROR_MESSAGE "chdir failed\n"
#define TOO_MANY_ARGUMENTS "Too many arguments\n"
#define DEFAULT_ERROR_MESSAGE "An error occurred\n"

// enum for the status of a command
typedef enum Status
{
    NOT_STARTED,
    RUNNING,
    DONE,
} Status;

// a struct for a command
typedef struct Command
{
    char command[MAX_COMMAND_LEN];
    Status status;
    pid_t pid;
    char isBuiltIn;
} Command;

// a macro to turn a status (int) into a string
#define STATUS_TO_STRING(x) x == RUNNING ? "RUNNING" : "DONE"

// a function that check which commands (=processes) have ended and updates their status
void updateCommands(Command commands[COMMANDS_LIMIT], int len)
{
    int status, i;
    for (i = 0; i < len; ++i)
    {
        if (!commands[i].isBuiltIn)
        {
            waitpid(commands[i].pid, &status, WNOHANG);
            if (WIFEXITED(status))
            {
                commands[i].status = DONE;
            }
        }
    }
}

// parsing section

// a function to remove spaces from the string
// (if there are multiple spaces it turns them into one,
// and it deletes the space in the begining and in the end of the string)
void removeSpacesCommand(char *str)
{
    char command[MAX_COMMAND_LEN] = {0};
    char *token, *str_origin = str;

    token = strtok(str, SPACE);

    while (token != NULL)
    {
        strcat(command, token);
        strcat(command, SPACE);
        token = strtok(NULL, SPACE);
    }
    command[strlen(command) - 1] = '\0';
    strcpy(str_origin, command);
}

// a function to remove spaces from the string but without deleting spaces that are between quotation marks
void removeSpacesEcho(char *str)
{
    int isStr = 0, ignoreSpaces = 1;
    char *curr = str;
    while (*curr != '\0')
    {
        if (!isStr)
        {
            if (*curr == ' ')
            {
                if (!ignoreSpaces)
                {
                    *str++ = *curr;
                    ignoreSpaces = 1;
                }
                ++curr;
            }
            else if (*curr == '\"')
            {
                isStr = 1 - isStr;
                ++curr;
                ignoreSpaces = 0;
            }
            else
            {
                *str++ = *curr++;
                ignoreSpaces = 0;
            }
        }
        else
        {
            if (*curr == '\"')
            {
                isStr = 1 - isStr;
                ++curr;
            }
            else
            {
                *str++ = *curr++;
            }
        }
    }
    *str = '\0';
}

// a function to split a string to tokens, using a given delimeter
// the function returns the number of tokens that it found
int split(char *str, char *tokens[MAX_COMMAND_LEN], char delim[])
{
    int index = 0;
    tokens[index] = strtok(str, delim);
    while (tokens[index] != NULL)
    {
        tokens[++index] = strtok(NULL, delim);
    }
    return index;
}

// built in functions section

// a function that implements the jobs command (we can assume that tokens = {"jobs", NULL, NULL, ...})
void jobs(char *tokens[MAX_COMMAND_LEN], Command commands[COMMANDS_LIMIT], int len)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        if (!commands[i].isBuiltIn && commands[i].status == RUNNING)
        {
            printf("%s\n", commands[i].command);
        }
    }
}

// a function that implements the history command (we can assume that tokens = {"history", NULL, NULL, ...})
void history(char *tokens[MAX_COMMAND_LEN], Command commands[COMMANDS_LIMIT], int len)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        printf("%s %s\n", commands[i].command, STATUS_TO_STRING(commands[i].status));
    }
}

// a function that implements the cd command
// the lastPath argument is a pointer to a string that represent the last
// working directory that the program was before the current working directory
void cd(char *tokens[MAX_COMMAND_LEN], int len, char *lastPath)
{
    // when there are too many arguments
    if (len > 2)
    {
        printf(TOO_MANY_ARGUMENTS);
        fflush(stdout);
        return;
    }
    // get the HOME environment variable
    char *home = getenv(HOME_ENV);
    char currentPath[MAX_PATH_LEN];
    // get the current working directory
    if (!getcwd(currentPath, MAX_PATH_LEN))
    {
        printf(DEFAULT_ERROR_MESSAGE);
        fflush(stdout);
        return;
    }
    // if the command is without arguments
    if (len == 1)
    {
        // change the working directory to the HOME working directory
        if (chdir(home) == -1)
        {
            printf(CHDIR_ERROR_MESSAGE);
            fflush(stdout);
        }
        strcpy(lastPath, currentPath);
        return;
    }
    char *path = tokens[len - 1];
    char *dirs[MAX_PATH_LEN] = {};
    int isStartWithSlash = (path[0] == '/');
    // split the given path by '/' delimeter
    int numOfDirs = split(path, dirs, SLASH);

    // case of path that starts with a '/'
    if (numOfDirs == 0)
    {
        if (chdir(SLASH) == -1)
        {
            printf(CHDIR_ERROR_MESSAGE);
            fflush(stdout);
            chdir(currentPath);
            return;
        }
        strcpy(lastPath, currentPath);
        return;
    }
    if (isStartWithSlash)
    {
        dirs[0] = path;
    }

    // change the current working directory to the dir we need to go to
    int i;
    for (i = 0; i < numOfDirs; ++i)
    {
        // if the current directory we need to pass to is ~ we change the working directory to HOME
        if (!strcmp(dirs[i], HOME_TOKEN))
        {
            if (i != 0 || chdir(home) == -1)
            {
                printf(CHDIR_ERROR_MESSAGE);
                fflush(stdout);
                chdir(currentPath);
                return;
            }
        }
        // if the current directory we need to pass to is - we change the working
        // directory to the last working directory we have been in
        else if (!strcmp(dirs[i], LAST_TOKEN))
        {
            if (chdir(lastPath) == -1)
            {
                printf(CHDIR_ERROR_MESSAGE);
                fflush(stdout);
                chdir(currentPath);
                return;
            }
        }
        // change the working directory to the directory (it also takes care of the .. sign)
        else if (chdir(dirs[i]) == -1)
        {
            printf(CHDIR_ERROR_MESSAGE);
            fflush(stdout);
            chdir(currentPath);
            return;
        }
    }
    // change the value of lastPath to the working directory we were in the beggining of this function
    strcpy(lastPath, currentPath);
}

// excecution section

// a function that excecute the command in the commands[index] cell
// the variable lastPath is a variable that used only in the cd command
// (it is passsed like that so it won't be a global variable)
void excecute(Command commands[COMMANDS_LIMIT], int index, char *lastPath)
{
    // set the current command's status to RUNNING
    commands[index].status = RUNNING;

    char command[MAX_COMMAND_LEN];
    strcpy(command, commands[index].command);
    commands[index].isBuiltIn = 1;
    char *tokens[MAX_COMMAND_LEN] = {};
    int len;
    // remove the spaces in the command
    removeSpacesCommand(command);
    // if the command is echo, its arguments must be parsed differently
    if (!strncmp(command, ECHO_COMMAND, sizeof(ECHO_COMMAND) - 1))
    {
        // copy the original command (with the spaces) to the command str
        strcpy(command, commands[index].command);
        // seperating the "echo" and the arguments with a '\0' to use them as 2 strings
        command[sizeof(ECHO_COMMAND) - 1] = '\0';
        // the first token is "echo"
        tokens[0] = command;
        // remove the spaces in the arguments as echo arguments
        removeSpacesEcho(command + sizeof(ECHO_COMMAND));
        // the second token is the parsed argument
        tokens[1] = command + sizeof(ECHO_COMMAND);
        // there are two tokens
        len = 2;
        // if there were a '&' which means run in the background,
        // it is attached to the arguments now so we must delete it
        if (command[strlen(command) - 1] == '&')
        {
            int index = strlen(command) - 1;
            // seperate the argument from the '&' with a '\0' creating two different strings - argument and "&"
            command[index++] = '\0';
            command[index++] = '&';
            command[index] = '\0';
            // set the third token to be "&" and increase len by 1
            tokens[2] = command + index;
            ++len;
        }
    }
    else
    {
        // if the command is not echo, it is parsed normally
        strcpy(commands[index].command, command);
        len = split(command, tokens, SPACE);
    }

    // if the command is jobs
    if (!strcmp(tokens[0], JOBS_COMMAND))
    {
        // update all the commands status if needed
        updateCommands(commands, index);
        jobs(tokens, commands, index + 1);
        // set the status of the command to DONE
        commands[index].status = DONE;
        return;
    }
    // if the command is history
    else if (!strcmp(tokens[0], HISTORY_COMMAND))
    {
        // update all the commands status if needed
        updateCommands(commands, index);
        history(tokens, commands, index + 1);
        // set the status of the command to DONE
        commands[index].status = DONE;
        return;
    }
    // if the command is cd
    else if (!strcmp(tokens[0], CD_COMMAND))
    {
        cd(tokens, len, lastPath);
        // set the status of the command to DONE
        commands[index].status = DONE;
        return;
    }
    // if the command is exit
    else if (!strcmp(tokens[0], EXIT_COMMAND))
    {
        // set the status of the command to DONE
        commands[index].status = DONE;
        _exit(0);
    }
    // if the command is a non built in command
    commands[index].isBuiltIn = 0;
    int isBackground = 0;
    // check if the last token is "&", if it is - the command should run in the background
    if (!strcmp(tokens[len - 1], BACKGROUND))
    {
        // set the "&" token to NULL, decrease len by 1, set the isBackground flag on and remove the "&" from the command str
        tokens[len - 1] = NULL;
        --len;
        isBackground = 1;
        commands[index].command[strlen(commands[index].command) - 2] = '\0'; // remove the &
    }

    // create a child process for the command
    pid_t pid = fork();
    if (pid > 0)
    {
        // set the command's pid
        commands[index].pid = pid;
        // if the command shouldn't run in the background, wait for it to be over
        if (!isBackground)
        {
            waitpid(pid, NULL, 0);
            commands[index].status = DONE;
        }
    }
    else if (pid == 0)
    {
        // excecute the command (with tokens as arguments and include the PATH environment variables)
        execvp(tokens[0], tokens);
        // if the exec failed, the command won't work and an error message is printed
        printf(EXEC_ERROR_MESSAGE);
        fflush(stdout);
        // set the status of the command to DONE
        _exit(0);
    }
    else
    {
        // error in fork
        printf(FORK_ERROR_MESSAGE);
        fflush(stdout);
        // set the status of the command to DONE
        commands[index].status = DONE;
    }
}

int main()
{
    Command commands[COMMANDS_LIMIT] = {};
    char lastPath[MAX_PATH_LEN];
    // the index of the current command
    int curr = 0;
    do
    {
        // print the prompt
        fflush(stdout);
        printf(PROMPT);
        fflush(stdout);

        // get the input
        fgets(commands[curr].command, MAX_COMMAND_LEN, stdin);
        // remove the '\n'
        commands[curr].command[strlen(commands[curr].command) - 1] = '\0';

        // excecute the command passing the lastPath string to it
        excecute(commands, curr, lastPath);
        // increase the index of the command
        ++curr;

    } while (curr < COMMANDS_LIMIT); // limit the while loop by the array size, as we can assume
    // if we want it to run forever, the array should be dynamic
    return 0;
}