#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

#include "lithp.h"

// PARSER

int
parser__is_symbol_token (char *token, size_t token_size) {
    regex_t re;
    int ret;
    ret = regcomp(&re, "[_a-zA-Z!0&*/:<=>?^][_a-zA-Z!0&*/:<=>?^0-9.+-]*", REG_EXTENDED|REG_NOSUB);
    if (ret != 0) {
        printf("Error comiling symbol regex\n");
        exit(1);
    }
    ret = regexec(&re, token, 0, NULL, 0);
    if (ret == 0)
        return 1;
    return 0;
}

int
parser__is_number_token (char *token) {
    char *endptr;
    strtol(token, &endptr, 0);
    return (*endptr == 0);
}

char
parser__token_to_character (char *token, size_t token_size) {
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
parser__is_nil_token (char *token, size_t token_size) {
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

const char* delim = " ()\n\"\\\0";
int n_delim = 7;
int
_next_token (char *buf) {
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

char *
tokenize (char *string) {
    _t_idx = 0;
    _t_orig_len = strlen(string);
    _t_orig = string;
    if (_t_orig_len == 0)
        return NULL;

    return next_token();
}

int
is_delim (char c) {
    int i;
    for (i=0; i<n_delim; i++) {
        if (delim[i] == c)
            return 1;
    }
    return 0;
}

void
consume_whitespace () {
    if (_t_idx >= _t_orig_len)
        return;
    _t_idx += strspn(_t_orig + _t_idx, " \n");
}

int
parser__parse_atom (char *token, size_t token_size, Atom *atom) {
    int string_buf_idx;
    int token_buf_idx;
    int string_terminated;
    int token_buf_start;
    int is_escaped;

    if (parser__is_number_token(token)) {
        atom->type = ATOM_TYPE_NUMBER;
        atom->number_value = strtol(token, NULL, 0);
        return 0;
    } else if (token[0] == '#') {
        // If we've only got the # token, the next character must be an escaped
        // sequence. Otherwise, assume it's either #t or #f
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
    } else {
        if (parser__is_symbol_token(token, token_size)) {
            atom->type = ATOM_TYPE_SYMBOL;
            strncpy(atom->string_value, token, token_size);
            return 0;
        }
    }
    return 1;
}

int
parser__parse_pair (char *token, size_t token_size, Pair *pair) {
    // parse car
    pair->car = (SExp*)(malloc(sizeof(SExp)));
    if (parser__parse_sexp(token, token_size, pair->car)) {
        return 1;
    }

    consume_whitespace();

    // handle NIL cdr
    pair->cdr = (SExp*)(malloc(sizeof(SExp)));
    token = peek_next_token();
    if (token == NULL) {
        return 1;
    } else if (token[0] == ')') {
        pair->cdr->type = SEXP_TYPE_NIL;
        return 0;
    }

    // parse cdr
    token = next_token();
    token_size = strlen(token);
    pair->cdr->pair = (Pair*)(malloc(sizeof(Pair)));
    pair->cdr->type = SEXP_TYPE_PAIR;
    if (parser__parse_pair(token, token_size, pair->cdr->pair)) {
        return 1;
    } else {
        return 0;
    }
}

int
parser__parse_sexp (char *token, size_t token_size, SExp *exp) {
    if (token[0] == '\'') {
        if (token == NULL || exp->is_quoted == 1)
            return 1;
        if (token_size == 1) {
            token = next_token();
            if (token == NULL)
                return 1;
            token_size = strlen(token);
        } else {
            token++;
            token_size--;
        }
        exp->is_quoted = 1;
        return parser__parse_sexp(token, token_size, exp);
    } 

    if (token[0] == '(') {
        token = next_token();
        if (token == NULL)
            return 1;
        token_size = strlen(token);

        if (token[0] == ')') {
            exp->type = SEXP_TYPE_NIL;
            return 0;
        }

        Pair *pair = (Pair*)(malloc(sizeof(Pair)));
        if (parser__parse_pair(token, token_size, pair) == 0) {
            token = next_token();
            if (token == NULL || token[0] != ')') {
                printf("Unclosed parenthesis\n");
                return 1;
            }
            exp->type = SEXP_TYPE_PAIR;
            exp->pair = pair;
            return 0;
        }

        free(pair);
        return 1;
    }

    Atom *atom = (Atom*)(malloc(sizeof(Atom)));
    if (parser__parse_atom(token, token_size, atom) == 0) {
        exp->type = SEXP_TYPE_ATOM;
        exp->atom = atom;
        return 0;
    }

    free(atom);
    return 1;
}

SExp *
parser__parse_program (char *program_txt) {
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

        if (!parser__parse_sexp(token, token_size, &curr_exp)) {
            program[program_idx++] = curr_exp;
        } else {
            printf("Unknown token %s\n", token);
            return NULL;
        }
        token = next_token();
    }

    return program;
}

// EVALUATOR
// These functions are largely modeled after SICP's metacircular evaluator

int is_atom (SExp *exp) { return (exp->type == SEXP_TYPE_ATOM); }
int is_pair (SExp *exp) { return (exp->type == SEXP_TYPE_PAIR); }
int is_nil (SExp *exp) { return (exp->type == SEXP_TYPE_NIL); }

SExp * car (SExp *exp) { return is_pair(exp) ? exp->pair->car : &NIL; }
SExp * cdr (SExp *exp) { return is_pair(exp) ? exp->pair->cdr : &NIL; }

int is_number (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_NUMBER); }
int is_string (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_STRING); }
int is_symbol (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_SYMBOL); }

int is_self_evaluating (SExp *exp) { return is_number(exp) || is_string(exp); }

int is_tagged_list (SExp *exp, const char *tag) {
    return is_pair(exp)
        && is_symbol(exp->pair->car)
        && (strcmp(tag, exp->pair->car->atom->string_value) == 0);
}
int is_quoted (SExp *exp) { return exp->is_quoted; }
int is_variable (SExp *exp) { return is_symbol(exp) && !is_quoted(exp); }

int is_assignment (SExp *exp) { return is_tagged_list(exp, "set!"); }
SExp * assignment_variable (SExp *exp) { return cadr(exp); }
SExp * assignment_value (SExp *exp) { return caddr(exp); }

int is_definition (SExp *exp) { return is_tagged_list(exp, "define"); }
// doesn't yet support lambda sugar
SExp * definition_variable (SExp *exp) { return cadr(exp); }
SExp * definition_value (SExp *exp) { return caddr(exp); }

SExp * cons (SExp *car, SExp *cdr) {
    SExp *ret = (SExp*)(malloc(sizeof(SExp)));
    Pair *pair = (Pair*)(malloc(sizeof(Pair)));
    ret->type = SEXP_TYPE_PAIR;
    ret->pair = pair;
    ret->pair->car = car;
    ret->pair->cdr = cdr;
    return ret;
}

SExp *
lookup_variable_value (SExp *var, SExp *env) {
    return &NIL;
}

SExp *
extend_environment (SExp *variables, SExp *values, SExp *base_env) {
    return &NIL;
}

void
set_variable (SExp *var, SExp *val, SExp *env) {
}

void
define_variable (SExp *var, SExp *val, SExp *env) {
}

SExp *
eval_assignment (SExp *exp, SExp *env) {
    set_variable(assignment_variable(exp), assignment_value(exp), env);
    return &NIL;
}

SExp *
eval_definition (SExp *exp, SExp *env) {
    define_variable(definition_variable(exp), definition_value(exp), env);
    return &NIL;
}

SExp *
eval (SExp *exp, SExp *env) {
    /*
    cond ((self-evaluating? exp) exp)
         ((variable? exp) (lookup-variable-value exp env))
         ((quoted? exp) (text-of-quotation exp))
         ((assignment? exp) (eval-assignment exp env))
         ((definition? exp) (eval-definition exp env))
    */
    if (is_self_evaluating(exp)) return exp;
    if (is_variable(exp)) return lookup_variable_value(exp, env);
    if (is_quoted(exp)) return exp; // what's text-of-quotation
    if (is_assignment(exp)) return eval_assignment(exp, env);
    if (is_definition(exp)) return eval_definition(exp, env);
    return &NIL;
}

// MAIN

void
print (SExp *exp) {
    if (exp->is_quoted == 1) {
        printf("'");
    }
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
            printf("\"%s\"", a->string_value);
        } else if (a->type == ATOM_TYPE_SYMBOL) {
            printf("%s", a->string_value);
        } else {
            printf("ERR: Unable to print invalid sexp");
        }
    } else if (exp->type == SEXP_TYPE_PAIR) {
        printf("(");
        print(exp->pair->car);
        printf(" ");
        print(exp->pair->cdr);
        printf(")");
    } else if (exp->type == SEXP_TYPE_NIL) {
        printf("()");
    } else {
        printf("ERR: Unable to print invalid sexp");
    }
}

int main (void) {
    printf("Welcome to Lithp\n");
    size_t buf_size = 1024;
    char *buf = malloc(buf_size);

    SExp *global_env = &NIL;
    while (1) {
        // read
        printf("> ");
        getline(&buf, &buf_size, stdin);
        if (0) {
            char * token = tokenize(buf);
            while (token != NULL) {
                consume_whitespace();
                printf("%s\n", token);
                token = next_token();
            }
        } else {
            // evaluate
            SExp *program = parser__parse_program(buf);
            if (program == NULL)
                continue;
            SExp *result = eval(program, global_env);
            if (result == NULL)
                continue;

            // print
            print(result);
            printf("\n");
        }
    }
    free(buf);
}