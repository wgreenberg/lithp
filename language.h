#define MAX_STRING_SIZE 1024

typedef enum {
    NUMBER,
    BOOLEAN,
    CHARACTER,
    STRING,
} atom_type;

typedef struct atom {
    atom_type type;
    union {
        long int number_value;
        char character_value;
        char string_value[MAX_STRING_SIZE]; // this is really wasteful isn't it
    };
} atom;

typedef enum {
    ATOM,
    PAIR,
    NIL,
} sexp_type;

typedef struct sexp {
    sexp_type type;
    union {
        struct atom* atom;
        struct pair* pair;
    };
} sexp;

typedef struct pair {
    sexp cons;
    struct pair* cdr;
} pair;
