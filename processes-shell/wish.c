#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_SUPPORTED_LENGTH (64)

char const error_message[30] = "An error has occurred\n";
void parse_and_execute(char *buffer, char **paths);
char* trim(char *string);
int is_space(char c);
size_t n_paths = 1;

int main(int argc, char const *argv[]) {
    char *buffer = NULL;
    int nread;
    size_t n = 0;
    char *paths[10] = {NULL};
    paths[0] = malloc(MAX_SUPPORTED_LENGTH * sizeof(char));
    strcpy(paths[0], "/bin/");
    
    if (argc == 1) {
        while (1) {
            printf("wish> ");
            nread = getline(&buffer, &n, stdin);
            if (nread == -1) {
                free(buffer);
                exit(0);
            } 
            parse_and_execute(buffer, paths);
        }
    }

    else if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(buffer);
            exit(1);
        }

        while ((nread = getline(&buffer, &n, fp)) != -1) {
            parse_and_execute(buffer, paths);
        }

        free(buffer);
        fclose(fp);
        exit(0);  
    }
    
    else {
        write(STDERR_FILENO, error_message, strlen(error_message));
        free(buffer);
        exit(1);
    }
}

void parse_and_execute(char *buffer, char **paths) {
    trim(buffer);
    size_t len = strlen(buffer);
    if (len == 0) return;

    int parallel_flag = buffer[len - 1] == '&';

    char *commands[MAX_SUPPORTED_LENGTH];
    char *inputs[MAX_SUPPORTED_LENGTH];
    char *outputs[MAX_SUPPORTED_LENGTH];
    char *args[MAX_SUPPORTED_LENGTH][MAX_SUPPORTED_LENGTH];
    int argcs[MAX_SUPPORTED_LENGTH];
    size_t commandc = 0;

    // Parse each command and initialize arrays
    while (buffer) {
        argcs[commandc] = 0;
        outputs[commandc] = malloc(MAX_SUPPORTED_LENGTH * sizeof(char));
        strcpy(outputs[commandc], "no outputs");
        commands[commandc] = strsep(&buffer, "&");
        commands[commandc] = trim(commands[commandc]);
        commandc++;
    }
    commandc -= parallel_flag;

    // Split commands into input and output
    for (size_t i = 0; i < commandc; i++) {
        inputs[i] = strsep(&commands[i], ">");
        inputs[i] = trim(inputs[i]);
        // Guard against starting with >
        if (strlen(inputs[i]) == 0 && commands[i]) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
        // Treat cases with >
        if (commands[i]) {
            commands[i] = trim(commands[i]);
            // Guard against no output
            if (strlen(commands[i]) == 0) {
                outputs[i] = "this is an error";
            } else {
                strcpy(outputs[i], commands[i]);
                strsep(&commands[i], " \n\t");
                // Guard against multiple outputs
                if (commands[i]) outputs[i] = "this is an error";
            }
        }
    }
    
    // Parse inputs of each command into arguments
    for (size_t i = 0; i < commandc; i++) {
        while (inputs[i])
            args[i][argcs[i]++] = strsep(&inputs[i], " \n\t");
        args[i][argcs[i]] = NULL; // End of argv sentinel
    }

    // Focus on built-in commands
    if (commandc == 1) {
        if (argcs[0] == 0) return;
        // exit command
        else if (strcmp(args[0][0], "exit") == 0) {
            if (argcs[0] == 1) {
                free(buffer);
                pid_t wpid;
                int status = 0;
                while ((wpid = wait(&status)) > 0);
                exit(0);
            }
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }

        // cd command
        else if (strcmp(args[0][0], "cd") == 0) {
            if (argcs[0] == 2) chdir(args[0][1]);
            else write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }

        // path command
        else if (strcmp(args[0][0], "path") == 0) {
            n_paths = argcs[0] - 1;
            for (int i = 0; i < n_paths; i++) {
                if (paths[i] != NULL)
                    free(paths[i]);
                char *temp_path = args[0][i + 1];
                size_t path_len = strlen(temp_path) + 2;
                char *path = malloc(path_len * sizeof(char));
                strcpy(path, temp_path);
                if (path[strlen(path) - 1] != '/'){
                    strcat(path, "/");
                }
                paths[i] = path;
            } return;
        }
    }

    // Execute binaries
    for (size_t i = 0; i < commandc; i++) {
        if (strcmp(args[i][0], "") == 0) continue;

        // Possibly redirect
        FILE *fp = NULL;
        if (strcmp(outputs[i], "no outputs")) {
            if (strcmp(outputs[i], "this is an error") == 0) {
                // Quit early
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            fp = fopen(outputs[i], "w");
            if (fp == NULL) write(STDERR_FILENO, error_message, strlen(error_message));
        }
        
        // Search for binary in all paths
        int bin_not_found = 1;        
        for (size_t j = 0; j < n_paths; j++) {
            char path[strlen(paths[j]) + strlen(args[i][0])];
            path[0] = '\0';
        
            strcat(path, paths[j]);
            strcat(path, args[i][0]);

            // Found binary
            if (access(path, X_OK) == 0) {
                // Redirect output
                int stdout_copy;
                if (fp != NULL) {
                    stdout_copy = dup(STDOUT_FILENO);
                    dup2(fileno(fp), STDOUT_FILENO);
                }

                // Check if should wait for child execution
                if (i == commandc - 1 && !parallel_flag) {
                    int rc = fork();
                    if (rc == 0) {
                        execv(path, args[i]);
                        printf(" Deu ruim. ");
                    } else {
                        pid_t wpid;
                        int status = 0;
                        while ((wpid = wait(&status)) > 0);
                        // Fix redirection output
                        if (fp != NULL) {
                            dup2(stdout_copy, STDOUT_FILENO);
                            close(stdout_copy);
                            fclose(fp);
                        }
                        bin_not_found = 0;
                        break;
                    }
                } else {
                    int rc = fork();
                    if (rc == 0) {
                        execv(path, args[i]);
                        printf(" Deu ruim. ");
                    } else {
                        // Fix redirection output
                        if (fp != NULL) {
                            dup2(stdout_copy, STDOUT_FILENO);
                            close(stdout_copy);
                            fclose(fp);
                        }
                        bin_not_found = 0;
                        break;
                    }
                }
            } 
        } if (bin_not_found) write(STDERR_FILENO, error_message, strlen(error_message));
    }
}

char* trim(char *string) {
    while ((char)is_space(*string)) string++;
    if ((char)*string == '\0') return string;

    char *end = string + strlen(string) - 1;
    while (end > string && (char)is_space(*end)) end--;
    end[1] = '\0';
    return string;
}

int is_space(char c) {return c == ' ' || c == '\t' || c == '\n';}
