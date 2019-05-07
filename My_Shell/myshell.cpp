/*
 Neet H Patel
 COP 5570
 Project 1: A Simplified Unix Shell Program
 10/25/2018
 */

//#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <unordered_map>
#include <string>
/***************************************************************************************************
        GLOBAL CONSTANTS AND VARIABLES
***************************************************************************************************/
/* Max limits for input line is 80 chars (including newline) but fgets null terminates the array
 that will have the input stored so one more space is required for the null terminator chracter */
#define MAXCHARS 81

/* Assume a max limit for arguments per command is 80 also but null termination requires one
 more character space */
#define MAXARGS 81

// Constants for pipe read and write descriptors for readability
#define READFD 0
#define WRITEFD 1

// I/O filestreams
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

// Delimiters for parsing the input line into commands
const char* cmd_delim = " \t\n&";
const char* pipe_cmd_delim = "|";
const char* io_cmd_sin = "<";
const char* io_cmd_sout = ">";
const char* bg_delim = "&";

/* Function pointer for internal commands. Useful when pipes are involved because it will not fork
 a child but run it as a function. */
void (*internal_cmd_func) (void);

// Variable for child PID
pid_t f = -1;

// Variable to determine status of forking and exec
int status;

// Variable to determine if pipe command and total number of pipe commands to help with looping
int is_pipe = 0;
int num_pipe_cmds = 0;

// Variable to determine if io redir command
int is_io_redir = 0;

// Variable to determine if background process
int background = 0;

/* Variable to determine if myls is one of the commands and to check whether "search" for myls by
 shell was successfull or not. Since myls will be forked, default it to fork but if "search" fails
 do not fork */
int is_myls = 0;
int myls_ignore = 1;

// Arrays to hold command line, parsed command arguments and myls path
char cmd_line[MAXCHARS];
char *args[MAXARGS];
char myls_path[71];

/* Array to hold pipes for piped commands. At most two pipes needed, one to read from and
 one to write to. Then they can be reused if more than 2 pipe commands. */
int pipes[2][2];

// stores the backgroud command to be run so to emulate output like bash shell
std::unordered_map<int, std::string> back_procs;
std::string bproc;
/***************************************************************************************************
        FUNCTIONS
***************************************************************************************************/
// checks the command line type whether its pipe, io redir, background process or regular
void cmd_line_type(const char *src);

// checks the command to see if it is internal command.
void check_internal_cmd(void);

/* parses the given string based on delimiter provided and stores the tokens into an array and null
 terminates.*/
void  parse(char *src, char **dest, const char* delim);

// runs/handles cd command
void run_cd(void);

// runs/handles pwd command
void run_pwd(void);

// runs/handles set command
void run_set(void);

// runs/handles exit command
void run_exit(void);

/* handles path searching for myls; returns 1 if successfully searched and myls found or 0;
 assigned to global variable "myls_ignore" which has a default value of 1 */
int handle_myls(void);

// runs/handles regular command line
void run_reg_cmd_line(void);

// runs/handles IO redirection command line
void run_io_redir_cmd_line(void);

// runs/handles pipe command line
void run_piped_cmd_line(void);

/* helper function that handles child exit depending on whether the child was ran as background
process or not */
void handle_child_exit(void);

// helper function that handles file opening in io redirection command line types
void handle_file_opening(const char *infile, const char *outfile);

/* helper function that redirect stdin to /dev/null for background processes that would read
 from the terminal if no stdin redirection is already involved
 */
void handle_background_redir(void);

/***************************************************************************************************
                                               MAIN
***************************************************************************************************/
int main(int argc, char* argv[]) {
    FILE* ifp = stdin;
    if (argc > 2) {
        fprintf(stderr, "Usage: myshell [< batchfile]\n");
        exit(EXIT_FAILURE);
    }
    printf("$ ");
    fflush(stdout);
    while (fgets(cmd_line, MAXCHARS, ifp)) {
        // check if any background processes finished in a loop
        while ((f = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("Done --> %d : %s\n", (int)f, back_procs[(int)f].c_str());
            back_procs.erase((int)f);
        }
        cmd_line_type(cmd_line);
        if (is_pipe) {
            run_piped_cmd_line();
        } else if (is_io_redir) {
            run_io_redir_cmd_line();
        } else {
            run_reg_cmd_line();
        }
        printf("$ ");
        fflush(stdout);
    }
    printf("\n");
    return 0;
}

/***************************************************************************************************
        DEFINITIONS
***************************************************************************************************/

void cmd_line_type(const char *src) {
    if (strstr(src, pipe_cmd_delim)) {
        //printf("Pipe command!\n");
        is_pipe = 1;
    } else if (strstr(src, io_cmd_sin) || strstr(cmd_line, io_cmd_sout)) {
        is_io_redir = 1;
    } else {
        is_pipe = 0;
        is_io_redir = 0;
    }
    if (strstr(src, bg_delim)) {
        background = 1;
        bproc = src;
        bproc.pop_back();
    }
    background = strstr(src, bg_delim) ? 1 : 0;

}

void check_internal_cmd() {
    if (strcmp(*args, "cd") == 0) {
        internal_cmd_func = run_cd;
    } else if (strcmp(*args, "pwd") == 0) {
        internal_cmd_func = run_pwd;
    } else if (strcmp(*args, "set") == 0) {
        internal_cmd_func = run_set;
    } else if (strcmp(*args, "exit") == 0) {
        internal_cmd_func = run_exit;
    } else {
        is_myls = strstr(*args, "myls") ? 1 : 0;
        internal_cmd_func = NULL;
    }
}

void  parse(char *src, char **dest, const char *delim) {
    char *tok = strtok(src, delim);
    while (tok != NULL) {
        *dest++ = tok;
        if (is_pipe) {
            ++num_pipe_cmds;
        }
        tok = strtok(NULL, delim);
    }
    *dest = NULL;
}

void run_cd() {
    if (!args[1]) { // emulate as if just typing cd by itself
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "cd: getenv HOME: %s\n", strerror(errno));
            if (background) {
                exit(EXIT_FAILURE);
            }
        }
        if (chdir(home)) {
            fprintf(stderr, "cd: %s\n", strerror(errno));
            if (background) {
                exit(EXIT_FAILURE);
            }
        }
    } else {
        if (chdir(args[1])) {
            fprintf(stderr, "cd: %s\n", strerror(errno));
            if (background) {
                exit(EXIT_FAILURE);
            }
        }
    }
}

void run_pwd() {
    char dir[4096];
    if (!getcwd(dir, sizeof(dir))) {
        fprintf(stderr, "set: getcwd: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("%s\n", dir);
}

void run_set() {
    if (!args[1]) {
        fprintf(stderr, "set: MYPATH not provided\n");
        if (background) {
            exit(EXIT_FAILURE);
        } else {
            return;
        }
    }
    char *var_path[3];
    parse(args[1], var_path, "=");
    if (!args[1]) {
        fprintf(stderr, "set: no path to MYPATH provided\n");
        return;
    }
    if (setenv(*var_path, var_path[1], 1)) {
        fprintf(stderr, "set: setenv: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void run_exit() {
    if (!background) {
        printf("\n");
    }
    exit(EXIT_SUCCESS);
}

void run_reg_cmd_line() {
    parse(cmd_line, args, cmd_delim); // parse command line into command and args
    if (*args == NULL) { // no command (empty command line)
        return;
    }
    check_internal_cmd();
    if (internal_cmd_func) {
        if (background) { // run internal command in forked process
            if ((f = fork()) < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (f == 0) {
                handle_background_redir(); // redirect stdin to /dev/null
                (*internal_cmd_func)();
                exit(EXIT_SUCCESS); // make sure command exits if function runs error free
            }
            handle_child_exit(); // handle waiting
        } else { // run internal command as builtin
            (*internal_cmd_func)();
        }
    } else { // external command
        if (is_myls) { // handle myls if myls command
            is_myls = 0;
            myls_ignore = handle_myls();
        }
        if (myls_ignore) { // proceed, if myls involved and no error OR myls not involved
            if ((f = fork()) < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (f == 0) { // child process; execute the command
                if (background) {
                    handle_background_redir();
                }
                if (execvp(*args, args) < 0) {
                    fprintf(stderr, "execvp: %s: %s\n", *args, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            handle_child_exit();
        } // end myls_ignore
        myls_ignore = 1;
    }
}

int handle_myls() {
    char *mp = getenv("MYPATH");
    if (!mp) {
        fprintf(stderr, "MYPATH not set!\n");
        return 0;
    }
    char mypath[81];
    strcpy(mypath, mp);
    char *paths[50];
    parse(mypath, paths, ":");
    char **i;
    struct stat buf;
    for (i = paths; *i; ++i) {
        if (**i == '~' || **i == '$') { // expand the ~ or the first $ in a path to obtain full path
            char *j = *i;
            if (*j == '~') {
                ++j;
                mp = getenv("HOME");
                if (!mp) { // continue with other paths
                    continue;
                }
                strcpy(myls_path, mp);
                strcat(myls_path, j);
            } else {
                ++j;
                int k = 0;
                char *l = j;
                myls_path[k++] = *j;
                ++l;
                while (*l && *l != '/') {
                    myls_path[k++]  = *l;
                    ++l;
                }
                myls_path[k] = '\0';
                mp = getenv(myls_path);
                if (!mp) {
                    continue;
                }
                strcpy(myls_path, mp);
                strcat(myls_path, l);
            }
        } else { // no expansion required
            strcpy(myls_path, *i);
        }
        strcat(myls_path, "/myls");
        if ((stat(myls_path, &buf)) == 0) {
            *args = myls_path;
            return 1;
        }
    }// end for
    fprintf(stderr, "myls not found in MYPATH!\n");
    return 0;
}

void run_io_redir_cmd_line() {
    char *files[5]; // command will be assigned to *args, infile to files[0], outfile to files[2]
    parse(cmd_line, files, io_cmd_sin);
    if (files[1]) { // stdin redirect involed so check if stdout also involved
        if (strstr(files[0], io_cmd_sout)) { // stdout redir right after cmd (left side)
            parse(files[0], &files[2], io_cmd_sout);
            *args = files[2];
            *files = files[1];
            files[2] = files[3];
            parse(files[2], &files[2], cmd_delim);
            if (!files[2]) { // if no file provided for stdout, exit
                fprintf(stderr, "no outfile provided!\n");
                return;
            }
        } else if (strstr(files[1], io_cmd_sout)) { // stdout redir after stdin redir (right side)
            *args = *files;
            parse(files[1], &files[1], io_cmd_sout);
            *files = files[1];
            parse(files[2], &files[2], cmd_delim);
            if (!files[2]) {
                fprintf(stderr, "no outfile provided!\n");
                return;
            }
        } else {
            *args = *files;
            *files = files[1];
        }
        parse(*files, files, cmd_delim);
        if (!*files) { // if no file provided for stdin, exit
            fprintf(stderr, "no infile provided!\n");
            return;
        }
    } else { // only stdout involved
        parse(cmd_line, files, io_cmd_sout);
        *args = *files;
        *files = NULL;
        files[2] = files[1];
        parse(files[2], &files[2], cmd_delim);
        if (!files[2]) {
            fprintf(stderr, "no outfile provided!\n");
            return;
        }
    }
    parse(*args, args, cmd_delim); // parse the command into command and arguments
    check_internal_cmd();
    int saved_out = -1, saved_in = -1;
    if (internal_cmd_func) {
        if (background) { // run internal command in forked process
            if ((f = fork()) < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (f == 0) { // child process; redirect
                handle_file_opening(*files, files[2]); // handle the opening of input and output files
                (*internal_cmd_func)();
                exit(EXIT_SUCCESS);
            }
            handle_child_exit();
        } else { // run internal command as builtin
            saved_in = dup(STDIN_FILENO); // preserving stdin and stdout
            saved_out = dup(STDOUT_FILENO);
            handle_file_opening(*files, files[2]);
            (*internal_cmd_func)();
            dup2(saved_in, STDIN_FILENO); // reassign stdin and stdout
            close(saved_in);
            dup2(saved_out, STDOUT_FILENO);
            close(saved_out);
        }
    } else { // external command
        if (is_myls) {
            is_myls = 0;
            myls_ignore = handle_myls();
        }
        if (myls_ignore) {
            if ((f = fork()) < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (f == 0) {
                handle_file_opening(*files, files[2]); // handle the opening of input and output files
                if (execvp(*args, args) < 0) {
                    fprintf(stderr, "execvp: %s: %s\n", *args, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            handle_child_exit();
        }// end myls_ignore
    }
    // cleanup global variables associated with io redirection
    is_io_redir = 0;
    myls_ignore = 1;
}

void run_piped_cmd_line() {
    char *cmds[81];
    parse(cmd_line, cmds, pipe_cmd_delim);
    is_pipe = 0;
    int saved_out = -1;
    int saved_in = -1;
    parse(*cmds, args, cmd_delim);
    if (pipe(pipes[0])) { // create first pipe
        fprintf(stderr, "pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    check_internal_cmd();
    if (internal_cmd_func) { // run internal command without fork
        saved_out = dup(STDOUT_FILENO); // preserve stdin and stdout as needed
        dup2(pipes[0][WRITEFD], STDOUT_FILENO);
        close(pipes[0][WRITEFD]); // dont close read end since we are in the parent
        (*internal_cmd_func)();
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    } else {
        if (is_myls) {
            is_myls = 0;
            myls_ignore = handle_myls();
        }
        if (myls_ignore) { // continue with the external command
            if ((f = fork()) < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (f == 0) {
                close(pipes[0][READFD]);
                dup2(pipes[0][WRITEFD], STDOUT_FILENO);
                close(pipes[0][WRITEFD]);
                printf("execing1: |%s|\n", *args);
                if (execvp(*args, args) < 0) {
                    fprintf(stderr, "execvp: %s: %s\n", *args, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            handle_child_exit();
        }// end myls_ignore
        myls_ignore = 1;
        close(pipes[0][WRITEFD]); // write end not needed for the next command; only read end needed
    }// end external command
    int i;
    int used_pipe = 0;
    int created_pipe = 1;
    for (i = 0; i < num_pipe_cmds - 2; ++i) { // handle middle commands in pipe
        parse(cmds[i + 1], args, cmd_delim);
        if (pipe(pipes[created_pipe])) {
            fprintf(stderr, "pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        check_internal_cmd();
        if (internal_cmd_func) {
            saved_in = dup(STDIN_FILENO); // preserve stdin and stdout as needed
            saved_out = dup(STDOUT_FILENO);
            dup2(pipes[used_pipe][READFD], STDIN_FILENO);
            close(pipes[used_pipe][READFD]); // used pipe's read and write closed at this point
            dup2(pipes[created_pipe][WRITEFD], STDOUT_FILENO);
            close(pipes[created_pipe][WRITEFD]); // created pipe's write end closed, read end open
            (*internal_cmd_func)();
            dup2(saved_in, STDIN_FILENO);
            close(saved_in);
            dup2(saved_out, STDOUT_FILENO);
            close(saved_out);
        } else {
            if (is_myls) {
                is_myls = 0;
                myls_ignore = handle_myls();
            }
            if (myls_ignore) {
                if ((f = fork()) < 0) {
                    fprintf(stderr, "fork: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if (f == 0) {
                    close(pipes[created_pipe][READFD]);
                    dup2(pipes[used_pipe][READFD], STDIN_FILENO);
                    close(pipes[used_pipe][READFD]);
                    dup2(pipes[created_pipe][WRITEFD], STDOUT_FILENO);
                    close(pipes[created_pipe][WRITEFD]);
                    if (execvp(*args, args) < 0) {
                        fprintf(stderr, "execvp: %s: %s\n", *args, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                }
                handle_child_exit();
            } // end myls_ignore
            myls_ignore = 1;
            close(pipes[used_pipe][READFD]); // used pipe's read and write closed at this point
            close(pipes[created_pipe][WRITEFD]); // created pipe's write end closed, read end open
        }// end external command
        int t = used_pipe;
        used_pipe = created_pipe;
        created_pipe = t; // created pipe is free; used pipe only has read end open
    }// end for
    parse(cmds[i + 1], args, cmd_delim); // run last command in pipe
    check_internal_cmd();
    if (internal_cmd_func) {
        saved_in = dup(STDIN_FILENO); // preserve stdin and stdout as needed
        dup2(pipes[used_pipe][READFD], STDIN_FILENO);
        close(pipes[used_pipe][READFD]); // all pipe's read and write fd closed
        (*internal_cmd_func)();
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    } else {
        if (is_myls) {
            is_myls = 0;
            myls_ignore = handle_myls();
        }
        if (myls_ignore) {
            if ((f = fork()) < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (f == 0) {
                dup2(pipes[used_pipe][READFD], STDIN_FILENO);
                close(pipes[used_pipe][READFD]);
                if (execvp(*args, args) < 0) {
                    fprintf(stderr, "execvp: %s: %s\n", *args, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            handle_child_exit();
        } //end myls_ignore
        myls_ignore = 1;
        close(pipes[used_pipe][READFD]); // all pipe's read and write fd closed
    }// end external
    // cleanup global variables associated with piped commands
    num_pipe_cmds = 0;
    is_pipe = 0;
}

void handle_child_exit() {
    if (background) {
        back_procs[(int)f] = bproc; // insert the backgroun command to be run in the table
        background = 0;
        return;
    }
    if (wait(&status) == -1) {
        fprintf(stderr, "wait: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void handle_file_opening(const char *infile, const char *outfile) {
    if (infile) { // redir stdin
        close(STDIN_FILENO);
        if (open(infile, O_RDONLY) == -1) {
            fprintf(stderr, "open: input file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    if (outfile) { // redir stdout
        close(STDOUT_FILENO);
        if (open(outfile, O_CREAT | O_WRONLY) == -1) {
            fprintf(stderr, "open: output file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

void handle_background_redir() {
    close(STDIN_FILENO); // redir stdin to /dev/null
    if (open("/dev/null", O_RDONLY) == -1) {
        fprintf(stderr, "open: /dev/null: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
