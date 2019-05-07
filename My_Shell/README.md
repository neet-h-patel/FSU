myshell and myls
----------------
- myshell is a simplified UNIX shell emulation program.
- myshell supports four internal commands, which are directly interpreted by the shell.
  - These are: cd, set, pwd and exit
  - The set command is used to define MYPATH environment variable with syntax:

    set MYPATH=path1:path2:...

    The paths in this variable are then searched to locate the myls program which emulates the ls -l command.
    Therefore myls must be located in one of those paths and then MYPATH must be set correctly

- myshell uses execvp for any other external commands that are found in $PATH
- myshell doesn't support job control and supports limited background processing (see ASSUMPTIONS for detail)

COMPILING
--------------------------
- to compile both myshell and myls:
  $ make all
  OR
  $ make

- to compile individual programs:
  $ make myshell
  $ make myls

- to clean directory:
  $ make clean

ASSUMPTIONS:
------------
 1) Command line can contain no more than 80 characters including newline
 2) Arguments to a command cannot contain quotes
 3) Background processing and piped commands cannot appear in any single, given command line
 4) File io redirection and piped commands cannot appear in any single, given command line
 5) If running the command in background, it must be the only command in the command line
    -- eg. this is a valid command line: echo test &
    -- this is not a valid command line: echo test & echo ting &
 5) Environemnt variables will not be expanded for external commands
    -- eg. "echo $USER" will print "$USER"
 6) ~, $HOME or other path varibles will be expanded for myls BUT they can only be at the start of a path.
    -- eg. this is valid: ~/:$HOME/:
    -- this is not valid: ~/$SOME_PATH:$OTHER/$SUBPATH
