#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

//limits
#define MAX_TOKENS 100
#define MAX_STRING_LEN 100
#define MAX_PID_NUM 100

size_t MAX_LINE_LEN = 10000;


// builtin commands
#define EXIT_STR "exit"
#define EXIT_CMD 0
#define UNKNOWN_CMD 99
#define ERROR_EXIT -1

FILE *fp; // file struct for stdin
char **tokens;
char *line;
int *pids;
int *IS_RUNNING;
int pid_num = 0;

void initialize()
{

    // allocate space for the whole line
    assert( (line = malloc(sizeof(char) * MAX_STRING_LEN)) != NULL);

    // allocate space for individual tokens
    assert( (tokens = malloc(sizeof(char*)*MAX_TOKENS)) != NULL);

    // open stdin as a file pointer
    assert( (fp = fdopen(STDIN_FILENO, "r")) != NULL);
    
    assert( (pids = malloc(sizeof(int) * MAX_PID_NUM)) != NULL);
    
    assert( (IS_RUNNING = malloc(sizeof(int) * MAX_PID_NUM)) != NULL);

}

void tokenize (char * string)
{
    int token_count = 0;
    int size = MAX_TOKENS;
    char *this_token;

    while ( (this_token = strsep( &string, " \t\v\f\n\r")) != NULL) {

        if (*this_token == '\0') continue;

        tokens[token_count] = this_token;

        printf("Token %d: %s\n", token_count, tokens[token_count]);

        token_count++;

        // if there are more tokens than space ,reallocate more space
        if(token_count >= size){
            size*=2;

            assert ( (tokens = realloc(tokens, sizeof(char*) * size)) != NULL);
        }
    }

    tokens[token_count] = NULL; // execvp expects a NULL at the end
}

void read_command()
{

    // getline will reallocate if input exceeds max length
    assert( getline(&line, &MAX_LINE_LEN, fp) > -1);

    printf("Shell read this line: %s\n", line);

    tokenize(line);
}

char *last_token()
{
    int i = 0;
    char *token = NULL;
    while (tokens[i] != NULL)
    {
        token = tokens[i];
        i++;
    }
    return token;
}

int run_command() {
    
    int background = 0;
    pid_t pid;

    if (strcmp( last_token(), "&") == 0) {
        background = 1;
    }
    
    if (strcmp( tokens[0], EXIT_STR ) == 0)
        return EXIT_CMD;
    
    //program running
    if (background == 1) {
        pid = fork();
        if(pid < 0){
            perror("fork failed:");
            exit(1);
        }
        pids[pid_num] = pid;
        IS_RUNNING[pid_num] = 1;
        pid_num++;
        if (pid_num > MAX_PID_NUM) {
            assert ( (pids = realloc(pids, sizeof(int *) * MAX_PID_NUM * 2)) != NULL);
            assert ( (IS_RUNNING = realloc(IS_RUNNING, sizeof(int *) * MAX_PID_NUM * 2)) != NULL);
        }
    }
    
    //check I/O redirection
    int input_sign_num = 0;
    int output_sign_num = 0;
    int input_sign_index = -1;
    int output_sign_index = -1;
    
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp( tokens[i], "<" ) ==0) {
            input_sign_num++;
            input_sign_index = i;
            if (tokens[i+1] == NULL || strcmp( tokens[i+1], ">" ) ==0 || strcmp( tokens[i], "|" ) ==0 || strcmp( tokens[i+1], "&" ) ==0 ) {
                input_sign_num++;
            }
        }
        if (strcmp( tokens[i], ">" ) ==0) {
            output_sign_num++;
            output_sign_index = i;
            if (tokens[i+1] == NULL || strcmp( tokens[i+1], "<" ) ==0 || strcmp( tokens[i], "|" ) ==0 || strcmp( tokens[i+1], "&" ) ==0 ) {
                output_sign_num++;
            }
        }
    }
    if (input_sign_num > 1 || output_sign_num >1) {
        printf("I/O redirection error\n");
        return UNKNOWN_CMD;
    }
    
    int token_num =0 ;
    while (tokens[token_num] != NULL) {
        token_num++;
    }
    if (token_num > 2 && (input_sign_num == 1 || output_sign_num == 1)) {
        if (input_sign_num == 1) {
            char *str;
            str = tokens[input_sign_index + 1];
            int fd = open(str, O_RDONLY);
            if (fd < 0) {
                printf("open error");
                return ERROR_EXIT;
            }
            dup2(fd, STDIN_FILENO);
        }
        if (output_sign_num == 1) {
            char *str;
            str = tokens[output_sign_index + 1];
            int fd = open(str, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                printf("open error");
                return ERROR_EXIT;
            }
            dup2(fd, STDOUT_FILENO);
        }
    }
    
    if (background == 0 || (background == 1 && pid == 0)) {
        if (strcmp( tokens[0], "cmd1" ) ==0)
        {
            int i = 1;
            while ( tokens[i] != NULL) {
                printf("cmd1 has arg:%s\n", tokens[i]);
                i++;
            }
        }else if (strcmp( tokens[0], "get_pid" ) ==0) {
            printf("Process PID: %d.\n", getpid());
        }else if (strcmp( tokens[0], "listjobs" ) == 0) {
            for(int i = 0; i < pid_num; i++){
                char *q = "finished";
                if (IS_RUNNING[i] == 1) {
                    q = "running";
                }
                printf("command %d with PID %d Status: %s\n", i+1, pids[i], q);
            }
        }else if (strcmp( tokens[0], "fg" ) == 0) {
            int ret, status;
            int id = -1;
            for (int i = 0; i < pid_num; i++) {
                if (pids[i] == atoi(tokens[1])) {
                    id = i;
                    break;
                }
            }
            ret = waitpid(atoi(tokens[1]), &status, 0);
            if (ret < 0) {
                perror("wait failed: ");
            }else{
                if (id != -1) {
                    IS_RUNNING[id] = 0;
                }
            }
        }else {
            int pid_1 = fork();
            if (pid_1 < 0) {
                printf("FORK EXECVP ERROR");
                return  ERROR_EXIT;
            }else if(pid_1 == 0){
                int a = 0;
                if (background == 1) {
                    a++;
                }
                if (input_sign_num == 1) {
                    a += 2;
                }
                if (output_sign_num == 1) {
                    a += 2;
                }
                char *arg1[token_num + 1 - a];
                for (int i = 0; i < token_num - a; i++) {
                    arg1[i] = tokens[i];
                    arg1[i+1] = NULL;
                }
                int ret = execvp(arg1[0], arg1);
                exit(ret);
            }else if(pid_1 > 0){
                if (background == 0) {
                    int ret, status;
                    ret = waitpid(pid_1, &status, 0);
                    if (WEXITSTATUS(status) == -1) {
                        printf("Unknown command.\n");
                    }
                }
            }
        }
        
        
            
        if (pid == 0 && background == 1) {
            //close(fileno(fp));
            exit(0);
        }
    }
    
    
        
    return UNKNOWN_CMD;
    
}

int main()
{
    initialize();

    do {
        printf("sh550> ");
        read_command();
        
    } while( run_command() != EXIT_CMD );

    return 0;
}
