#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <stdbool.h>

#define HISTORY_COUNT 10
#define MAX_LINE 80

typedef struct {
    char username[50];
    char history[HISTORY_COUNT][MAX_LINE];
    int history_count;
} UserHistory;

typedef struct HashNode {
    UserHistory userHistory;      
    char historyCircular[HISTORY_COUNT][MAX_LINE]; 
    int startIndex;                
    int endIndex;                  
    struct HashNode *next;        
} HashNode;

typedef struct {
    char name[50];
    char command[80];
} Alias;

#define ALIAS_COUNT 100
#define USER_COUNT 10
#define HASH_TABLE_SIZE 100

HashNode *userHashTable[HASH_TABLE_SIZE] = {NULL};
UserHistory user_histories[USER_COUNT];
int user_count = 0;

Alias aliases[ALIAS_COUNT];
int alcount = 0;

char activeuser[50] = "activeuser";

void activRaw() {
    struct termios config;
    tcgetattr(STDIN_FILENO, &config);
    config.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &config);
}

void disRaw() {
    struct termios terminalConfig;
    tcgetattr(STDIN_FILENO, &terminalConfig);
    terminalConfig.c_lflag |= (ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSANOW, &terminalConfig);
}

void manageChild(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void configSigHandler() {
    struct sigaction action;
    action.sa_handler = manageChild;
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &action, NULL);
}

void initializeProfile() {
    FILE *filePointer = fopen(".profile_medsh", "r");

    if (!filePointer) {
        perror("Error: Could not open .profile_medsh");
        return;
    }

    char buffer[150];

    while (fgets(buffer, sizeof(buffer), filePointer)) {
        buffer[strcspn(buffer, "\n")] = '\0';

        char *aliasName = strtok(buffer, "=");
        char *aliasCommand = strtok(NULL, "");

        if (aliasName && aliasCommand && alcount < ALIAS_COUNT) {
            strncpy(aliases[alcount].name, aliasName, sizeof(aliases[alcount].name) - 1);
            aliases[alcount].name[sizeof(aliases[alcount].name) - 1] = '\0';

            strncpy(aliases[alcount].command, aliasCommand, sizeof(aliases[alcount].command) - 1);
            aliases[alcount].command[sizeof(aliases[alcount].command) - 1] = '\0';

            alcount++;
        }
    }
    fclose(filePointer);
}

void profileStore() {
    FILE *file = fopen(".profile_medsh", "w");
    file ? ({ 
        int i = 0;
        while (i < alcount) {
            fprintf(file, "%s=%s\n", aliases[i].name, aliases[i].command);
            i++;
        }
        fclose(file);
    }) : perror("Failed to open .profile_medsh for writing");
}

unsigned int hashFunction(const char *str) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash * 31) + *str++;
    }
    return hash % HASH_TABLE_SIZE;
}

UserHistory *getUserHistory(const char *username) {
    if (!username) {
        fprintf(stderr, "Error: Username cannot be NULL.\n");
        return NULL;
    }

    unsigned int index = hashFunction(username);
    HashNode *current = userHashTable[index];

    while (current) {
        if (strcmp(current->userHistory.username, username) == 0) {
            return &current->userHistory;
        }
        current = current->next;
    }

    if (user_count < USER_COUNT) {
        HashNode *newNode = (HashNode *)malloc(sizeof(HashNode));
        if (!newNode) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return NULL;
        }

        strncpy(newNode->userHistory.username, username, sizeof(newNode->userHistory.username) - 1);
        newNode->userHistory.username[sizeof(newNode->userHistory.username) - 1] = '\0';
        newNode->userHistory.history_count = 0;
        newNode->startIndex = 0;
        newNode->endIndex = 0;
        newNode->next = userHashTable[index];
        userHashTable[index] = newNode;

        user_count++;
        return &newNode->userHistory;
    }

    fprintf(stderr, "Error: Maximum user limit reached.\n");
    return NULL;
}

void appendHstry(UserHistory *userHistory, const char *cmd) {
    if (!userHistory || !cmd) {
        fprintf(stderr, "Error: Invalid user history or command.\n");
        return;
    }

    unsigned int index = hashFunction(userHistory->username);
    HashNode *current = userHashTable[index];

    while (current) {
        if (strcmp(current->userHistory.username, userHistory->username) == 0) {
            break;
        }
        current = current->next;
    }

    if (!current) {
        fprintf(stderr, "Error: User not found in hash table.\n");
        return;
    }

    snprintf(current->historyCircular[current->endIndex], MAX_LINE, "%s", cmd);
    current->endIndex = (current->endIndex + 1) % HISTORY_COUNT;

    if (current->userHistory.history_count < HISTORY_COUNT) {
        current->userHistory.history_count++;
    } else {
        current->startIndex = (current->startIndex + 1) % HISTORY_COUNT;
    }
}

int navigateHistory(UserHistory *user_history, char *inputBuffer) {
    activRaw();
    unsigned int index = hashFunction(activeuser);
    HashNode *current = userHashTable[index];

    while (current) {
        if (strcmp(current->userHistory.username, activeuser) == 0) {
            break;
        }
        current = current->next;
    }

    if (!current) {
        printf("No history found for user: %s\n", activeuser);
        return -1;
    }

    int currentIndex = (current->endIndex - 1 + HISTORY_COUNT) % HISTORY_COUNT;
    int browsing = 1;

    printf("\033[2K\r%s@medsh> ", activeuser);
    fflush(stdout);

    while (browsing) {
        int ch = getchar();

        if (ch == 27) {
            if (getchar() == '[') {
                ch = getchar();

                if (ch == 'A') { 
                    if (currentIndex != current->startIndex) {
                        currentIndex = (currentIndex - 1 + HISTORY_COUNT) % HISTORY_COUNT;
                    }
                    printf("\033[2K\r%s@medsh> %s", activeuser, current->historyCircular[currentIndex]);
                    strncpy(inputBuffer, current->historyCircular[currentIndex], MAX_LINE);
                    fflush(stdout);
                } else if (ch == 'B') { 
                    if ((currentIndex + 1) % HISTORY_COUNT != current->endIndex) {
                        currentIndex = (currentIndex + 1) % HISTORY_COUNT;
                    }
                    printf("\033[2K\r%s@medsh> %s", activeuser, current->historyCircular[currentIndex]);
                    strncpy(inputBuffer, current->historyCircular[currentIndex], MAX_LINE);
                    fflush(stdout);
                }
            }
        } else if (ch == 127) {
            if (strlen(inputBuffer) > 0) {
                inputBuffer[strlen(inputBuffer) - 1] = '\0';
                printf("\033[2K\r%s@medsh> %s", activeuser, inputBuffer);
                fflush(stdout);
            }
        } else if (ch == '\n') {
            browsing = 0;
        } else { 
            size_t len = strlen(inputBuffer);
            inputBuffer[len] = (char)ch;
            inputBuffer[len + 1] = '\0';
            printf("%c", ch);
            fflush(stdout);
        }
    }

    printf("\n");
    disRaw();
    return currentIndex;
}


int main() {
    configSigHandler();
    initializeProfile();

    char input[MAX_LINE];

    while (1) {
        UserHistory *user_history = getUserHistory(activeuser); 

        printf("%s@medsh> ", activeuser);
        fflush(stdout);

        memset(input, 0, sizeof(input));
        navigateHistory(user_history, input);

        if (strlen(input) == 0) {
            continue;
        }

        if (strcmp(input, "exit") == 0) {
            profileStore();
            break;
        }

        if (strcmp(input, "history") == 0) {
            unsigned int index = hashFunction(activeuser);
            HashNode *current = userHashTable[index];

            while (current) {
                if (strcmp(current->userHistory.username, activeuser) == 0) {
                    break;
                }
                current = current->next;
            }

            if (!current) {
                printf("No history available for user: %s\n", activeuser);
                continue;
            }

            int i = current->startIndex;
            int count = 0;
            while (count < current->userHistory.history_count) {
                printf("%d: %s\n", count + 1, current->historyCircular[i]);
                i = (i + 1) % HISTORY_COUNT;
                count++;
            }

       
            int selectedIndex = navigateHistory(user_history, input);
            if (selectedIndex != -1) {
                printf("Executing: %s\n", input);
                fflush(stdout);

                char *args[MAX_LINE / 2 + 1];
                int i = 0;
                char *token = strtok(input, " ");
                while (token != NULL) {
                    args[i++] = token;
                    token = strtok(NULL, " ");
                }
                args[i] = NULL;

                pid_t pid = fork();
                if (pid == 0) {
                    execvp(args[0], args);
                    perror("execvp failed");
                    exit(1);
                } else if (pid > 0) {
                    wait(NULL);
                } else {
                    perror("fork failed");
                }
            }

            continue;
        }

        if (strcmp(input, "!!") != 0 && input[0] != '!' && strcmp(input, "history") != 0) {
            appendHstry(user_history, input);
        }

        if (strncmp(input, "set user", 8) == 0) {
            if (sscanf(input + 9, "%s", activeuser) != 1 || strlen(activeuser) == 0) {
                printf("Username not recognized. Correct usage: set user <username>\n");
            } else {
                printf("Successfully set: %s\n", activeuser);
            }
            continue;
        }

        if (strncmp(input, "alias definition ", 17) == 0) {
            char *name = strtok(input + 17, ":");
            char *command = strtok(NULL, "\"");

            if (name && command && strlen(name) > 0 && strlen(command) > 0) {
                if (alcount < ALIAS_COUNT) {
                    strncpy(aliases[alcount].name, name, sizeof(aliases[alcount].name) - 1);
                    strncpy(aliases[alcount].command, command, sizeof(aliases[alcount].command) - 1);
                    alcount++;
                    printf("Alias added: %s -> %s\n", name, command);
                } else {
                    printf("Alias limit reached. Cannot add more aliases.\n");
                }
            } else {
                printf("Invalid alias format. Use: alias definition <name>:\"<command>\"\n");
            }
            continue;
        }

        for (int i = 0; i < alcount; i++) {
            if (strcmp(input, aliases[i].name) == 0) {
                strcpy(input, aliases[i].command);
                break;
            }
        }

        int background = 0;
        if (input[strlen(input) - 1] == '&') {
            background = 1;
            input[strlen(input) - 1] = '\0';
        }

        char *args[MAX_LINE / 2 + 1];
        int i = 0;
        char *token = strtok(input, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);
        } else if (pid > 0) {
            if (!background) {
                wait(NULL);
            }
        } else {
            perror("fork failed");
        }
    }

    return 0;
}