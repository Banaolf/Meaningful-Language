CC = gcc
CFLAGS = -static -O3 -Wall -Wextra -lm
AR = ar
ARFLAGS = rcs

LIB_NAME = libcore.a
TARGET = .\bin\meaningful.exe

LIB_OBJS = lexer.o parser.o
MAIN_OBJ = interpreter.o

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) -o $(TARGET) $(MAIN_OBJ) -L. -lcore
	rm *.o *.a 
$(LIB_NAME): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(LIB_OBJS)

lexer.o: lexer.c lexer.h parser.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c parser.h lexer.h
	$(CC) $(CFLAGS) -c parser.c

interpreter.o: interpreter.c parser.h lexer.h
	$(CC) $(CFLAGS) -c interpreter.c

clean:
	cd .\bin
	rm -f *.o *.a *.exe
	cd ..\

.PHONY: all clean
