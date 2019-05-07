/*
  Neet H Patel
  COP 5570
  Project 1: A Simplified Unix Shell Program
  10/25/2018

  myls - a similar implementation of ls -l command
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <list>
#include <string>
#include <map>
#include <algorithm>
/***************************************************************************************************
        GLOBAL CONSTANTS AND VARIABLES
***************************************************************************************************/
// variable to hold the fully expanded path
char abs_path[2048];
// variable to hold the absolute path of dir contents
char file_path[2048];
// variable to hold the abs path of symlink
char sym_point[2048];
// variables to handle width spaces to get uniform output like ls -l
unsigned nlinks_wspace = 0;
unsigned user_wspace = 0;
unsigned group_wspace = 0;
unsigned size_wspace = 0;

// list that stores the file information whilst the width spaces have been finalized
std::map< std::string, std::list< std::string > > files;

/***************************************************************************************************
        FUNCTIONS
***************************************************************************************************/
// handles the ls -l of some_path argument based on its type
void print_ls_l(char *some_path);

// handles resolves the ~ or $ starting paths
void resolve_special(char *path);

// stores the actual ls -l information of the obtained filename as a list and stores it into the files list
void print_info(char *filename);

// updated the widht spaces of some fields in the ls -l output
void update_space(int field, const std::string val);

// prints the stored lists in files list
void print_lists();

/***************************************************************************************************
                                               MAIN
***************************************************************************************************/
int main(int argc, char **argv) {
    if (argc  > 2) {
        fprintf(stderr, "Usage: myls [filename]\n");
        exit(EXIT_FAILURE);
    }
    print_ls_l(argv[1]);
}

void print_ls_l(char *some_path) {
    struct stat buf;
    resolve_special(some_path); // resolve the path name to obtain full path
    if (lstat(abs_path, &buf) < 0) {
        fprintf(stderr, "stat %s failed\n", abs_path);
        exit(EXIT_FAILURE);
    }
    if (S_ISDIR(buf.st_mode)) { // iterate through directory
        DIR *d;
        struct dirent *w;
        if ((d = opendir(abs_path)) == NULL) {
            fprintf(stderr, "opendir failed.\n");
            exit(EXIT_FAILURE);
        }
        while ((w = readdir(d)) != NULL) { // obtain file paths
            if ((strcmp(w->d_name, ".") == 0) || (strcmp(w->d_name, "..") == 0) || *w->d_name == '.') {
                continue;
            }
            strcpy(file_path, abs_path);
            strcat(file_path, "/");
            strcat(file_path, w->d_name);
            print_info(w->d_name);
        }
        closedir(d);
    } else { // not a directory so simply print the information
        strcpy(file_path, abs_path);
        print_info(file_path);
    }
    print_lists();
}

void print_info(char *filename) {
    struct stat buf;
    //files.push_back(std::list< std::string >()); // store a new list
    char time_stamp[50];
    if (lstat(file_path, &buf) < 0) {
        fprintf(stderr, "stat %s failed (probably file does not exist).\n", file_path);
        exit(EXIT_FAILURE);
    }
    std::string f(filename);
    transform(f.begin(), f.end(), f.begin(), ::tolower); // to sort lexicographically like ls -l
    char perms[11];
    perms[10] = '\0';
    if (S_ISREG(buf.st_mode)) {
        *perms = '-';
    } else if (S_ISDIR(buf.st_mode)) {
        *perms = 'd';
    } else if (S_ISCHR(buf.st_mode)) {
        *perms = 'c';
    } else if (S_ISBLK(buf.st_mode)) {
        *perms = 'b';
    } else if (S_ISFIFO(buf.st_mode)) {
        *perms = 'f';
    } else if (S_ISLNK(buf.st_mode)) {
        *perms = '-';
    } else if (S_ISSOCK(buf.st_mode)) {
        *perms = 's';
    } else {
        *perms = 'u';
    }
    if (S_IRUSR & buf.st_mode) {
        perms[1] = 'r';
    } else {
        perms[1] = '-';
    }
    if (S_IWUSR & buf.st_mode) {
        perms[2] = 'w';
    } else {
        perms[2] = '-';
    }
    if (S_IXUSR & buf.st_mode) {
        perms[3] = 'x';
    } else {
        perms[3] = '-';
    }
    if (S_IRGRP & buf.st_mode) {
        perms[4] = 'r';
    } else {
        perms[4] = '-';
    }
    if (S_IWGRP & buf.st_mode) {
        perms[5] = 'w';
    } else {
        perms[5] = '-';
    }
    if (S_IXGRP & buf.st_mode) {
        perms[6] = 'x';
    } else {
        perms[6] = '-';
    }
    if (S_IROTH & buf.st_mode) {
        perms[7] = 'r';
    } else {
        perms[7] = '-';
    }
    if (S_IWOTH & buf.st_mode) {
        perms[8] = 'w';
    } else {
        perms[8] = '-';
    }
    if (S_IXOTH & buf.st_mode) {
        perms[9] = 'x';
    } else {
        perms[9] = '-';
    }
    files[f].push_back(std::string(perms)); // push the permissions string into list
    update_space(0, std::to_string((int)buf.st_nlink)); // update wspace for links
    files[f].push_back(std::to_string((int)buf.st_nlink));
    update_space(1, std::string(getpwuid(buf.st_uid)->pw_name));
    files[f].push_back(std::string(getpwuid(buf.st_uid)->pw_name));
    update_space(2, std::string(getgrgid(buf.st_gid)->gr_name));
    files[f].push_back(std::string(getgrgid(buf.st_gid)->gr_name));
    update_space(3, std::to_string((int)buf.st_size));
    files[f].push_back(std::to_string((int)buf.st_size));
    strftime(time_stamp, 50, "%b %d %R", localtime(&buf.st_mtime));
    files[f].push_back(std::string(time_stamp));
    files[f].push_back(std::string(filename));
    if (S_ISLNK(buf.st_mode)) {
        memset(sym_point, '\0', 2048);
        readlink(file_path, sym_point, 2048);
        files[f].push_back(std::string(" -> "));
        files[f].push_back(std::string(sym_point));
    }
}

void print_lists() {
    std::map< std::string, std::list< std::string > >::iterator it;
    std::list< std::string >::iterator it2;
    printf("total ----\n");
    for (it = files.begin(); it != files.end(); ++it) { // iterate through files list
        it2 = it->second.begin(); // get a file and print its info
        printf("%s", (*it2++).c_str());
        printf(" ");
        printf("%*d", nlinks_wspace, stoi((*it2++)));
        printf(" ");
        printf("%-*s", user_wspace, (*it2++).c_str());
        printf(" ");
        printf("%-*s", group_wspace, (*it2++).c_str());
        printf(" ");
        printf("%*d", size_wspace, stoi((*it2++)));
        printf(" ");
        printf("%s", (*it2++).c_str());
        printf(" ");
        printf("%s", (*it2++).c_str());
        if (it2 != it->second.end()) { // symlink
            printf("%s", (*it2++).c_str()); // prints the ->
            printf("%s", (*it2++).c_str()); // prints the absolute path
        }
        printf("\n");
    }
}

void resolve_special(char *some_path) {
    char *i = some_path;
    if (!i) {
        if (!getcwd(abs_path, sizeof(abs_path))) {
            fprintf(stderr, "myls: getcwd: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        if (*i == '~' || *i == '$') { // expand the ~ or the first $ in a path to obtain full path
            printf("~ or $\n");
            char *j = i;
            if (*j == '~') {
                ++j;
                i = getenv("HOME");
                if (!i) {
                    fprintf(stderr, "myls: getenv: $HOME: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                strcpy(abs_path, i);
                strcat(abs_path, j);
                printf("resolved path is: %s\n", abs_path);
            } else {
                ++j;
                int k = 0;
                char *l = j;
                some_path[k++] = *j;
                ++l;
                while (*l && *l != '/') {
                    some_path[k++]  = *l;
                    ++l;
                }
                some_path[k] = '\0';
                i = getenv(some_path);
                if (!i) {
                    fprintf(stderr, "myls: getenv: $%s: %s\n", some_path, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                strcpy(abs_path, i);
                strcat(abs_path, l);
            }
        } else {
            strcpy(abs_path, i);
        }
    }
}

void update_space(int field, const std::string val) {
    if (field == 0 && val.length() > nlinks_wspace) { // update wspace for nlinks
        nlinks_wspace = val.length();
    } else if (field == 1 && val.length() > user_wspace) { // update wspace for user
        user_wspace = val.length();
    } else if (field == 2 && val.length() > group_wspace) { // update wspace for group
        group_wspace = val.length();
    } else if (field == 3 && val.length() > size_wspace) { // update wspace for size
        size_wspace = val.length();
    }
}
