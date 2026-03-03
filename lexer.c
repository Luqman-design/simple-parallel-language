#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Lexer new_lexer(char *input) {
  Lexer lexer;
  lexer.input = input;
  lexer.position = 0;
  lexer.length = strlen(input);
  return lexer;
}

static char peek(Lexer *lexer) { return lexer->input[lexer->position]; }

Token next_token(Lexer *lexer) {
  State state = START;
  char temp_token[64];
  int temp_token_tracker = 0;

  while (lexer->position < lexer->length) {
    char current_char = peek(lexer);

    // Symbols
    if (state == START && current_char == ';') {
      Token token;
      token.type = SEMI_COLON;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '(') {
      Token token;
      token.type = LEFT_PAREN;
      lexer->position++;
      return token;
    } else if (state == START && current_char == ')') {
      Token token;
      token.type = RIGHT_PAREN;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '{') {
      Token token;
      token.type = LEFT_CURLYBRACKET;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '}') {
      Token token;
      token.type = RIGHT_CURLYBRACKET;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '+') {
      Token token;
      token.type = PLUS;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '-') {
      Token token;
      token.type = MINUS;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '*') {
      Token token;
      token.type = MULTIPLY;
      lexer->position++;
      return token;
    } else if (state == START && current_char == '/') {
      Token token;
      token.type = DIVIDE;
      lexer->position++;
      return token;
    }

    // Operators
    else if (state == START && (current_char == '=' || current_char == '<' ||
                                current_char == '>' || current_char == '&' ||
                                current_char == '|' || current_char == '!')) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
      state = IN_OPERATOR;
    } else if (state == IN_OPERATOR &&
               (current_char == '=' || current_char == '<' ||
                current_char == '>' || current_char == '&' ||
                current_char == '|' || current_char == '!')) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
    } else if (state == IN_OPERATOR) {
      temp_token[temp_token_tracker] = '\0';

      Token token;

      if (strcmp(temp_token, "=") == 0) {
        token.type = EQUAL;
      } else if (strcmp(temp_token, "==") == 0) {
        token.type = EQUAL_EQUAL;
      } else if (strcmp(temp_token, "<") == 0) {
        token.type = LESS;
      } else if (strcmp(temp_token, ">") == 0) {
        token.type = GREATER;
      } else if (strcmp(temp_token, "<=") == 0) {
        token.type = LESS_EQUAL;
      } else if (strcmp(temp_token, ">=") == 0) {
        token.type = GREATER_EQUAL;
      } else if (strcmp(temp_token, "!") == 0) {
        token.type = NOT;
      } else if (strcmp(temp_token, "!=") == 0) {
        token.type = NOT_EQUAL;
      } else if (strcmp(temp_token, "&&") == 0) {
        token.type = AND;
      } else if (strcmp(temp_token, "||") == 0) {
        token.type = OR;
      } else {
        token.type = ILLEGAL;
      }

      return token;
    }

    // Identifiers & Keywords
    else if (state == START && (current_char == '_' || isalpha(current_char))) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
      state = IN_IDENTIFIER;
    } else if (state == IN_IDENTIFIER &&
               (current_char == '_' || isalpha(current_char))) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
    } else if (state == IN_IDENTIFIER) {
      temp_token[temp_token_tracker] = '\0';

      Token token;
      token.type = IDENTIFIER;
      token.value.string_value = strdup(temp_token);

      if (strcmp(temp_token, "int") == 0) {
        token.type = INT_TYPE;
      } else if (strcmp(temp_token, "string") == 0) {
        token.type = STRING_TYPE;
      } else if (strcmp(temp_token, "print") == 0) {
        token.type = PRINT;
      } else if (strcmp(temp_token, "if") == 0) {
        token.type = IF;
      } else if (strcmp(temp_token, "else") == 0) {
        token.type = ELSE;
      }

      return token;
    }

    // Strings
    else if (state == START && current_char == '"') {
      lexer->position++;
      state = IN_STRING;
    } else if (state == IN_STRING && current_char != '"') {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
    } else if (state == IN_STRING && current_char == '"') {
      temp_token[temp_token_tracker] = '\0';
      lexer->position++;

      Token token;
      token.type = STRING_VALUE;
      token.value.string_value = strdup(temp_token);
      return token;
    }

    // Integers
    else if (state == START && isdigit(current_char)) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
      state = IN_INTEGER;
    } else if (state == IN_INTEGER && isdigit(current_char)) {
      temp_token[temp_token_tracker] = current_char;
      temp_token_tracker++;
      lexer->position++;
    } else if (state == IN_INTEGER) {
      temp_token[temp_token_tracker] = '\0';

      Token token;
      token.type = INT_VALUE;
      token.value.int_value = atoi(temp_token);
      return token;
    }

    // Other (such as whitespaces)
    else {
      lexer->position++;
    }
  }

  Token token;
  return token;
}
