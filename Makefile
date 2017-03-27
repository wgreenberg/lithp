HEADERS = language.h parse.h
OBJ = parse.o main.o
CFLAGS = -Wall

.PHONY: clean

%.o: %.c $(HEADERS)
	gcc -ggdb -c -o $@ $< $(CFLAGS)

lithp: $(OBJ)
	gcc -ggdb -o $@ $^ $(CFLAGS)

clean:
	rm lithp
