//
//  global.h
//
//  Created on 2/3/18.
//

#ifndef global_h
#define global_h


#include <stdio.h>
#include <ctype.h>

#define BSIZE 128
#define NONE -1
#define EOS '\0'
#define IF 256
#define ELSE 257
#define WHILE 258
#define RETURN 259
#define DONE 300
#define ID 301
#define INT8 302
#define INT16 303
#define INT32 304
#define ARG 305

#define SUB 3 // value to subtract in order for ID tokenval to range from [3,inf)

extern int tokenval;
extern int lineno;

struct entry {
	char *lexptr;
	int token;
};

extern struct entry symtable[];

// functions for symbol.c
int lookup(char s[]);
int insert(char s[], int tok);

//functions for init.c
void init(void);

// funcitons for error.c
void error(char *m);

//functions for lexer.c
int lexan(void);

#endif
