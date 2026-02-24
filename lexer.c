#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"
#include "syntax.h"
//Licensed under BMLL2.0, see LICENCE for further info
// --- THE TOOLS ---

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

// --- 3. THE LEXER ENGINE ---

TokenStream* lex(char* source) {
    TokenStream* stream = createTokenStream();
    char* c = source;

    int line = 0;
    int character = 0;

    while (*c != '\0') {
        character = character + 1;
        if (isspace(*c)) {
            if (*c == '\n'){line=line+1;character=0;addToken(stream, TOKEN_EOL, "\n", line, character);}
            c++;
            continue;
        }

        if (*c == ':' && *(c+1) == ':') {
            while (*c != '\0' || *c != '\n'){c++;}
        }

        if (isalpha(*c)) {
            char buffer[256] = {0};
            int i = 0;
            while (isalnum(*c)) buffer[i++] = *c++;
            
            char *peek = c;
            while (isspace(*peek)) peek++;

            if (*peek == '(') {
                addToken(stream, TOKEN_FUNCTION, buffer, line, character);
            } else if (get_keyword_type(buffer)) {
                addToken(stream, TOKEN_KEYWORD, buffer, line, character);
            } else {
                addToken(stream, TOKEN_IDENTIFIER, buffer, line, character);
            }
            continue;
        }

        if (isdigit(*c)) {
            char buffer[256] = {0};
            int i = 0;
            while (isdigit(*c)) buffer[i++] = *c++;
            addToken(stream, TOKEN_NUMBER, buffer, line, character);
            continue;
        }
        
        if (*c == '(' || *c == ')') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_PARENTHESIS, buffer, line, character);
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