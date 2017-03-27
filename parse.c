#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "parse.h"

int
is_number (char *token) {
    char *endptr;
    strtol(token, &endptr, 0);
    return (*endptr == 0);
}

char
to_character (char *token, size_t token_size) {
    if (token_size == 3) {
        return token[2];
    } else {
        if (strcmp(token + 2, "newline") == 0) {
            return '\n';
        } else if (strcmp(token + 2, "space") == 0) {
            return ' ';
        }
    }
    return '\0';
}

int
is_nil (char *token, size_t token_size) {
    if (token_size == 2) {
        return token[0] == '(' && token[1] == ')';
    }
    return 0;
}

int _t_idx;
int _t_orig_len;
char _t_buf[MAX_STRING_SIZE];
char _t_peek_buf[MAX_STRING_SIZE];
char *_t_orig;

char *
tokenize (char *string) {
    _t_idx = 0;
    _t_orig_len = strlen(string);
    _t_orig = string;
    if (_t_orig_len == 0)
        return NULL;

    return next_token();
}

const char* delim = " ()\n\"\\\0";
int n_delim = 7;

int
is_delim (char c) {
    int i;
    for (i=0; i<n_delim; i++) {
        if (delim[i] == c)
            return 1;
    }
    return 0;
}

int
_next_token(char *buf) {
    if (_t_idx >= _t_orig_len)
        return 0;

    int i=0;
    char *bookmark = _t_orig + _t_idx;
    int next_t_pos = strcspn(bookmark, delim);

    if (next_t_pos == 0) {
        buf[i++] = bookmark[0];
    } else {
        for (i=0; i<next_t_pos; i++) {
            buf[i] = bookmark[i];
        }
    }
    buf[i] = '\0';
    return i;
}

char *
peek_next_token () {
    int buf_len = _next_token(_t_peek_buf);
    if (buf_len == 0)
        return NULL;
    return _t_peek_buf;
}

char *
next_token () {
    int buf_len = _next_token(_t_buf);
    if (buf_len == 0)
        return NULL;
    _t_idx += buf_len;
    return _t_buf;
}

int
parse_atom (char *token, size_t token_size, Atom *atom) {
    int string_buf_idx;
    int token_buf_idx;
    int string_terminated;
    int token_buf_start;
    int is_escaped;

    if (is_number(token)) {
        atom->type = ATOM_TYPE_NUMBER;
        atom->number_value = strtol(token, NULL, 0);
        return 0;
    } else if (token[0] == '#') {
        if (token_size == 1) {
            token = next_token();
            if (token == NULL || token[0] != '\\') {
                return 1;
            }
            token = next_token();
            if (token == NULL) {
                return 1;
            }
            token_size = strlen(token);
            atom->type = ATOM_TYPE_CHARACTER;
            if (token_size == 1) {
                atom->character_value = token[0];
                return 0;
            } else {
                if (strcmp(token, "newline") == 0) {
                    atom->character_value = '\n';
                    return 0;
                } else if (strcmp(token, "space") == 0) {
                    atom->character_value = ' ';
                    return 0;
                }
            }
        } else if (token_size == 2) {
            if (token[1] == 't' || token[1] == 'f') {
                atom->type = ATOM_TYPE_BOOLEAN;
                atom->number_value = (token[1] == 't');
                return 0;
            }
        }
    } else if (token[0] == '"') {
        atom->type = ATOM_TYPE_STRING;

        string_buf_idx = 0;
        string_terminated = 0;
        is_escaped = 0;
        while ((token = next_token())) {
            token_size = strlen(token);
            token_buf_start = 0;

            if (is_escaped) {
                if (token[0] == 'n')
                    atom->string_value[string_buf_idx++] = '\n';
                else // not totally correct but w/e
                    atom->string_value[string_buf_idx++] = token[0];
                is_escaped = 0;
                token_buf_start++;
            }

            if (token[token_buf_start] == '"') {
                string_terminated = 1;
                char *next_token = peek_next_token();
                if (next_token != NULL && !is_delim(next_token[0])) {
                    printf("Can't terminate quote here: %s\n", token);
                    return 1;
                }
                atom->string_value[string_buf_idx] = '\0';
                break;
            }

            if (token[token_buf_start] == '\\') {
                is_escaped = 1;
                continue;
            }

            for (token_buf_idx=token_buf_start; token_buf_idx < token_size; token_buf_idx++) {
                char c = token[token_buf_idx];
                atom->string_value[string_buf_idx++] = c;
            }
        }

        if (!string_terminated) {
            printf("Unterminated string\n");
            return 1;
        }
        return 0;
    }
    return 1;
}

int
is_pair_start (char *token) {
    return token[0] == '(';
}

int
is_pair_end (char *token, size_t token_size) {
    return token[token_size - 1] == ')';
}

int
parse_pair (char *token, size_t token_size, Pair *pair) {
    return 1;
}

int
parse_sexp (char *token, size_t token_size, SExp *exp) {
    Pair *pair = (Pair*)(malloc(sizeof(Pair)));
    Atom *atom = (Atom*)(malloc(sizeof(Atom)));
    if (token[0] == '(') {
        token = next_token();
        if (token == NULL)
            return 1;
        token_size = strlen(token);
        if (token[0] == ')') {
            exp->type = SEXP_TYPE_NIL;
            return 0;
        } else if (parse_pair(token, token_size, pair) == 0) {
            exp->type = SEXP_TYPE_PAIR;
            exp->pair = pair;
            free(atom);
            return 0;
        }
    } else if (parse_atom(token, token_size, atom) == 0) {
        exp->type = SEXP_TYPE_ATOM;
        exp->atom = atom;
        free(pair);
        return 0;
    }
    return 1;
}

SExp *
parse_program (char *program_txt) {
    char *token;
    size_t token_size;
    SExp *program;
    SExp curr_exp;
    int program_idx;

    program = (SExp*)(malloc(1024 * sizeof(SExp)));
    if (program == NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    program_idx = 0;
    
    token = tokenize(program_txt);
    while (token != NULL) {
        if (isspace(token[0])) {
            token = next_token();
            continue;
        }

        token_size = strlen(token);
        curr_exp = (SExp){0};

        if (!parse_sexp(token, token_size, &curr_exp)) {
            program[program_idx++] = curr_exp;
        } else {
            printf("Unknown token %s\n", token);
            return NULL;
        }
        token = next_token();
    }

    return program;
}
