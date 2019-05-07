//
//  lexer.c
//  lextest
//
//  Created by Neet Patel on 2/3/18.
//  Copyright © 2018 Neet Patel. All rights reserved.
//


#include "global.h"

char lexbuf[BSIZE];
int lineno = 1;
int tokenval = NONE;

int lexan () {
    int t;
    while (1) {
        t = getchar();
        if (t == ' ' || t == '\t') {
            ;
        } else if (t == '\n') {
            lineno = lineno + 1;
        } else if (isdigit(t)) {
            ungetc(t, stdin);
            scanf("%d", &tokenval);
            if (tokenval < 128) {
                return INT8;
            } else if (tokenval < 32768) {
                return INT16;
            } else if (tokenval < 2147483647) {
                return INT32;
            } else {
                error("integer size error");
            }
        } else if (isalpha(t) || t == '_') {
            int p, b = 0;
            while (isalnum(t) || t == '_') {
                lexbuf[b] = t;
                t = getchar();
                b = b + 1;
                if (b >= BSIZE) {
                    error("compiler error");
                }
            }
            lexbuf[b] = EOS;
            if (t != EOF) {
                ungetc(t, stdin);
            }
            p = lookup(lexbuf);
            if (p == 0) {
                //printf("calling insert\n");
                p = insert(lexbuf, ID);
            }
            int token = symtable[p].token;
            if (token != ID) {
                tokenval = NONE;
            } else {
                tokenval = p - SUB;
            }
            return token;
        } else if (t == EOF) {
            return DONE;
        } else {
            tokenval = NONE;
            return t;
        }
    }
}
