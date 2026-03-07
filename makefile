# Configuración del compilador
CC = gcc
CFLAGS = -static -O3 -Wall -Wextra -lm
AR = ar
ARFLAGS = rcs

LIB_NAME = libcore.a
TARGET = .\bin\meaningful.exe

# Objetos necesarios
LIB_OBJS = lexer.o parser.o
MAIN_OBJ = interpreter.o

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) -o $(TARGET) $(MAIN_OBJ) -L. -lcore
	del *.o *.a 
#The above command is only for windows.
$(LIB_NAME): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(LIB_OBJS)

lexer.o: lexer.c lexer.h parser.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c parser.h lexer.h
	$(CC) $(CFLAGS) -c parser.c

interpreter.o: interpreter.c parser.h lexer.h
	$(CC) $(CFLAGS) -c interpreter.c

#If you install the source code and is using make, please make sure you make the bin folder.
clean:
	cd .\bin
	del -f *.o *.a *.exe
	cd ..\

.PHONY: all clean
