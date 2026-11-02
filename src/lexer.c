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

// Position of the token currently being lexed (1-based line and column of its first character).
static int line = 1;
static int character = 1;

static int curLine = 1;
static const char* lineStart = NULL;

static void note_newline(const char* p) {
    curLine++;
    lineStart = p + 1;
}

// Add a token to the list and grow if full
static void addToken(TokenStream* stream, TokenType type, const char* value) {
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
    stream->tokens[stream->size].bitlength = 0; // only binary literals set this (see the 0b branch)
    stream->size++;
}

// --- 3. THE LEXER ENGINE ---

TokenStream* lex(const char* source) {
    TokenStream* stream = createTokenStream();
    const char* c = source;

    // Fresh position state for every lex() call (the REPL and imports call it repeatedly).
    curLine = 1;
    lineStart = source;
    line = 1;
    character = 1;

    while (*c != '\0') {
        if (*c == '\n') {
            note_newline(c);
            c++;
            continue;
        }
        if (isspace((unsigned char)*c)) {
            c++;
            continue;
        }

        // This is where the next token starts: 1-based line and column.
        line = curLine;
        character = (int)(c - lineStart) + 1;

        if (*c == ':') {
            if (*(c+1) == ':') {
                // LINE COMMENT '::' - stop before the '\n'; the main loop counts it.
                while (*c != '\0' && *c != '\n') c++;
                continue;
            } else if (*(c+1) == ';'){
                // BLOCK COMMENT ':;' ... ';:'
                c += 2; // skip the opening ':;'
                while (*c != '\0' && !(*c == ';' && *(c+1) == ':')) {
                    if (*c == '\n') note_newline(c);
                    c++; // one character at a time, so nothing is skipped
                }
                if (*c == '\0') {
                    fprintf(stderr, "CommentWarning: block comment reaches end of file!\n");
                } else {
                    c += 2; // skip the closing ';:'
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
            c += 2; // skip the '0b' prefix
            while ((*c == '0' || *c == '1') && i < 255) {
                buffer[i++] = *c++;
            }
            // Needs at least one bit, and must not run straight into other characters
            // (e.g. 0b102, 0bxyz, or more bits than fit in the buffer).
            if (i == 0 || isalnum((unsigned char)*c) || *c == '_') {
                while (isalnum((unsigned char)*c) || *c == '_') c++; // skip the rest of the bad literal
                error_t err = make_error(ERRTY_SyntaxError, "Malformed binary literal", line, character);
                set_err(&err);
                free(err.message); // set_err keeps its own copy
                continue;
            }
            addToken(stream, TOKEN_BINARY, buffer); // value is the bits only, without '0b'
            stream->tokens[stream->size - 1].bitlength = i; // number of bits
            continue;
        }

        // Identifiers and keywords must START with a letter or '_' (digits are handled below),
        // otherwise numbers get swallowed here and floats like 3.14 never reach the number branch.
        if (isalpha((unsigned char)*c) || *c == '_' ) {
            char buffer[256] = {0};
            int i = 0;
            while (isalnum(*c) || *c == '_' ) { buffer[i++] = *c++; }

            if (get_keyword_type(buffer)) {
                addToken(stream, TOKEN_KEYWORD, buffer);
            } else if (strcmp(buffer, "true") == 0 || strcmp(buffer, "false") == 0){
                addToken(stream, TOKEN_BOOLEAN, buffer);
            } else if (strcmp(buffer, "non") == 0){
                addToken(stream, TOKEN_NON, "non");
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

        if (*c == '%') {
            addToken(stream, TOKEN_OPERATOR, "%");
            c ++;
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
                    if (*c == '\n') note_newline(c); // raw newline inside the string
                    buffer[i++] = *c;
                }
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
                if (*c == '\n') note_newline(c); // raw newline inside the string
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
        c++; // skip the bad character, otherwise this loop never ends
    }

    // The EOF token sits at the very end of the source.
    line = curLine;
    character = (int)(c - lineStart) + 1;
    addToken(stream, TOKEN_EOF, "EOF");
    return stream;
}