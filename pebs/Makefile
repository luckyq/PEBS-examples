CC = g++ -g
CFLAGS = -w -fPIC
INCLUDES = -I/usr/local/include
LIBS = -lm -lpthread -ldl -lpfm

OBJ1 = pebs
SRC1 = main.c pebs.cpp


all: $(SRC1) $(SRC2) $(SRC3)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OBJ1) $(SRC1) $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ1) 
