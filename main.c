#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "parse.h"

sexp *
eval (sexp *exp) {
    return exp;
}

void
print (sexp *exp) {
    if (exp->type == ATOM) {
        atom *a = exp->atom;
        if (a->type == NUMBER) {
            printf("%ld", a->number_value);
        } else if (a->type == BOOLEAN) {
            if (a->number_value)
                printf("#t");
            else
                printf("#f");
        } else if (a->type == CHARACTER) {
            if (a->character_value == ' ') {
                printf("#\\space");
            } else if (a->character_value == '\n') {
                printf("#\\newline");
            } else {
                printf("#\\%c", a->character_value);
            }
        } else if (a->type == STRING) {
            printf("%s", a->string_value);
        } else {
            printf("ERR: Unable to print invalid sexp");
        }
    } else if (exp->type == PAIR) {
        printf("pair???");
    } else if (exp->type == NIL) {
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
        sexp *program = parse_program(buf);
        sexp *result = eval(program);

        // print
        print(result);
    }
    free(buf);
}
