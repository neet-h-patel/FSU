//
//  compile.c
//
//  Created on 2/4/18.
//

#include "javaclass.h"
#include "bytecode.h"
#include "global.h"

// Functions associated with Non-terminals
void stmt(void);
void opt_stmt(void);
void expr(void);
void term(void);
void factor(void);
void match(int);

// Utility functions
void add_ret(int);          // adds loc of goto ins to the ret_goto_arr array
void backpatch_ret(void);   // backpatch the goto's for return labels
void emit_init(void);       // emit initial code
void emit_end(void);        // emit code after backpatching return labels

// Class file globals
struct ClassFile cf;
int index1, index2, index3;
int label1, label2;

// Parse related globals
int lookahead;
int *ret_goto_arr;          // array to store goto ins of return
int ret_goto_cap = 8;       // inital size of array
int num_ret_goto = 0;       // index of ret_goto_arr

/*******************************************************************************
            						MAIN
*******************************************************************************/
int main(void) {
	init(); // initialize symbol table
	emit_init(); // emit initial code
	ret_goto_arr = (int *)malloc(sizeof(int) * ret_goto_cap);
	if (ret_goto_arr == NULL) {
		printf("ret_goto_arr: malloc failed\n");
		exit(1);
	}
	lookahead = lexan();
	stmt(); // parse
	backpatch_ret(); // backpatch all returns
	emit_end(); // emit final code

	// pre-save operations
	cf.methods[0].code_length = pc;
	cf.methods[0].code = copy_code();
	save_classFile(&cf);

	free(ret_goto_arr);
	return 0;
}
/*******************************************************************************/
// Definitions of Non Terminal Functions
void stmt() {
	if (lookahead == '{') {
		match('{');
		opt_stmt();
		match('}');
	} else if (lookahead == ID) {
		int locvar = tokenval;
		match(ID);
		match('=');
		expr();
		emit2(istore, locvar);
		match(';');
	} else if (lookahead == IF) {
		int out, el;
		match(IF);
		match('(');
		expr();
		match(')');
		emit(iconst_0);
		el = pc;                    // label for branching to else
		emit3(if_icmpeq, PAD);      // will be backpatched
		stmt();
		out = pc;                   // label for branching past else
		emit3(goto_, PAD);          // will be backpatched
		match(ELSE);
		backpatch(el, pc - el);     // one past 'goto' is the location of else
		stmt();
		backpatch(out, pc - out);   // location past if else
	} else if (lookahead == WHILE) {
		int cond, out;
		match(WHILE);
		match('(');
		cond = pc;                  // location for conditional expr
		expr();
		match(')');
		emit(iconst_0);
		out = pc;                   // label for branching out of while
		emit3(if_icmpeq, PAD);      // will be backpached
		stmt();
		emit3(goto_, cond - pc);    // goto condition expr
		backpatch(out, pc - out);   // location past while
	} else if (lookahead == RETURN) {
		match(RETURN);
		expr();
		emit(istore_2);
		add_ret(pc);
		emit3(goto_, PAD);    // goto end of code
		match(';');
	} else {
		error("syntax error");
	}
}

void opt_stmt() {
	// keep calling statement if the lookahead is in FIRST(stmt)
	while (lookahead == '{'
	        || lookahead == ID
	        || lookahead == IF
	        || lookahead == WHILE
	        || lookahead == RETURN) {
		stmt();
	}
}

void expr() {
	term();
	// procedure moreterms "inlined"
	while (1) {
		if (lookahead == '+') {
			match('+');
			term();
			emit(iadd);
		} else if (lookahead == '-') {
			match('-');
			term();
			emit(isub);
		} else {
			break;
		}
	}
}

void term() {
	factor();
	// procedure morefactors inlined
	while (1) {
		if (lookahead == '*') {
			match('*');
			factor();
			emit(imul);
		} else if (lookahead == '/') {
			match('/');
			factor();
			emit(idiv);
		} else if (lookahead == '%') {
			match('%');
			factor();
			emit(irem);
		} else {
			break;
		}
	}
}

void factor() {
	if (lookahead == '(') {
		match('(');
		expr();
		match(')');
	} else if (lookahead == '-') {
		match('-');
		factor();
		emit(ineg);
	} else if (lookahead == INT8) {
		emit2(bipush, tokenval);
		match(INT8);
	} else if (lookahead == INT16) {

		emit3(sipush, tokenval);
		match(INT16);
	} else if (lookahead == INT32) {
		int tv = tokenval;
		match(INT32);
		emit2(ldc, constant_pool_add_Integer(&cf, tv));
	} else if (lookahead == ID) {
		emit2(iload, tokenval);
		match(ID);
	} else if (lookahead == ARG) {
		int num;
		match(ARG);
		match('[');
		num = tokenval;
		match(INT8);
		match(']');
		emit(aload_1);
		emit2(bipush, num);
		emit(iaload);
	} else {
		error("syntax error");
	}
}

void match(int t) {
	if (lookahead == t) {
		lookahead = lexan();
	} else {
		error("syntax error");
	}
}
/*******************************************************************************/
// Utility function definitions
void add_ret(int loc) {
	if (num_ret_goto == ret_goto_cap) {
		ret_goto_cap *= 2;
		ret_goto_arr = realloc(ret_goto_arr, sizeof(int) * ret_goto_cap);
		if (ret_goto_arr == NULL) {
			printf("ret_goto_arr: realloc failed\n");
			exit(1);
		}
	}
	ret_goto_arr[num_ret_goto++] = loc;
}

void backpatch_ret() {
	if (num_ret_goto != 0) {
		int rg, i;
		for (i = 0; i < num_ret_goto; ++i) {
			rg = ret_goto_arr[i];
			backpatch(rg, pc - rg);
		}
	}
}

void emit_init() {
	init_ClassFile(&cf);    // new class file structure
	cf.access = ACC_PUBLIC; // class access type
	cf.name = "Code";       // class name to be
	cf.field_count = 0;     // num fields
	cf.method_count = 1;    // num methods
	// allocate array for num methods
	cf.methods = (struct MethodInfo*)malloc(cf.method_count * sizeof(struct MethodInfo));
	cf.methods[0].access = ACC_PUBLIC | ACC_STATIC; // method access type
	cf.methods[0].name = "main"; // method name
	// method descriptor "void main(String[] arg)"
	cf.methods[0].descriptor = "([Ljava/lang/String;)V";
	cf.methods[0].max_stack = 127; // max size of operand stack
	cf.methods[0].max_locals = 127; // max local vars

	init_code();

	emit(aload_0);
	emit(arraylength);
	emit2(newarray, T_INT);
	emit(astore_1);
	emit(iconst_0);
	emit(istore_2);
	label1 = pc;
	emit(iload_2);
	emit(aload_0);
	emit(arraylength);
	label2 = pc;
	emit3(if_icmpge, PAD);
	emit(aload_1);
	emit(iload_2);
	emit(aload_0);
	emit(iload_2);
	emit(aaload);
	index1 = constant_pool_add_Methodref(&cf, "java/lang/Integer", "parseInt", "(Ljava/lang/String;)I");
	emit3(invokestatic, index1);
	emit(iastore);
	emit32(iinc, 2, 1);
	emit3(goto_, label1 - pc);
	backpatch(label2, pc - label2);
}

void emit_end() {
	index2 = constant_pool_add_Fieldref(&cf, "java/lang/System", "out", "Ljava/io/PrintStream;");
	emit3(getstatic, index2);
	emit(iload_2);
	index3 = constant_pool_add_Methodref(&cf, "java/io/PrintStream", "println", "(I)V");
	emit3(invokevirtual, index3);
	emit(return_);
}
