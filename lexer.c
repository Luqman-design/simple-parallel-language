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
  LexerState state = STATE_START;
  char current_token_buffer[64];
  int current_token_length = 0;

  while (lexer->position < lexer->length) {
    char current_character = peek(lexer);

    // Symbols
    if (state == STATE_START && current_character == ';') {
      Token token;
      token.type = TOKEN_SEMICOLON;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '(') {
      Token token;
      token.type = TOKEN_LEFT_PAREN;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == ')') {
      Token token;
      token.type = TOKEN_RIGHT_PAREN;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '{') {
      Token token;
      token.type = TOKEN_LEFT_CURLYBRACKET;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '}') {
      Token token;
      token.type = TOKEN_RIGHT_CURLYBRACKET;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '+') {
      Token token;
      token.type = TOKEN_PLUS;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '-') {
      Token token;
      token.type = TOKEN_MINUS;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '*') {
      Token token;
      token.type = TOKEN_MULTIPLY;
      lexer->position++;
      return token;
    } else if (state == STATE_START && current_character == '/') {
      Token token;
      token.type = TOKEN_DIVIDE;
      lexer->position++;
      return token;
    }

    // Operators
    else if (state == STATE_START && (current_character == '=' || current_character == '<' ||
                                current_character == '>' || current_character == '&' ||
                                current_character == '|' || current_character == '!')) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_OPERATOR;
    } else if (state == STATE_IN_OPERATOR &&
               (current_character == '=' || current_character == '<' ||
                current_character == '>' || current_character == '&' ||
                current_character == '|' || current_character == '!')) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_OPERATOR) {
      current_token_buffer[current_token_length] = '\0';

      Token token;

      if (strcmp(current_token_buffer, "=") == 0) {
        token.type = TOKEN_EQUAL;
      } else if (strcmp(current_token_buffer, "==") == 0) {
        token.type = TOKEN_EQUAL_EQUAL;
      } else if (strcmp(current_token_buffer, "<") == 0) {
        token.type = TOKEN_LESS;
      } else if (strcmp(current_token_buffer, ">") == 0) {
        token.type = TOKEN_GREATER;
      } else if (strcmp(current_token_buffer, "<=") == 0) {
        token.type = TOKEN_LESS_EQUAL;
      } else if (strcmp(current_token_buffer, ">=") == 0) {
        token.type = TOKEN_GREATER_EQUAL;
      } else if (strcmp(current_token_buffer, "!") == 0) {
        token.type = TOKEN_NOT;
      } else if (strcmp(current_token_buffer, "!=") == 0) {
        token.type = TOKEN_NOT_EQUAL;
      } else if (strcmp(current_token_buffer, "&&") == 0) {
        token.type = TOKEN_AND;
      } else if (strcmp(current_token_buffer, "||") == 0) {
        token.type = TOKEN_OR;
      } else {
        token.type = TOKEN_ILLEGAL;
      }

      return token;
    }

    // Identifiers & Keywords
    else if (state == STATE_START && (current_character == '_' || isalpha(current_character))) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_IDENTIFIER;
    } else if (state == STATE_IN_IDENTIFIER &&
               (current_character == '_' || isalpha(current_character))) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_IDENTIFIER) {
      current_token_buffer[current_token_length] = '\0';

      Token token;
      token.type = TOKEN_IDENTIFIER;
      token.value.string_value = strdup(current_token_buffer);

      if (strcmp(current_token_buffer, "int") == 0) {
        token.type = TOKEN_INT_TYPE;
      } else if (strcmp(current_token_buffer, "string") == 0) {
        token.type = TOKEN_STRING_TYPE;
      } else if (strcmp(current_token_buffer, "print") == 0) {
        token.type = TOKEN_PRINT;
      } else if (strcmp(current_token_buffer, "if") == 0) {
        token.type = TOKEN_IF;
      } else if (strcmp(current_token_buffer, "else") == 0) {
        token.type = TOKEN_ELSE;
      }

      return token;
    }

    // Strings
    else if (state == STATE_START && current_character == '"') {
      lexer->position++;
      state = STATE_IN_STRING;
    } else if (state == STATE_IN_STRING && current_character != '"') {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_STRING && current_character == '"') {
      current_token_buffer[current_token_length] = '\0';
      lexer->position++;

      Token token;
      token.type = TOKEN_STRING_VALUE;
      token.value.string_value = strdup(current_token_buffer);
      return token;
    }

    // Integers
    else if (state == STATE_START && isdigit(current_character)) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_INTEGER;
    } else if (state == STATE_IN_INTEGER && isdigit(current_character)) {
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_INTEGER) {
      current_token_buffer[current_token_length] = '\0';

      Token token;
      token.type = TOKEN_INT_VALUE;
      token.value.int_value = atoi(current_token_buffer);
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
