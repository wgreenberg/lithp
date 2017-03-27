#define MAX_STRING_SIZE 1024

typedef enum {
    ATOM_TYPE_NUMBER,
    ATOM_TYPE_BOOLEAN,
    ATOM_TYPE_CHARACTER,
    ATOM_TYPE_STRING,
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
    SExp cons;
    struct Pair* cdr;
} Pair;
