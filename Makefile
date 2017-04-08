CFLAGS = -Wall

.PHONY: clean

lithp.o: lithp.c lithp.h
	gcc -ggdb -c -o $@ $< $(CFLAGS)

lithp: lithp.o
	gcc -ggdb -o $@ $^ $(CFLAGS)

clean:
	rm lithp
