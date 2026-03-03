#ifndef LEXER_H
#define LEXER_H

typedef enum {
  // Types & Values
  INT_TYPE,
  INT_VALUE,
  STRING_TYPE,
  STRING_VALUE,

  // Variable
  IDENTIFIER,

  // Operators
  EQUAL,
  EQUAL_EQUAL,
  NOT,
  NOT_EQUAL,
  GREATER,
  LESS,
  GREATER_EQUAL,
  LESS_EQUAL,
  AND,
  OR,
  PLUS,
  MINUS,
  MULTIPLY,
  DIVIDE,

  // Keywords
  IF,
  ELSE,
  PRINT,

  // Symbols
  SEMI_COLON,
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_CURLYBRACKET,
  RIGHT_CURLYBRACKET,

  // Other
  ILLEGAL,
} TokenType;

typedef enum {
  START,
  IN_INTEGER,
  IN_IDENTIFIER,
  IN_STRING,
  IN_OPERATOR,
  ACCEPT
} State;

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
