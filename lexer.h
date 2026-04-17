#ifndef LEXER_H
#define LEXER_H

typedef enum {
  // Types & Values
  TOKEN_INT_TYPE,
  TOKEN_INT_VALUE,
  TOKEN_STRING_TYPE,
  TOKEN_STRING_VALUE,
  TOKEN_AMOUNT_OF_THREADS, // for the for loop

  // Variable
  TOKEN_IDENTIFIER,

  // Operators
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_NOT,
  TOKEN_NOT_EQUAL,
  TOKEN_GREATER,
  TOKEN_LESS,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS_EQUAL,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_PLUS,
  TOKEN_PLUS_PLUS,
  TOKEN_PLUS_EQUAL,
  TOKEN_MINUS,
  TOKEN_MINUS_EQUAL,
  TOKEN_MULTIPLY,
  TOKEN_DIVIDE,

  // Keywords
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_PRINT,
  TOKEN_FOR,
  TOKEN_FUNCTION,
  TOKEN_RETURN,
  TOKEN_PROCESS,
  TOKEN_AWAIT,
  TOKEN_THREAD,
  TOKEN_PARALLEL,

  // Symbols
  TOKEN_SEMICOLON,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_CURLYBRACKET,
  TOKEN_RIGHT_CURLYBRACKET,
  TOKEN_COMMA,

  // Other
  TOKEN_ILLEGAL,
  TOKEN_EOF,
} TokenType;

typedef enum {
  STATE_START,
  STATE_IN_INTEGER,
  STATE_IN_IDENTIFIER,
  STATE_IN_STRING,
  STATE_IN_OPERATOR,
  STATE_ACCEPT
} LexerState;

typedef struct {
  TokenType type;
  union {
    char *string_value;
    int int_value;
  } value;
} Token;

typedef struct {
  char *input;
  int position;
  int length;
} Lexer;

Lexer new_lexer(char *input);
Token next_token(Lexer *lexer);

#endif
