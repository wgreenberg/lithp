#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "parse.h"

int
is_number (char *token) {
    char *endptr;
    strtol(token, &endptr, 0);
    return (*endptr == 0);
}

int
is_character (char *token, size_t token_size) {
    if (token_size == 3) {
        return (token[0] == '#' && token[1] == '\\');
    } else if (token_size > 3) {
        return (strcmp(token, "#\\newline") == 0 || strcmp(token, "#\\space") == 0);
    }
    return 0;
}

int
is_boolean (char *token, size_t token_size) {
    if (token_size == 2) {
        return (token[0] == '#') && (token[1] == 't' || token[1] == 'f');
    }
    return 0;
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
}

int
is_string_start (char *token) {
    return token[0] == '"';
}

int
is_string_end (char *token, size_t token_size) {
    return token[token_size - 1] == '"';
}

int
is_nil (char *token, size_t token_size) {
    if (token_size == 2) {
        return token[0] == '(' && token[1] == ')';
    }
    return 0;
}

char *
_next_token () {
    return strtok(NULL, "\n ");
}

atom *
parse_atom (char *token, size_t token_size) {
    struct atom *atom;
    int string_buf_idx;
    int token_buf_idx;
    int string_terminated;

    atom = (struct atom*)(malloc(sizeof(atom)));

    if (is_number(token)) {
        atom->type = NUMBER;
        atom->number_value = strtol(token, NULL, 0);
    } else if (is_boolean(token, token_size)) {
        atom->type = BOOLEAN;
        atom->number_value = (token[1] == 't');
    } else if (is_character(token, token_size)) {
        atom->type = CHARACTER;
        atom->character_value = to_character(token, token_size);
    } else if (is_string_start(token)) {
        atom->type = STRING;
        
        string_buf_idx = 0;
        string_terminated = 0;
        while (token != NULL) {
            token_size = strlen(token);
            int token_buf_start = 0;
            if (is_string_start(token)) {
                token_buf_start++;
            }

            int token_buf_end = token_size;
            if (is_string_end(token, token_size)) {
                token_buf_end--;
                string_terminated = 1;
            }

            for (token_buf_idx=token_buf_start; token_buf_idx < token_buf_end; token_buf_idx++) {
                char c = token[token_buf_idx];
                if (c == '"') {
                    printf("Can't terminate quote here: %s\n", token);
                    exit(1);
                }
                if (c == '\\') {
                    token_buf_idx++;
                    if (token_buf_idx == token_buf_end) {
                        printf("Can't end string with escape character\n");
                        exit(1);
                    }
                    c = token[token_buf_idx];
                    if (c == 'n') {
                        atom->string_value[string_buf_idx++] = '\n';
                        continue;
                    }
                }
                atom->string_value[string_buf_idx++] = c;
            }

            if (string_terminated) {
                atom->string_value[string_buf_idx] = '\0';
                break;
            } else {
                atom->string_value[string_buf_idx++] = ' ';
                token = _next_token();
            }
        }

        if (!string_terminated) {
            printf("Unterminated string\n");
            exit(1);
        }
    } else {
        atom = NULL;
    }
    return atom;
}

int
is_pair_start (char *token) {
    return token[0] == '(';
}

int
is_pair_end (char *token, size_t token_size) {
    return token[token_size - 1] == ')';
}

pair *
parse_pair (char *token, size_t token_size) {
    return NULL;
}

int
parse_sexp (char *token, size_t token_size, sexp *exp) {
    if (is_nil(token, token_size)) {
        exp->type = NIL;
        return 0;
    } else if (exp->pair = parse_pair(token, token_size)) {
        exp->type = PAIR;
        return 0;
    } else if (exp->atom = parse_atom(token, token_size)) {
        exp->type = ATOM;
        return 0;
    }
    return 1;
}

sexp *
parse_program (char *program_txt) {
    const char delim[2] = "\n ";
    char *token;
    size_t token_size;
    sexp *program;
    sexp curr_exp;
    int program_idx;

    program = (sexp*)(malloc(1024 * sizeof(sexp)));
    if (program == NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    program_idx = 0;
    
    token = strtok(program_txt, delim);
    while (token != NULL) {
        token_size = strlen(token);
        curr_exp = (sexp){0};
        if (!parse_sexp(token, token_size, &curr_exp)) {
            program[program_idx++] = curr_exp;
        } else {
            printf("Unknown token %s\n", token);
            exit(1);
        }
        token = strtok(NULL, delim);
    }

    return program;
}
