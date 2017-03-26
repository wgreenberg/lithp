#include "language.h"

SExp * parse_program (char *program_txt);

int parse_sexp (char *token, size_t token_size, SExp *exp);
int parse_pair (char *token, size_t token_size, Pair *pair);
int parse_atom (char *token, size_t token_size, Atom *atom);

char * tokenize (char *string);
char * next_token ();
char * peek_next_token ();
