#define MAX_STRING_SIZE 1024

typedef enum {
    NUMBER,
    BOOLEAN,
    CHARACTER,
    STRING,
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
    ATOM,
    PAIR,
    NIL,
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
