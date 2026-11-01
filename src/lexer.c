#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer.h"
#include "parser.h"
#include "err_handler.h"
//Licensed under BMLL2.0, see LICENCE for further info

static bool get_keyword_type(const char* str) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return true; // It's a keyword
        }
    }
    return false; // It's just a name (Identifier)
}

// Initialize the stream "Suitcase"
static TokenStream* createTokenStream() {
    TokenStream* stream = malloc(sizeof(TokenStream));
    if (!stream) {
        error_t err = make_error(ERRTY_InternalError, "Couldn't allocate TokenStream struct", 0, 0);
        set_err(&err);
        halt();
    }
    stream->size = 0;
    stream->capacity = 10;
    stream->tokens = malloc(sizeof(Token) * stream->capacity);
    if (!stream->tokens) {
        error_t err = make_error(ERRTY_InternalError, "Couldn't allocate token array", 0, 0);
        set_err(&err);
        halt();
    }
    return stream;
}

void finalCleanup(TokenStream* stream) {
    for (int i = 0; i < stream->size; i++) {
        free(stream->tokens[i].value);
    }
    free(stream->tokens);
    free(stream);
}

static int line = 0;
static int character = 0;

// Add a token to the list and grow if full
static void addToken(TokenStream* stream, TokenType type, const char* value) {
    if (value[0] == '0' && value[1] == 'b') {
        int count = 0;
        for (int i = 2; i < strlen(value); i++) {
            const char chara = value[i];
            if (chara != '1' && chara != '0') {
                error_t err = make_error(ERRTY_SyntaxError, "Malformed Binary", line, character);
                set_err(&err);
            }
            count++;
        }
        stream->tokens[stream->size].bitlength = count;
    }
    if (stream->size == stream->capacity) {
        stream->capacity *= 2;
        void* realloc_result = realloc(stream->tokens, sizeof(Token) * stream->capacity);
        if (!realloc_result) {
            error_t err = make_error(ERRTY_InternalError, "Couldn't reallocate token list", line, character);
            set_err(&err);
            finalCleanup(stream);
            halt();
        }
        stream->tokens = realloc_result;
    }
    stream->tokens[stream->size].type = type;
    stream->tokens[stream->size].value = strdup(value); // Copy string to safe memory
    stream->tokens[stream->size].ln = line;
    stream->tokens[stream->size].character = character;
    stream->size++;
}

static int is_digit(const char *s) {
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

TokenStream* lex(const char* source) {
    TokenStream* stream = createTokenStream();
    const char* c = source;

    while (*c != '\0') {
        character = character + 1;
        if (isspace(*c)) {
            if (*c == '\n'){line++; character = 0;}
            c++;
            continue;
        }

        if (*c == ':') {
            if (*(c+1) == ':') {
                while (*c != '\0' && *c != '\n'){if(*c == '\n')line++;character++;c++;} // COMMENTS!!
                continue;
            } else if (*(c+1) == ';'){
                while (*c != '\0' && !(*c == ';' && *(c+1) == ':')){if(*c == '\n')line++;character++;c++;} // BLOCK COMMENTS!!
                if(*c == '\0'){
                    // Non-fatal warning or treat as syntax error depending on preference; keeping message structure clean
                    fprintf(stderr, "[LEXER] WARN: block comment reaches end of file!\n");
                    c+=2;
                }
                continue;
            } else {
                addToken(stream, TOKEN_COLON, ":");
                c++;
                continue;
            }
        }

        if (*c == '!') {
            addToken(stream, TOKEN_UNARY, "!");
            c++;
            continue;
        }

        if (*c == '?') {
            addToken(stream, TOKEN_QUESTION_MARK, "?");
            c++;
            continue;
        }

        if (*c == '.'){
            addToken(stream, TOKEN_DOT, ".");
            c++;
            continue;
        }

        if (*c == '$'){
            addToken(stream, TOKEN_DLRSIGN, "$");
            c++;
            continue;
        }

        if (*c == '@') {
            addToken(stream, TOKEN_AT, "@");
            c++;
            continue;
        }

        if (*c == '^') {
            addToken(stream, TOKEN_CARET, "^");
            c++;
            continue;
        }

        if (*c == '#') {
            addToken(stream, TOKEN_HASHTAG, "#"); //COMING SOON
            c++;
            continue;
        }

        if (*c == '0' && *(c+1) == 'b') {
            char buffer[256] = {0};
            int i = 0;
            c+=2;
            while (*c == '0' || *c == '1') {
                character++;
                buffer[i++] = *c++;
            }
            addToken(stream, TOKEN_BINARY, buffer);
            continue;
        }

        if (isalnum(*c) || *c == '_' ) {
            char buffer[256] = {0};
            int i = 0;
            while (isalnum(*c) || *c == '_' ) {character++; buffer[i++] = *c++;}

            if (get_keyword_type(buffer)) {
                addToken(stream, TOKEN_KEYWORD, buffer);
            } else if (strcmp(buffer, "true") == 0 || strcmp(buffer, "false") == 0){
                addToken(stream, TOKEN_BOOLEAN, buffer);
            } else if (strcmp(buffer, "non") == 0){
                addToken(stream, TOKEN_NON, "non");
            } else if (is_digit(buffer) == 0) {
                addToken(stream, TOKEN_NUMBER, buffer);
            } else if (strcmp(buffer, "not") == 0) {
                addToken(stream, TOKEN_UNARY, "not");
            } else {
                addToken(stream, TOKEN_IDENTIFIER, buffer);
            }
            continue;
        }

        if (isdigit(*c)) {
            char buffer[256] = {0};
            int i = 0;
            int pointCount = 0;
            while (isdigit(*c) || *c == '.') {
                character++;
                buffer[i++] = *c++;
                if (*c == '.'){
                    pointCount++;
                }
            }
            if (pointCount > 1){
                error_t err = make_error(ERRTY_SyntaxError, "Floats cannot have more than 1 decimal point '.'", line, character);
                set_err(&err);
            }
            if (strchr(buffer, '.') != NULL){
                addToken(stream, TOKEN_FLOAT, buffer);
            } else{
                addToken(stream, TOKEN_NUMBER, buffer);
            }
            continue;
        }

        if (*c == '(' || *c == ')') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_PARENTHESIS, buffer);
            c++;
            continue;
        }

        if (*c == '[' || *c == ']') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_SQUARE, buffer);
            c++;
            continue;
        }

        if (*c == '{' || *c == '}') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_CURLY, buffer);
            c++;
            continue;
        }

        if (*c == ',') {
            addToken(stream, TOKEN_COMMA, ",");
            c++;
            continue;
        }

        if (*c == ';') {
            addToken(stream, TOKEN_SEMICOLON, ";");
            c++;
            continue;
        }

        // Handle ==, !=, <=, >=
        if ((*c == '=' || *c == '!' || *c == '<' || *c == '>') && *(c+1) == '=') {
            char buffer[3] = {*c, *(c+1), '\0'};
            addToken(stream, TOKEN_COMPARISON, buffer);
            c += 2;
            continue;
        }

        // Handle <, >
        if (*c == '<' || *c == '>') {
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_COMPARISON, buffer);
            c++;
            continue;
        }

        if (*c == '-' && *(c+1) == '/'){
            addToken(stream, TOKEN_OPERATOR, "-/");
            c += 2;
            continue;
        }

        if (strchr("+-*/", *c)) {
            if (*(c+1) == '=') {
                char buffer[3] = {*c, '=', '\0'};
                addToken(stream, TOKEN_COMPOUND_ASSIGN, buffer);
                c += 2;
                continue;
            } else if (*c == '*' && *(c+1) == '*'){
                addToken(stream, TOKEN_OPERATOR, "**");
                c += 2;
                continue;
            } else if (*c == '/' && *(c+1) == '/'){
                addToken(stream, TOKEN_OPERATOR, "//");
                c += 2;
                continue;
            }
            char buffer[2] = {*c, '\0'};
            addToken(stream, TOKEN_OPERATOR, buffer);
            c++;
            continue;
        }

        if (strchr("%", *c)){
            addToken(stream, TOKEN_OPERATOR, "%");
            c+=2;
            continue;
        }

        if (*c == '=') {
            addToken(stream, TOKEN_EQUALS, "=");
            c++;
            continue;
        }

        if (*c == '"') {
            c++; // Skip opening quote
            char buffer[256] = {0};
            int i = 0;
            while (*c != '"' && *c != '\0') {
                if (*c == '\\') {
                    c++; // skip backslash
                    switch (*c) {
                        case 'n':  buffer[i++] = '\n'; break;
                        case 't':  buffer[i++] = '\t'; break;
                        case 'r':  buffer[i++] = '\r'; break;
                        case '\\': buffer[i++] = '\\'; break;
                        case '"':  buffer[i++] = '"';  break;
                        case '0':  buffer[i++] = '\0'; break;
                        default: {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Unknown escape sequence '\\%c'", *c);
                            error_t err = make_error(ERRTY_SyntaxError, msg, line, character);
                            set_err(&err);
                            break;
                        }
                    }
                } else {
                    buffer[i++] = *c;
                }
                character++;
                c++;
            }
            if (*c == '"') c++; // Skip closing quote
            addToken(stream, TOKEN_STRING, buffer);
            continue;
        }

        if (*c == '\'') { // Single quotes are raw strings
            c++; // Skip opening quote
            char buffer[256] = {0};
            int i = 0;
            while (*c != '\'' && *c != '\0') {
                character++;
                buffer[i++] = *c++;
            }
            if (*c == '\'') c++; // Skip closing quote
            addToken(stream, TOKEN_STRING, buffer);
            continue;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "Unknown character '%c'", *c);
        error_t err = make_error(ERRTY_SyntaxError, msg, line, character);
        set_err(&err);
    }

    addToken(stream, TOKEN_EOF, "EOF");
    return stream;
}