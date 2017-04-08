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
    int is_quoted;
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

SExp * parse_program (char *program_txt);

int parse_sexp (char *token, size_t token_size, SExp *exp);
int parse_pair (char *token, size_t token_size, Pair *pair);
int parse_atom (char *token, size_t token_size, Atom *atom);

char * tokenize (char *string);
char * next_token ();
char * peek_next_token ();
void consume_whitespace ();
