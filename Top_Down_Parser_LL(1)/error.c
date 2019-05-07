//
//  error.c
//
//  Created on 2/3/18.
//

#include <stdio.h>
#include <stdlib.h>
#include "global.h"


void error(char *m) {
	fprintf(stderr, "line %d: %s\n", lineno, m);
	exit(1);
}
