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

// I think this is actually wrong, since in SICP it's implied that the only
// false value is the singleton FALSE expression
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

SExp * car (SExp *exp) {
    if (!is_pair(exp)) {
        printf("Tried to apply car to non-pair: ");
        print(exp);
        printf("\n");
        exit(1);
    }
    return exp->pair->car;
}

SExp * cdr (SExp *exp) {
    if (!is_pair(exp)) {
        printf("Tried to apply cdr to non-pair: ");
        print(exp);
        printf("\n");
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
    if (!is_pair(list)) {
        printf("ERR: length must be applied to a pair\n");
        return &NIL;
    }
    return new_number(length(list));
}

typedef long int (*num_reducer)(long int acc, long int next);

SExp *
num_reducer_proc (SExp *arguments, num_reducer fn, long int init) {
    long int result = init;
    while (!is_nil(arguments)) {
        if (!is_number(car(arguments))) {
            printf("ERR: Unexpected non-numeric value ");
            print(car(arguments));
            printf("\n");
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
            printf("ERR: Unexpected non-numeric value ");
            print(car(arguments));
            printf("\n");
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
    printf("Unbound variable: ");
    print(var);
    printf("\n");
    exit(1);
}

SExp *
extend_environment (SExp *vars, SExp *vals, SExp *base_env) {
    if (length(vars) != length(vals)) {
        printf("Variables and values must be equal in length: \n");
        print(vars);
        printf("\n");
        print(vals);
        printf("\n");
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
    printf("Unable to set unbound variable ");
    print(var);
    printf("\n");
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
eval_if (SExp *exp, SExp *env) {
    SExp *predicate = cadr(exp);
    SExp *consequent = caddr(exp);
    SExp *alternative = is_nil(cdddr(exp)) ? &FALSE : cadddr(exp);
    if (is_true(eval(predicate, env))) {
        return eval(consequent, env);
    } else {
        return eval(alternative, env);
    }
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
    printf("Unknown procedure type in apply: ");
    print(procedure);
    printf("\n");
    exit(1);
}

SExp *
eval (SExp *exp, SExp *env) {
    if (is_self_evaluating(exp)) return exp;
    if (is_variable(exp)) return lookup_variable_value(exp, env);
    if (is_quoted(exp)) return cadr(exp); // (quote (exp ()))
    if (is_assignment(exp)) return eval_assignment(exp, env);
    if (is_definition(exp)) return eval_definition(exp, env);
    if (is_if(exp)) return eval_if(exp, env);
    if (is_and(exp) || is_or(exp)) return eval(bool_to_if(exp), env);
    if (is_lambda(exp)) return make_procedure(exp, env);
    if (is_let(exp)) return eval(let_to_lambda(exp), env);
    if (is_begin(exp)) return eval_sequence(cdr(exp), env);
    if (is_cond(exp)) return eval(cond_to_if(exp), env);
    if (is_application(exp)) return apply(eval(car(exp), env),
                                          list_of_values(cdr(exp), env));
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
        printf("(compound-procedure ");
        print(params);
        printf(" ");
        print(body);
        printf("'<procedure-env>)");
    } else if (is_pair(exp)) {
        printf("(");
        print(exp->pair->car);
        printf(" ");
        print(exp->pair->cdr);
        printf(")");
    } else if (is_nil(exp)) {
        printf("()");
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

void
init_primitive_procs (SExp *global_env) {
    define_variable(new_symbol("length"), new_primitive_proc(length_proc), global_env);
    define_variable(new_symbol("cons"), new_primitive_proc(cons_proc), global_env);
    define_variable(new_symbol("car"), new_primitive_proc(car_proc), global_env);
    define_variable(new_symbol("cdr"), new_primitive_proc(cdr_proc), global_env);
    define_variable(new_symbol("set-car!"), new_primitive_proc(set_car_proc), global_env);
    define_variable(new_symbol("set-cdr!"), new_primitive_proc(set_cdr_proc), global_env);
    define_variable(new_symbol("list"), new_primitive_proc(list_proc), global_env);

    define_variable(new_symbol("+"), new_primitive_proc(add_proc), global_env);
    define_variable(new_symbol("*"), new_primitive_proc(mult_proc), global_env);
    define_variable(new_symbol("="), new_primitive_proc(num_eq_proc), global_env);
    define_variable(new_symbol("<"), new_primitive_proc(lt_proc), global_env);
    define_variable(new_symbol("<="), new_primitive_proc(lte_proc), global_env);
    define_variable(new_symbol(">"), new_primitive_proc(gt_proc), global_env);
    define_variable(new_symbol(">="), new_primitive_proc(gte_proc), global_env);

    define_variable(new_symbol("null?"), new_primitive_proc(nil_proc), global_env);
    define_variable(new_symbol("boolean?"), new_primitive_proc(boolean_proc), global_env);
    define_variable(new_symbol("symbol?"), new_primitive_proc(symbol_proc), global_env);
    define_variable(new_symbol("integer?"), new_primitive_proc(number_proc), global_env);
    define_variable(new_symbol("character?"), new_primitive_proc(character_proc), global_env);
    define_variable(new_symbol("pair?"), new_primitive_proc(pair_proc), global_env);
    define_variable(new_symbol("string?"), new_primitive_proc(string_proc), global_env);
    define_variable(new_symbol("procedure?"), new_primitive_proc(primitive_procedure_proc), global_env);
    define_variable(new_symbol("eq?"), new_primitive_proc(poly_eq_proc), global_env);
}

int main (void) {
    printf("Welcome to Lithp\n");
    size_t buf_size = 1024;
    char *buf = malloc(buf_size);

    SExp *global_env = new_env();
    init_primitive_procs(global_env);
    SExp *symbol_table = new_symbol_table(global_env);
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
            symbol_table = build_symbol_table(program, symbol_table);
            program = prune_symbols(program, symbol_table);
            SExp *result = eval(program, global_env);
            if (result == NULL)
                continue;
            symbol_table = build_symbol_table(result, symbol_table);

            // print
            print(result);
            printf("\n");
        }
    }
    free(buf);
}
