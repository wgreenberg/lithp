#include "language.h"

sexp * parse_program (char *program_txt);

int parse_sexp (char *token, size_t token_size, sexp *exp);
pair * parse_pair (char *token, size_t token_size);
atom * parse_atom (char *token, size_t token_size);
