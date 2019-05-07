//
//  init.c
//
//  Created on 2/4/18.
//

#include "global.h"

struct entry keywords[] = {
    {"if", IF},
    {"else", ELSE},
    {"while", WHILE},
    {"return", RETURN},
    {"arg", ARG},
    {0, 0}
};

void init() {
    struct entry *p;
    for (p = keywords; p->token; p++){
        insert(p->lexptr, p->token);
    }
}
