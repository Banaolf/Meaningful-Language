#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_KEYWORD,    // SET, PRINT, IF, ELSE, WHILE
    TOKEN_IDENTIFIER, // variable names 
    TOKEN_NUMBER,     // integers 123213
    TOKEN_OPERATOR,   // +, -, *, /
    TOKEN_EQUALS,     // =
    TOKEN_COMPARISON, // <, >, <=, >=, ==, !=
    TOKEN_STRING,     // "String"
    TOKEN_EOF,        // End of program
    TOKEN_FUNCTION,   // Function f()
    TOKEN_PARENTHESIS,// Parenthesis ()
    TOKEN_COMMA,      // Comma ,
    TOKEN_SEMICOLON,  // Semicolon ;
    TOKEN_NON, //non
    TOKEN_BOOLEAN, //true | false
    TOKEN_SQUARE, //[]
    TOKEN_CURLY, //{}
    TOKEN_COLON, // :
    TOKEN_COMPOUND_ASSIGN, // +=, -=, *=, /=
    TOKEN_FLOAT, //3.14
    TOKEN_DOT,
    ERR //Mostly for when theres multiple . in float. Maybe other uses in the future
} TokenType;

typedef struct {
    TokenType type;
    char* value; 
    int ln;
    int character;
} Token;

typedef struct {
    Token* tokens;
    int size;
    int capacity;
} TokenStream;

TokenStream* lex(char* source);

void finalCleanup(TokenStream* stream);

#endif