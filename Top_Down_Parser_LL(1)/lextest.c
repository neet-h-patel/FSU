//
//  lextest.c
//
//  Created on 2/3/18.
//

#include <stdio.h>
#include "global.h"

int main(void) {
	init(); // initialize symbol table with keywords
	int token = lexan(); // get first token as lookahead
	while (token != DONE) {
		switch (token) {
		case ID:
			printf("<ID,%d> ", tokenval);
			break;
		case INT8:
			printf("<INT8,%d> ", tokenval);
			break;
		case INT16:
			printf("<INT16,%d> ", tokenval);
			break;
		case INT32:
			printf("<INT32,%d> ", tokenval);
			break;
		case IF:
			printf("<IF,%d> ", tokenval);
			break;
		case ELSE:
			printf("<ELSE,%d> ", tokenval);
			break;
		case WHILE:
			printf("<WHILE,%d> ", tokenval);
			break;
		case RETURN:
			printf("<RETURN,%d> ", tokenval);
			break;
		case ARG:
			printf("<ARG,%d> ", tokenval);
			break;
		case DONE:
			return 0;
		default:
			printf("<%c,%d> ", token, tokenval);
			break;
		}
		token = lexan();
	}
	printf("\n");
	return 0;
}
