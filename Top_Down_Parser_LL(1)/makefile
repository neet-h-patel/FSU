CC=cc
CPP=c++
LIBS=
COFLAGS=-g -O2
CWFLAGS=-Wall -Wextra
CIFLAGS=
CMFLAGS=
CFLAGS=$(CWFLAGS) $(COFLAGS) $(CIFLAGS) $(CMFLAGS)

all:		compile lextest calcclass Calc

calcclass:	javaclass.h bytecode.h calcclass.c javaclass.o bytecode.o
		$(CC) $(CFLAGS) -o calcclass calcclass.c javaclass.o bytecode.o

Calc:		calcclass
		./calcclass

compile:	javaclass.h bytecode.h global.h compile.c lexer.c init.c symbol.c error.c javaclass.o bytecode.o
		$(CC) $(CFLAGS) -o compile compile.c lexer.c init.c symbol.c error.c javaclass.o bytecode.o

lextest: 	lextest.c error.c init.c symbol.c lexer.c
		$(CC) $(CFLAGS) -o $@ $< error.c init.c symbol.c lexer.c
.c.o:
		$(CC) $(CFLAGS) -c $<

.PHONY: 	clean distclean
clean:
		-rm -f *.o
distclean:
		-rm -f compile lextest calcclass *.o *.class
