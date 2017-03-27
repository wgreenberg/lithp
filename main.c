#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "parse.h"

SExp *
eval (SExp *exp) {
    return exp;
}

void
print (SExp *exp) {
    if (exp->type == SEXP_TYPE_ATOM) {
        Atom *a = exp->atom;
        if (a->type == ATOM_TYPE_NUMBER) {
            printf("%ld", a->number_value);
        } else if (a->type == ATOM_TYPE_BOOLEAN) {
            if (a->number_value)
                printf("#t");
            else
                printf("#f");
        } else if (a->type == ATOM_TYPE_CHARACTER) {
            if (a->character_value == ' ') {
                printf("#\\space");
            } else if (a->character_value == '\n') {
                printf("#\\newline");
            } else {
                printf("#\\%c", a->character_value);
            }
        } else if (a->type == ATOM_TYPE_STRING) {
            printf("%s", a->string_value);
        } else {
            printf("ERR: Unable to print invalid sexp");
        }
    } else if (exp->type == SEXP_TYPE_PAIR) {
        printf("pair???");
    } else if (exp->type == SEXP_TYPE_NIL) {
        printf("()");
    } else {
        printf("ERR: Unable to print invalid sexp");
    }
    printf("\n");
}

int main (void) {
    printf("Welcome to Lithp\n");
    size_t buf_size = 1024;
    char *buf = malloc(buf_size);
    while (1) {
        // read
        printf("> ");
        getline(&buf, &buf_size, stdin);

        // evaluate
        SExp *program = parse_program(buf);
        SExp *result = eval(program);

        // print
        print(result);
    }
    free(buf);
}
