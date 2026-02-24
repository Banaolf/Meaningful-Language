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
    TOKEN_EOL
} TokenType;

typedef struct {
    TokenType type;
    char* value; // Dynamic string for the actual text
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