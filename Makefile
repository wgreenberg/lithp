HEADERS = language.h parse.h
OBJ = parse.o main.o
CFLAGS = -Wall

.PHONY: clean

%.o: %.c $(HEADERS)
	gcc -c -o $@ $< $(CFLAGS)

lithp: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

clean:
	rm lithp
