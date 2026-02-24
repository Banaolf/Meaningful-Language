# Configuración del compilador
CC = gcc
CFLAGS = -Wall -Wextra -g
AR = ar
ARFLAGS = rcs

# Nombres de archivos
LIB_NAME = libcore.a
TARGET = meaningful.exe

# Objetos necesarios
LIB_OBJS = lexer.o syntax.o
MAIN_OBJ = interpreter.o

# Regla principal (la que se ejecuta al escribir 'make')
all: $(TARGET)

# 1. Crear el ejecutable final enlazando el objeto principal con la librería
$(TARGET): $(MAIN_OBJ) $(LIB_NAME)
	$(CC) $(CFLAGS) -o $(TARGET) $(MAIN_OBJ) -L. -lcore

# 2. Crear la biblioteca estática a partir de los objetos del lexer y syntax
$(LIB_NAME): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(LIB_OBJS)

# 3. Compilar los archivos .c a archivos objeto .o
# Se agregan los .h como dependencias para que si cambias un header, se recompile el .c
lexer.o: lexer.c lexer.h syntax.h
	$(CC) $(CFLAGS) -c lexer.c

syntax.o: syntax.c syntax.h lexer.h
	$(CC) $(CFLAGS) -c syntax.c

interpreter.o: interpreter.c syntax.h lexer.h
	$(CC) $(CFLAGS) -c interpreter.c

# Limpiar archivos temporales y el ejecutable
clean:
	rm -f *.o *.a $(TARGET)

# Evitar conflictos con archivos que se llamen igual que las reglas
.PHONY: all clean
