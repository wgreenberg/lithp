#include <stdio.h>
#include <unistd.h>
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
    ret = regcomp(&re, "[_a-zA-Z!0&*/:<=>+?^][_a-zA-Z!0&*/:<=>?^0-9.+-]*", REG_EXTENDED|REG_NOSUB);
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

FILE *_t_in;
char _t_buf[MAX_STRING_SIZE];
char _t_peek_buf[MAX_STRING_SIZE];

const char* delim = " ()\n\"\\\0";
int n_delim = 7;

void
init_parser (FILE *in) {
    _t_in = in;
}

int
_next_token (char *buf) {
    int i;
    char c;

    i = 0;
    while (1) {
        if (i >= MAX_STRING_SIZE) {
            printf("ran out of token buffer space\n");
            exit(1);
        }

        c = getc(_t_in);

        if (c == EOF)
            break;

        if (is_delim(c)) {
            if(i == 0) {
                buf[i++] = c;
                break;
            } else {
                ungetc(c, _t_in);
                break;
            }
        }
        buf[i++] = c;
    }

    buf[i] = '\0';
    return i;
}

char *
peek_next_token () {
    int buf_len = _next_token(_t_peek_buf);

    if (buf_len == 0)
        return NULL;

    while (buf_len > 0) {
        ungetc(_t_peek_buf[--buf_len], _t_in);
    }

    return _t_peek_buf;
}

char *
next_token () {
    int buf_len = _next_token(_t_buf);
    if (buf_len == 0)
        return NULL;
    return _t_buf;
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
    char c;
    c = getc(_t_in);
    while (c == ' ' || c == '\n') c = getc(_t_in);
    ungetc(c, _t_in);
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
    pair->car = new_sexp();
    if (parser__parse_sexp(token, token_size, pair->car)) {
        return 1;
    }

    consume_whitespace();

    // handle NIL cdr
    pair->cdr = new_sexp();
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
    pair->cdr->pair = new_pair();
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
        if (token_size == 1) {
            token = next_token();
            if (token == NULL)
                return 1;
            token_size = strlen(token);
        } else {
            token++;
            token_size--;
        }
        exp->type = SEXP_TYPE_PAIR;
        exp->pair = new_pair();
        exp->pair->car = new_symbol("quote");
        exp->pair->cdr = cons(new_sexp(), &NIL);
        return parser__parse_sexp(token, token_size, cadr(exp));
    }

    if (token[0] == '(') {
        consume_whitespace();
        token = next_token();
        if (token == NULL)
            return 1;
        token_size = strlen(token);

        if (token[0] == ')') {
            exp->type = SEXP_TYPE_NIL;
            return 0;
        }

        Pair *pair = new_pair();
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

    Atom *atom = new_atom();
    if (parser__parse_atom(token, token_size, atom) == 0) {
        exp->type = SEXP_TYPE_ATOM;
        exp->atom = atom;
        return 0;
    }

    free(atom);
    return 1;
}

SExp *
parser__parse_program (FILE *in, int is_repl) {
    char *token;
    size_t token_size;
    SExp *program, *ret;
    SExp *curr_exp;

    program = &NIL;

    init_parser(in);
    token = next_token();
    while (token != NULL) {
        if (isspace(token[0])) {
            token = next_token();
            continue;
        }

        token_size = strlen(token);
        curr_exp = new_sexp();

        if (!parser__parse_sexp(token, token_size, curr_exp)) {
            // this builds a list of the expressions from last to first
            program = cons(curr_exp, program);

            // if we're running in the context of a REPL, just return after the
            // first expression is parsed
            if (is_repl) break;
        } else {
            printf("Unknown token %s\n", token);
            return NULL;
        }
        token = next_token();
    }

    // we want to return cons('begin, reverse(program))
    ret = &NIL;
    while (!is_nil(program)) {
        ret = cons(car(program), ret);
        program = cdr(program);
    }
    ret = cons(new_symbol("begin"), ret);

    return ret;
}

// EVALUATOR
// These functions are largely modeled after SICP's metacircular evaluator

SExp *
new_sexp () {
    return (SExp*)(malloc(sizeof(SExp)));
}

Atom *
new_atom () {
    return (Atom*)(malloc(sizeof(Atom)));
}

Pair *
new_pair () {
    return (Pair*)(malloc(sizeof(Pair)));
}

SExp *
new_symbol (const char* symbol_string) {
    SExp *ret = new_sexp();
    ret->type = SEXP_TYPE_ATOM;
    ret->atom = new_atom();
    ret->atom->type = ATOM_TYPE_SYMBOL;
    strcpy(ret->atom->string_value, symbol_string);
    return ret;
}

SExp *
new_number (long int value) {
    SExp *ret = new_sexp();
    ret->type = SEXP_TYPE_ATOM;
    ret->atom = new_atom();
    ret->atom->type = ATOM_TYPE_NUMBER;
    ret->atom->number_value = value;
    return ret;
}

SExp *
new_boolean (int value) {
    return value ? &TRUE : &FALSE;
}

SExp *
new_primitive_proc (Proc proc) {
    SExp *ret = new_sexp();
    ret->type = SEXP_TYPE_PRIMITIVE_PROC;
    ret->proc = proc;
    return ret;
}

int is_atom (SExp *exp) { return (exp->type == SEXP_TYPE_ATOM); }
int is_pair (SExp *exp) { return (exp->type == SEXP_TYPE_PAIR); }
int is_nil (SExp *exp) { return (exp->type == SEXP_TYPE_NIL); }
int is_number (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_NUMBER); }
int is_string (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_STRING); }
int is_symbol (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_SYMBOL); }
int is_boolean (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_BOOLEAN); }
int is_character (SExp *exp) { return is_atom(exp) && (exp->atom->type == ATOM_TYPE_CHARACTER); }
int is_self_evaluating (SExp *exp) { return is_number(exp) || is_string(exp) || is_boolean(exp); }
int is_tagged_list (SExp *exp, const char *tag) {
    return is_pair(exp)
        && is_symbol(exp->pair->car)
        && (strcmp(tag, exp->pair->car->atom->string_value) == 0);
}
int is_quoted (SExp *exp) { return is_tagged_list(exp, "quote"); }
int is_variable (SExp *exp) { return is_symbol(exp) && !is_quoted(exp); }
int is_assignment (SExp *exp) { return is_tagged_list(exp, "set!"); }
int is_definition (SExp *exp) { return is_tagged_list(exp, "define"); }

int is_false (SExp *exp) { return is_boolean(exp) && exp->atom->number_value == 0; }
int is_true (SExp *exp) { return !is_false(exp); }

int is_if (SExp *exp) { return is_tagged_list(exp, "if"); }
int is_application (SExp *exp) { return is_pair(exp); }
int is_primitive_procedure (SExp *exp) { return exp->type == SEXP_TYPE_PRIMITIVE_PROC; }
int is_compound_procedure (SExp *exp) { return is_tagged_list(exp, "procedure"); }
int is_lambda (SExp *exp) { return is_tagged_list(exp, "lambda"); }
int is_begin (SExp *exp) { return is_tagged_list(exp, "begin"); }
int is_cond (SExp *exp) { return is_tagged_list(exp, "cond"); }
int is_let (SExp *exp) { return is_tagged_list(exp, "let"); }
int is_and (SExp *exp) { return is_tagged_list(exp, "and"); }
int is_or (SExp *exp) { return is_tagged_list(exp, "or"); }

int is_list (SExp *exp) {
    if (is_nil(exp))
        return 1;
    if (!is_pair(exp))
        return 0;
    while (is_pair(exp)) {
        exp = cdr(exp);
    }
    return is_nil(exp);
}

SExp * car (SExp *exp) {
    if (!is_pair(exp)) {
        printf("Tried to apply car to non-pair: "); print(exp); printf("\n");
        exit(1);
    }
    return exp->pair->car;
}

SExp * cdr (SExp *exp) {
    if (!is_pair(exp)) {
        printf("Tried to apply cdr to non-pair: "); print(exp); printf("\n");
        exit(1);
    }
    return exp->pair->cdr;
}

SExp *
cons (SExp *car, SExp *cdr) {
    SExp *ret = new_sexp();
    ret->type = SEXP_TYPE_PAIR;
    ret->pair = new_pair();
    ret->pair->car = car;
    ret->pair->cdr = cdr;
    return ret;
}

int
length (SExp *list) {
    if (is_nil(list)) {
        return 0;
    }
    return 1 + length(cdr(list));
}

SExp *
length_proc (SExp *arguments) {
    if (length(arguments) != 1) {
        printf("ERR: wrong number of arguments to length, expected 1");
        return &NIL;
    }
    SExp *list = car(arguments);
    if (is_pair(list) || is_nil(list)) {
        return new_number(length(list));
    } else {
        printf("ERR: length must be applied to a pair\n");
        return &NIL;
    }
}

SExp *
print_proc (SExp *exp) {
    print(exp); printf("\n");
    return &NIL;
}

typedef long int (*num_reducer)(long int acc, long int next);

SExp *
num_reducer_proc (SExp *arguments, num_reducer fn, long int init) {
    long int result = init;
    while (!is_nil(arguments)) {
        if (!is_number(car(arguments))) {
            printf("ERR: Unexpected non-numeric value "); print(car(arguments)); printf("\n");
            return &NIL;
        }
        result = fn(result, car(arguments)->atom->number_value);
        arguments = cdr(arguments);
    }
    return new_number(result);
}

long int add_reducer (long int acc, long int next) { return acc + next; }
SExp * add_proc (SExp *args) { return num_reducer_proc(args, add_reducer, 0); }

long int mult_reducer (long int acc, long int next) { return acc * next; }
SExp * mult_proc (SExp *args) { return num_reducer_proc(args, mult_reducer, 1); }

SExp *
num_comparator_proc (SExp *arguments, num_reducer fn) {
    if (length(arguments) < 2) {
        printf("ERR: need at least 2 numbers to compare\n");
        return &NIL;
    }
    long int result = 1;
    SExp *a, *b;
    while (!is_nil(cdr(arguments))) {
        a = car(arguments);
        b = cadr(arguments);
        if (!is_number(a) || !is_number(b)) {
            printf("ERR: Unexpected non-numeric value "); print(car(arguments)); printf("\n");
            return &NIL;
        }
        if (!fn(a->atom->number_value, b->atom->number_value)) {
            result = 0;
            break;
        }
        arguments = cdr(arguments);
    }
    return new_boolean(result);
}

long int eq_comparator (long int a, long int b) { return a == b; }
SExp * num_eq_proc (SExp *args) { return num_comparator_proc(args, eq_comparator); }

long int lt_comparator (long int a, long int b) { return a < b; }
SExp * lt_proc (SExp *args) { return num_comparator_proc(args, lt_comparator); }

long int lte_comparator (long int a, long int b) { return a <= b; }
SExp * lte_proc (SExp *args) { return num_comparator_proc(args, lte_comparator); }

long int gt_comparator (long int a, long int b) { return a > b; }
SExp * gt_proc (SExp *args) { return num_comparator_proc(args, gt_comparator); }

long int gte_comparator (long int a, long int b) { return a >= b; }
SExp * gte_proc (SExp *args) { return num_comparator_proc(args, gte_comparator); }

typedef int (*type_predicate) (SExp* exp);

SExp *
type_wrapper(type_predicate fn, SExp *arguments) {
    if (length(arguments) != 1) {
        printf("ERR: Wrong number of arguments to predicate\n");
        return &NIL;
    }
    return new_boolean(fn(car(arguments)));
}

SExp *nil_proc (SExp *args) { return type_wrapper(is_nil, args); }
SExp *boolean_proc (SExp *args) { return type_wrapper(is_boolean, args); }
SExp *symbol_proc (SExp *args) { return type_wrapper(is_symbol, args); }
SExp *number_proc (SExp *args) { return type_wrapper(is_number, args); }
SExp *character_proc (SExp *args) { return type_wrapper(is_character, args); }
SExp *pair_proc (SExp *args) { return type_wrapper(is_pair, args); }
SExp *primitive_procedure_proc (SExp *args) { return type_wrapper(is_primitive_procedure, args); }
SExp *string_proc (SExp *args) { return type_wrapper(is_string, args); }
SExp *is_list_proc (SExp *args) { return type_wrapper(is_list, args); }
SExp *is_finite_proc (SExp *args) { return type_wrapper(is_finite, args); }

SExp *
cons_proc (SExp *args) {
    if (length(args) != 2) {
        printf("ERR: cons requires 2 args\n");
        return &NIL;
    }
    return cons(car(args), cadr(args));
}

SExp *
car_proc (SExp *args) {
    if (length(args) != 1) {
        printf("ERR: car requires 1 arg\n");
        return &NIL;
    }
    return caar(args);
}
SExp *
cdr_proc (SExp *args) {
    if (length(args) != 1) {
        printf("ERR: cdr requires 1 arg\n");
        return &NIL;
    }
    return cdar(args);
}
SExp *
set_car_proc (SExp *args) {
    if (length(args) != 2) {
        printf("ERR: set-car! requires 2 arg\n");
        return &NIL;
    }
    if (!is_pair(car(args))) {
        printf("ERR: invalid first argument to set-car!\n");
        return &NIL;
    }
    car(args)->pair->car = cadr(args);
    return &NIL;
}

SExp *
set_cdr_proc (SExp *args) {
    if (length(args) != 2) {
        printf("ERR: set-cdr! requires 2 arg\n");
        return &NIL;
    }
    if (!is_pair(car(args))) {
        printf("ERR: invalid first argument to set-cdr!\n");
        return &NIL;
    }
    car(args)->pair->cdr = cadr(args);
    return &NIL;
}

SExp *
list_proc (SExp *args) {
    return args;
}

SExp *
poly_eq_proc (SExp *args) {
    if (length(args) != 2) {
        printf("ERR: eq? requires 2 args\n");
        return &NIL;
    }
    SExp *a = car(args);
    SExp *b = cadr(args);

    if (a->type != b->type)
        return &FALSE;

    if (a->type == SEXP_TYPE_ATOM) {
        if (a->atom->type != b->atom->type)
            return &FALSE;
        switch (a->atom->type) {
            case ATOM_TYPE_NUMBER:
            case ATOM_TYPE_BOOLEAN:
                return new_boolean(a->atom->number_value == b->atom->number_value);
            case ATOM_TYPE_STRING:
            case ATOM_TYPE_SYMBOL:
                return new_boolean(strcmp(a->atom->string_value, b->atom->string_value) == 0);
            case ATOM_TYPE_CHARACTER:
                return new_boolean(a->atom->character_value == b->atom->character_value);
        }
    } else if (a->type == SEXP_TYPE_PRIMITIVE_PROC) {
        return new_boolean(a->proc == b->proc);
    } else if (a->type == SEXP_TYPE_PAIR) {
        SExp *car_list = cons(car(a), cons(car(b), &NIL));
        SExp *cdr_list = cons(cdr(a), cons(cdr(b), &NIL));
        if (is_true(poly_eq_proc(car_list))) {
            return poly_eq_proc(cdr_list);
        } else {
            return &FALSE;
        }
    }
    return &TRUE;
}

void
load_and_run (char *filename) {
    SExp *program;
    FILE *in;

    in = fopen(filename, "r");

    if (in == NULL) {
        printf("ERR: Couldn't read file %s\n", filename);
        return;
    }

    program = parser__parse_program(in, 0);

    if (program == NULL) {
        printf("ERR: Parser error for %s\n", filename);
    } else {
        global_symbol_table = build_symbol_table(program, global_symbol_table);
        program = prune_symbols(program, global_symbol_table);
        eval(program, global_env);
    }

    fclose(in);
}

SExp *
load_proc (SExp* args) {
    char *filename;

    if (length(args) != 1 || !is_string(car(args))) {
        printf("ERR: load requires a single filename\n");
        return &NIL;
    }

    filename = car(args)->atom->string_value;
    load_and_run(filename);
    return &NIL;
}

SExp *
apply_primitive_procedure (SExp *procedure, SExp *arguments) {
    return (procedure->proc)(arguments);
}

int
is_eq (SExp *a, SExp *b) {
    return a == b;
}

void
add_binding_to_frame (SExp *var, SExp *val, SExp *frame) {
    frame->pair->car = cons(var, car(frame));
    frame->pair->cdr = cons(val, cdr(frame));
}

SExp *
lookup_variable_value (SExp *var, SExp *env) {
    SExp *frame, *frame_vars, *frame_vals;
    while (!is_nil(env)) {
        frame = car(env);
        frame_vars = car(frame);
        frame_vals = cdr(frame);
        while (!is_nil(frame_vars)) {
            if (is_eq(var, car(frame_vars))) {
                return car(frame_vals);
            }
            frame_vars = cdr(frame_vars);
            frame_vals = cdr(frame_vals);
        }
        env = cdr(env);
    }
    printf("Unbound variable: "); print(var); printf("\n");
    exit(1);
}

SExp *
extend_environment (SExp *vars, SExp *vals, SExp *base_env) {
    if (length(vars) != length(vals)) {
        printf("Variables and values must be equal in length: \n"); print(vars); printf("\n"); print(vals); printf("\n");
        return &NIL;
    }
    return cons(cons(vars, vals), base_env);
}

void
set_variable (SExp *var, SExp *val, SExp *env) {
    SExp *frame, *frame_vars, *frame_vals;
    while (!is_nil(env)) {
        frame = car(env);
        frame_vars = car(frame);
        frame_vals = cdr(frame);
        while (!is_nil(frame_vars)) {
            if (is_eq(var, car(frame_vars))) {
                frame_vals->pair->car = val;
                return;
            }
            frame_vars = cdr(frame_vars);
            frame_vals = cdr(frame_vals);
        }
        env = cdr(env);
    }
    printf("Unable to set unbound variable "); print(var); printf("\n");
}

void
define_variable (SExp *var, SExp *val, SExp *env) {
    SExp *frame, *frame_vars, *frame_vals;
    frame = car(env);
    frame_vars = car(frame);
    frame_vals = cdr(frame);
    while (!is_nil(frame_vars)) {
        if (is_eq(var, car(frame_vars))) {
            frame_vals->pair->car = val;
            return;
        } else {
            frame_vars = cdr(frame_vars);
            frame_vals = cdr(frame_vals);
        }
    }
    add_binding_to_frame(var, val, frame);
}

SExp *
eval_assignment (SExp *exp, SExp *env) {
    SExp *variable = cadr(exp);
    SExp *value = caddr(exp);
    set_variable(variable, eval(value, env), env);
    return new_symbol("ok");
}

SExp *
definition_variable (SExp *exp) {
    if (is_symbol(cadr(exp))) {
        return cadr(exp);
    } else {
        return caadr(exp);
    }
}

SExp *
definition_value (SExp *exp) {
    if (is_symbol(cadr(exp))) {
        return caddr(exp);
    } else {
        SExp *params = cdadr(exp);
        SExp *body = cddr(exp);
        SExp *lambda_body = cons(params, body);
        return cons(new_symbol("lambda"), lambda_body);
    }
}

SExp *
eval_definition (SExp *exp, SExp *env) {
    define_variable(definition_variable(exp), eval(definition_value(exp), env), env);
    return new_symbol("ok");
}

SExp *
list_of_values (SExp *exp, SExp *env) {
    if (is_nil(exp)) {
        return &NIL;
    }
    return cons(eval(car(exp), env), list_of_values(cdr(exp), env));
}

SExp *
eval_sequence (SExp *seq, SExp *env) {
    SExp *ret;
    while (!is_nil(seq)) {
        ret = eval(car(seq), env);
        seq = cdr(seq);
    }
    return ret;
}

SExp *
make_procedure (SExp *exp, SExp *env) {
    SExp *params = cadr(exp);
    SExp *body = cddr(exp);
    return cons(new_symbol("procedure"), cons(params, cons(body, cons(env, &NIL))));
}

SExp *
sequence_to_exp (SExp *seq) {
    if (is_nil(seq))
        return seq;
    if (is_nil(cdr(seq)))
        return car(seq);
    return cons(new_symbol("begin"), seq);
}

SExp *
make_if (SExp *predicate, SExp *consequent, SExp *alternative) {
    if (alternative == NULL) {
        return cons(new_symbol("if"), cons(predicate, cons(consequent, &NIL)));
    } else {
        return cons(new_symbol("if"), cons(predicate, cons(consequent, cons(alternative, &NIL))));
    }
}

SExp *
expand_clauses (SExp *clauses) {
    if (is_nil(clauses))
        return &FALSE;
    SExp *first = car(clauses);
    SExp *rest = cdr(clauses);
    if (is_tagged_list(first, "else")) {
        if (is_nil(rest)) {
            return sequence_to_exp(cdr(first));
        } else {
            printf("ERR: else clause isn't last in cond clauses\n");
            return &NIL;
        }
    } else {
        SExp *predicate = car(first);
        SExp *consequent = sequence_to_exp(cdr(first));
        SExp *alternative = expand_clauses(rest);
        return make_if(predicate, consequent, alternative);
    }
}

SExp *
cond_to_if (SExp *exp) {
    return expand_clauses(cdr(exp));
}

// SExp transform from:
//   (let ((var val) ...) <let_body_seq>)
// to
//   ((lambda (<let vars>) <let_body_seq>) <let_vals>)
SExp *
let_to_lambda (SExp *exp) {
    // (let (<let_env> (<let_body_seq>)))
    SExp *let_env = cadr(exp);
    SExp *let_body_seq = cddr(exp);

    // separate vars from vals from bindings like ((var val) ...)
    SExp *let_vars = &NIL;
    SExp *let_vals = &NIL;
    while (!is_nil(let_env)) {
        let_vars = cons(caar(let_env), let_vars);
        let_vals = cons(cadar(let_env), let_vals);
        let_env = cdr(let_env);
    }

    SExp *lambda_body = cons(let_vars, let_body_seq);
    SExp *lambda_exp = cons(new_symbol("lambda"), lambda_body);
    return cons(lambda_exp, let_vals);
}

// (and a b c) => (if a (if b (if c c)))
SExp *
and_to_if (SExp *exp) {
    if (is_nil(exp))
        return &TRUE;
    SExp *first = car(exp);
    SExp *rest = cdr(exp);
    if (is_nil(rest)) {
        return make_if(first, first, NULL);
    } else {
        return make_if(first, and_to_if(rest), NULL);
    }
}

// (or a b c) => (if a a (if b b (if c c)))
SExp *
or_to_if (SExp *exp) {
    if (is_nil(exp))
        return &FALSE;
    SExp *first = car(exp);
    SExp *rest = cdr(exp);
    return make_if(first, first, or_to_if(rest));
}

SExp *
bool_to_if (SExp *exp) {
    if (is_and(exp))
        return and_to_if(cdr(exp));
    else
        return or_to_if(cdr(exp));
}

SExp *
apply (SExp *procedure, SExp *arguments) {
    if (is_primitive_procedure(procedure)) {
        return apply_primitive_procedure(procedure, arguments);
    } else if (is_compound_procedure(procedure)) {
        SExp *params = cadr(procedure);
        SExp *body = caddr(procedure);
        SExp *env = cadddr(procedure);
        return eval_sequence(body, extend_environment(params, arguments, env));
    }
    printf("Unknown procedure type in apply: "); print(procedure); printf("\n");
    exit(1);
}

#define tail_call(new_exp, new_env) { exp = new_exp; env = new_env; goto eval_begin; }

SExp *
eval (SExp *exp, SExp *env) {
eval_begin:
    if (is_self_evaluating(exp)) return exp;
    if (is_variable(exp)) return lookup_variable_value(exp, env);
    if (is_quoted(exp)) return cadr(exp); // (quote (exp ()))
    if (is_assignment(exp)) return eval_assignment(exp, env);
    if (is_definition(exp)) return eval_definition(exp, env);
    if (is_if(exp)) {
        SExp *predicate = cadr(exp);
        SExp *consequent = caddr(exp);
        SExp *alternative = is_nil(cdddr(exp)) ? &FALSE : cadddr(exp);
        if (is_true(eval(predicate, env))) {
            tail_call(consequent, env);
        } else {
            tail_call(alternative, env);
        }
    }
    if (is_and(exp) || is_or(exp)) tail_call(bool_to_if(exp), env);
    if (is_lambda(exp)) return make_procedure(exp, env);
    if (is_let(exp)) tail_call(let_to_lambda(exp), env);
    if (is_begin(exp)) return eval_sequence(cdr(exp), env);
    if (is_cond(exp)) tail_call(cond_to_if(exp), env);
    if (is_application(exp)) {
        if (is_tagged_list(exp, "interaction-environment")) {
            return env;
        }

        SExp *procedure, *arguments;

        arguments = list_of_values(cdr(exp), env);

        if (is_tagged_list(exp, "apply")) {
            procedure = cadr(exp);
            arguments = cadr(arguments);
            exp = cons(procedure, arguments);
            tail_call(cons(procedure, arguments), env);
        } else if (is_tagged_list(exp, "eval")) {
            tail_call(car(arguments), cadr(arguments));
        } else {
            procedure = eval(car(exp), env);
        }

        return apply(procedure, arguments);
    }
    printf("ERR: Unknown expression type: "); print(exp); printf("\n");
    return &NIL;
}

// MAIN

void
print (SExp *exp) {
    if (is_atom(exp)) {
        Atom *a = exp->atom;
        if (is_number(exp)) {
            printf("%ld", a->number_value);
        } else if (is_boolean(exp)) {
            if (a->number_value)
                printf("#t");
            else
                printf("#f");
        } else if (is_character(exp)) {
            if (a->character_value == ' ') {
                printf("#\\space");
            } else if (a->character_value == '\n') {
                printf("#\\newline");
            } else {
                printf("#\\%c", a->character_value);
            }
        } else if (is_string(exp)) {
            printf("\"%s\"", a->string_value);
        } else if (is_symbol(exp)) {
            printf("%s", a->string_value);
        } else {
            printf("ERR: Unable to print invalid sexp");
        }
    } else if (is_compound_procedure(exp)) {
        SExp *params = cadr(exp);
        SExp *body = caddr(exp);
        printf("(compound-procedure "); print(params); printf(" "); print(body); printf(" '<procedure-env>)");
    } else if (is_nil(exp)) {
        printf("()");
    } else if (is_list(exp)) {
        printf("(");
        while (1) {
            print(car(exp));
            if (is_nil(cdr(exp))) {
                printf(")");
                break;
            } else {
                printf(" ");
            }
            exp = cdr(exp);
        }
    } else if (is_pair(exp)) {
        printf("("); print(exp->pair->car); printf(" . "); print(exp->pair->cdr); printf(")");
    } else if (is_primitive_procedure(exp)) {
        printf("#<primitive>");
    } else {
        printf("ERR: Unable to print invalid sexp");
    }
}

SExp *
new_env () {
    return extend_environment(&NIL, &NIL, &NIL);
}

SExp *
new_symbol_table (SExp *env) {
    return caar(env);
}

SExp *
find_symbol_match (SExp *symbol, SExp *symbol_table) {
    if (is_nil(symbol_table))
        return NULL;
    if (strcmp(symbol->atom->string_value, car(symbol_table)->atom->string_value) == 0)
        return car(symbol_table);
    return find_symbol_match(symbol, cdr(symbol_table));
}

// Build a list of the unique symbols present in a given expression
SExp *
build_symbol_table (SExp *exp, SExp *symbol_table) {
    if (is_symbol(exp)) {
        SExp *existing_symbol = find_symbol_match(exp, symbol_table);
        if (existing_symbol == NULL) {
            return cons(exp, symbol_table);
        }
    } else if (is_pair(exp)) {
        symbol_table = build_symbol_table(car(exp), symbol_table);
        return build_symbol_table(cdr(exp), symbol_table);
    }
    return symbol_table;
}

// Deduplicates any unique symbol pointers from the expression tree
SExp *
prune_symbols (SExp *exp, SExp *symbol_table) {
    if (is_symbol(exp)) {
        SExp *existing_symbol = find_symbol_match(exp, symbol_table);
        if (existing_symbol != NULL) {
            return existing_symbol;
        }
    } else if (is_pair(exp)) {
        return cons(prune_symbols(car(exp), symbol_table), prune_symbols(cdr(exp), symbol_table));
    }
    return exp;
}

SExp *
null_env_proc (SExp *exp) {
    return new_env();
}

SExp *
init_scheme_env () {
    SExp *env = new_env();

    // list functions
    define_variable(new_symbol("length"), new_primitive_proc(length_proc), env);
    define_variable(new_symbol("cons"), new_primitive_proc(cons_proc), env);
    define_variable(new_symbol("car"), new_primitive_proc(car_proc), env);
    define_variable(new_symbol("cdr"), new_primitive_proc(cdr_proc), env);
    define_variable(new_symbol("set-car!"), new_primitive_proc(set_car_proc), env);
    define_variable(new_symbol("set-cdr!"), new_primitive_proc(set_cdr_proc), env);
    define_variable(new_symbol("list"), new_primitive_proc(list_proc), env);

    // integer functions
    define_variable(new_symbol("+"), new_primitive_proc(add_proc), env);
    define_variable(new_symbol("*"), new_primitive_proc(mult_proc), env);
    define_variable(new_symbol("="), new_primitive_proc(num_eq_proc), env);
    define_variable(new_symbol("<"), new_primitive_proc(lt_proc), env);
    define_variable(new_symbol("<="), new_primitive_proc(lte_proc), env);
    define_variable(new_symbol(">"), new_primitive_proc(gt_proc), env);
    define_variable(new_symbol(">="), new_primitive_proc(gte_proc), env);

    // type definition functions
    define_variable(new_symbol("null?"), new_primitive_proc(nil_proc), env);
    define_variable(new_symbol("boolean?"), new_primitive_proc(boolean_proc), env);
    define_variable(new_symbol("symbol?"), new_primitive_proc(symbol_proc), env);
    define_variable(new_symbol("integer?"), new_primitive_proc(number_proc), env);
    define_variable(new_symbol("character?"), new_primitive_proc(character_proc), env);
    define_variable(new_symbol("pair?"), new_primitive_proc(pair_proc), env);
    define_variable(new_symbol("string?"), new_primitive_proc(string_proc), env);
    define_variable(new_symbol("procedure?"), new_primitive_proc(primitive_procedure_proc), env);
    define_variable(new_symbol("eq?"), new_primitive_proc(poly_eq_proc), env);
    define_variable(new_symbol("list?"), new_primitive_proc(is_list_proc), env);

    // apply and eval are special, since we'll use tail call elimination to
    // obviate the need for an actual procedure call
    define_variable(new_symbol("apply"), new_primitive_proc(NULL), env);
    define_variable(new_symbol("eval"), new_primitive_proc(NULL), env);

    // environment functions
    define_variable(new_symbol("null-environment"), new_primitive_proc(null_env_proc), env);
    // interaction-environment requires no proc, since it needs to steal the
    // current env from eval
    define_variable(new_symbol("interaction-environment"), new_primitive_proc(NULL), env);

    // I/O functions
    define_variable(new_symbol("load"), new_primitive_proc(load_proc), env);
    define_variable(new_symbol("print"), new_primitive_proc(print_proc), env);

    return env;
}

void
run_repl () {
    SExp *program, *result;

    printf("Welcome to Lithp\n");
    while (1) {
        printf("> ");
        program = parser__parse_program(stdin, 1);
        if (program == NULL)
            continue;
        global_symbol_table = build_symbol_table(program, global_symbol_table);
        program = prune_symbols(program, global_symbol_table);

        result = eval(program, global_env);
        if (result == NULL) {
            printf("ERR: eval returned null\n");
            exit(1);
        }

        print(result); printf("\n");
    }
}

int main (int n_args, char **argv) {
    global_env = init_scheme_env();
    global_symbol_table = new_symbol_table(global_env);

    if (n_args == 1) {
        run_repl();
    } else {
        load_and_run(argv[1]);
    }
    return 0;
}
