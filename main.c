#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define MAX_STRING_SIZE 1024

typedef enum {
    NUMBER,
    BOOLEAN,
    CHARACTER,
    STRING,
} atom_type;

typedef struct {
    atom_type type;
    union {
        long int number_value;
        char character_value;
        char string_value[MAX_STRING_SIZE]; // this is really wasteful isn't it
    };
} atom;

typedef struct {
    atom atomic_value;
} sexp;

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

sexp *
parse (char *program_txt) {
    const char delim[2] = "\n ";
    char *token;
    char *string_buf;
    int string_buf_idx;
    int token_buf_idx;
    int string_terminated;
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
        if (is_number(token)) {
            curr_exp.atomic_value = (atom){
                type: NUMBER,
                number_value: strtol(token, NULL, 0),
            };
        } else if (is_boolean(token, token_size)) {
            curr_exp.atomic_value = (atom){
                type: BOOLEAN,
                number_value: (token[1] == 't'),
            };
        } else if (is_character(token, token_size)) {
            curr_exp.atomic_value = (atom) {
                type: CHARACTER,
                character_value: to_character(token, token_size),
            };
        } else if (is_string_start(token)) {
            curr_exp.atomic_value = (atom) {
                type: STRING,
            };
            
            string_buf = curr_exp.atomic_value.string_value;
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
                            string_buf[string_buf_idx++] = '\n';
                            continue;
                        }
                    }
                    string_buf[string_buf_idx++] = c;
                }

                if (string_terminated) {
                    string_buf[string_buf_idx] = '\0';
                    break;
                } else {
                    string_buf[string_buf_idx++] = ' ';
                    token = strtok(NULL, delim);
                }
            }

            if (!string_terminated) {
                printf("Unterminated string\n");
                exit(1);
            }
        } else {
            printf("Unexpected input: %s\n", token);
            exit(1);
        }
        program[program_idx] = curr_exp;
        token = strtok(NULL, delim);
    }

    return program;
}

sexp *
eval (sexp *exp) {
    return exp;
}

void
print (sexp *exp) {
    atom atom = exp->atomic_value;
    if (atom.type == NUMBER) {
        printf("%ld", atom.number_value);
    } else if (atom.type == BOOLEAN) {
        if (atom.number_value)
            printf("#t");
        else
            printf("#f");
    } else if (atom.type == CHARACTER) {
        if (atom.character_value == ' ') {
            printf("#\\space");
        } else if (atom.character_value == '\n') {
            printf("#\\newline");
        } else {
            printf("#\\%c", atom.character_value);
        }
    } else if (atom.type == STRING) {
        printf("%s", atom.string_value);
    } else {
        printf("ERR: Unable to print invalid sexp");
    }
    printf("\n");
}

int main (void) {
    printf("Welcome to Lithp\n");
    size_t buf_size = 1024;
    char *buf = malloc(buf_size);
    while (1) {
        // read
        printf("> ");
        getline(&buf, &buf_size, stdin);

        // evaluate
        sexp *program = parse(buf);
        sexp *result = eval(program);

        // print
        print(result);
    }
    free(buf);
}
