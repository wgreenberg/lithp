#define MAX_STRING_SIZE 1024

typedef enum {
    ATOM_TYPE_NUMBER,
    ATOM_TYPE_BOOLEAN,
    ATOM_TYPE_CHARACTER,
    ATOM_TYPE_STRING,
    ATOM_TYPE_SYMBOL,
} AtomType;

typedef struct Atom {
    AtomType type;
    union {
        long int number_value;
        char character_value;
        char string_value[MAX_STRING_SIZE];
    };
} Atom;

typedef enum {
    SEXP_TYPE_ATOM,
    SEXP_TYPE_PAIR,
    SEXP_TYPE_NIL,
} SExpType;

typedef struct SExp {
    SExpType type;
    union {
        struct Atom* atom;
        struct Pair* pair;
    };
} SExp;

typedef struct Pair {
    SExp *car;
    SExp *cdr;
} Pair;

SExp NIL = { SEXP_TYPE_NIL };
Atom _true_atom = { ATOM_TYPE_BOOLEAN, { 1 } };
SExp TRUE = { SEXP_TYPE_ATOM, { &_true_atom } };
Atom _false_atom = { ATOM_TYPE_BOOLEAN, { 0 } };
SExp FALSE = { SEXP_TYPE_ATOM, { &_false_atom } };

SExp * new_sexp ();
Pair * new_pair ();
Atom * new_atom ();
SExp * new_symbol (const char* symbol_string);
SExp * car (SExp *exp);
SExp * cdr (SExp *exp);
SExp * cons (SExp *car, SExp *cdr);

int is_eq (SExp *a, SExp *b);

#define caar(obj)   car(car(obj))
#define cadr(obj)   car(cdr(obj))
#define cdar(obj)   cdr(car(obj))
#define cddr(obj)   cdr(cdr(obj))
#define caaar(obj)  car(car(car(obj)))
#define caadr(obj)  car(car(cdr(obj)))
#define cadar(obj)  car(cdr(car(obj)))
#define caddr(obj)  car(cdr(cdr(obj)))
#define cdaar(obj)  cdr(car(car(obj)))
#define cdadr(obj)  cdr(car(cdr(obj)))
#define cddar(obj)  cdr(cdr(car(obj)))
#define cdddr(obj)  cdr(cdr(cdr(obj)))
#define caaaar(obj) car(car(car(car(obj))))
#define caaadr(obj) car(car(car(cdr(obj))))
#define caadar(obj) car(car(cdr(car(obj))))
#define caaddr(obj) car(car(cdr(cdr(obj))))
#define cadaar(obj) car(cdr(car(car(obj))))
#define cadadr(obj) car(cdr(car(cdr(obj))))
#define caddar(obj) car(cdr(cdr(car(obj))))
#define cadddr(obj) car(cdr(cdr(cdr(obj))))
#define cdaaar(obj) cdr(car(car(car(obj))))
#define cdaadr(obj) cdr(car(car(cdr(obj))))
#define cdadar(obj) cdr(car(cdr(car(obj))))
#define cdaddr(obj) cdr(car(cdr(cdr(obj))))
#define cddaar(obj) cdr(cdr(car(car(obj))))
#define cddadr(obj) cdr(cdr(car(cdr(obj))))
#define cdddar(obj) cdr(cdr(cdr(car(obj))))
#define cddddr(obj) cdr(cdr(cdr(cdr(obj))))

// PARSE

SExp * parser__parse_program (char *program_txt);

int parser__is_symbol_token (char *token, size_t token_size);
int parser__is_number_token (char *token);
char parser__token_to_character (char *token, size_t token_size);
int parser__is_nil_token (char *token, size_t token_size);

int parser__parse_atom (char *token, size_t token_size, Atom *atom);
int parser__parse_pair (char *token, size_t token_size, Pair *pair);
int parser__parse_sexp (char *token, size_t token_size, SExp *exp);

// EVAL
SExp * eval (SExp *exp, SExp *env);

void print (SExp *exp);
