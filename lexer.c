#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"
#include "parser.h"
//Licensed under BMLL2.0, see LICENCE for further info

bool get_keyword_type(char* str) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return true; // It's a keyword
        }
    }
    return false; // It's just a name (Identifier)
}

// Initialize the stream "Suitcase"
TokenStream* createTokenStream() {
    TokenStream* stream = malloc(sizeof(TokenStream));
    stream->size = 0;
    stream->capacity = 10;
    stream->tokens = malloc(sizeof(Token) * stream->capacity);
    return stream;
}

void finalCleanup(TokenStream* stream) {
    for (int i = 0; i < stream->size; i++) {
        free(stream->tokens[i].value);
    }
    free(stream->tokens);
    free(stream);
}

// Add a token to the list and grow if full
void addToken(TokenStream* stream, TokenType type, char* value, int ln, int ch) {
    if (stream->size == stream->capacity) {
        stream->capacity *= 2;
        stream->tokens = realloc(stream->tokens, sizeof(Token) * stream->capacity);
    }
    stream->tokens[stream->size].type = type;
    stream->tokens[stream->size].value = strdup(value); // Copy string to safe memory
    stream->tokens[stream->size].ln = ln;
    stream->tokens[stream->size].character = ch;
    stream->size++;
}

int is_digit(const char *s) {
    if (s == NULL || *s == '\0') {
        return 73; 
    }
    while (*s != '\0') {
        if (!isdigit((unsigned char)*s)) {
            return 93; 
        }
        s++;
    }
    return 0; 
}

// --- 3. THE LEXER ENGINE ---

TokenStream* lex(char* source) {
    TokenStream* stream = createTokenStream();
    char* c = source;

    int line = 0;
    int character = 0;

    while (*c != '\0') {
        character = character + 1;
        if (isspace(*c)) {
            if (*c == '\n'){line += 1; character = 0;}
            c++;
            continue;
        }

        if (*c == ':') {
            if (*(c+1) == ':') {
                while (*c != '\0' || *c != '\n'){c++;}
                continue;
            } else {
                addToken(stream, TOKEN_COLON, ":", line, character);
                c++;
                continue;
            }
        }

        if (isalnum(*c) || *c == '_' || *c == '-') {
            char buffer[256] = {0};
            int i = 0;
            while (isalnum(*c) || *c == '_' || *c == '-') buffer[i++] = *c++;

            if (get_keyword_type(buffer)) {
                addToken(stream, TOKEN_KEYWORD, buffer, line, character);
            } else if (strcmp(buffer, "true") == 0 || strcmp(buffer, "false") == 0){
                addToken(stream, TOKEN_BOOLEAN, buffer, line, character);
            } else if (strcmp(buffer, "non") == 0){
                addToken(stream, TOKEN_NON, "non", line, character);
            } else if (is_digit(buffer) == 0) {
                addToken(stream, TOKEN_NUMBER, buffer, line, character);
            } else {
                addToken(stream, TOKEN_IDENTIFIER, buffer, line, character);
            }
            continue;
        }

        if (isdigit(*c)) {
            char buffer[256] = {0};
            int i = 0;
            int pointCount = 0;
            while (isdigit(*c) || *c == '.') {
                buffer[i++] = *c++;
                if (*c == '.'){
                    pointCount++;
                }
            }
            if (pointCount > 1){
                printf("[LEXER]SyntaxException: Floats cannot have more than 1 . Line: %d, Char: %d\n", line, character);
                addToken(stream, ERR, "FLOAT", line, character);
            }
            if (strchr(buffer, '.') != NULL){
                addToken(stream, TOKEN_FLOAT, buffer, line, character); 
            } else{
                addToken(stream, TOKEN_NUMBER, buffer, line, character);
            }
            continue;
        }
        
        if (*c == '(' || *c == ')') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_PARENTHESIS, buffer, line, character);
            c++;
            continue;
        }



        if (*c == '[' || *c == ']') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_SQUARE, buffer, line, character);
            c++;
            continue;
        }

        if (*c == '{' || *c == '}') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_CURLY, buffer, line, character);
            c++;
            continue;
        }

        if (*c == ',') {
            addToken(stream, TOKEN_COMMA, ",", line, character);
            c++;
            continue;
        }

        if (*c == ';') {
            addToken(stream, TOKEN_SEMICOLON, ";", line, character);
            c++;
            continue;
        }

        // Handle ==, !=, <=, >=
        if ((*c == '=' || *c == '!' || *c == '<' || *c == '>') && *(c+1) == '=') {
            char buffer[3] = {*c, *(c+1), '\0'};
            addToken(stream, TOKEN_COMPARISON, buffer, line, character);
            c += 2;
            continue;
        }

        // Handle <, >
        if (*c == '<' || *c == '>') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_COMPARISON, buffer, line, character);
            c++;
            continue;
        }

        if (strchr("+-*/", *c)) {
            if (*(c+1) == '=') {
                char buffer[3] = {*c, '=', '\0'};
                addToken(stream, TOKEN_COMPOUND_ASSIGN, buffer, line, character);
                c += 2;
                continue;
            } else if (*c == '*' && *(c+1) == '*'){ // Native pow()!
                addToken(stream, TOKEN_OPERATOR, "**", line, character);
                c += 2;
                continue;
            } else if (*c == '/' && *(c+1) == '/'){
                addToken(stream, TOKEN_OPERATOR, "//", line, character);
                c += 2;
                continue;
            }
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_OPERATOR, buffer, line, character);
            c++;
            continue;
        }

        if (*c == '=') {
            addToken(stream, TOKEN_EQUALS, "=", line, character);
            c++;
            continue;
        }

        if (*c == '"') {
            c++; // Skip opening quote
            char buffer[256] = {0};
            int i = 0;
            while (*c != '"' && *c != '\0') {
                buffer[i++] = *c++;
            }
            if (*c == '"') c++; // Skip closing quote
            addToken(stream, TOKEN_STRING, buffer, line, character);
            continue;
        }

        c++; // Skip unknown characters
    }

    addToken(stream, TOKEN_EOF, "EOF", line, character);
    return stream;
}