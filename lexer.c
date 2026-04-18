/**
 * @file lexer.c
 * @brief Lexical analyzer (tokenizer) for FluCs language.
 *
 * The lexer takes raw source code as input and converts it into a sequence
 * of tokens. It uses a finite state machine approach to recognize different
 * token types including identifiers, keywords, operators, integers, strings,
 * and punctuation symbols.
 */

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

/**
 * Ensures the token buffer has room for at least one more character.
 * Doubles the buffer capacity if needed.
 */
static void ensure_buffer_capacity(char **buf, int *capacity, int len) {
  if (len >= *capacity - 1) {
    *capacity *= 2;
    *buf = realloc(*buf, (size_t)*capacity);
    if (!*buf) {
      fprintf(stderr, "Error: Memory allocation failed\n");
      exit(1);
    }
  }
}

/**
 * Frees memory owned by a token (string values for identifiers and strings).
 */
void free_token(Token *token) {
  if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_VALUE) {
    free(token->value.string_value);
    token->value.string_value = NULL;
  }
}

Token next_token(Lexer *lexer) {
  LexerState state = STATE_START;
  int buffer_capacity = 64;
  char *current_token_buffer = malloc((size_t)buffer_capacity);
  if (!current_token_buffer) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }
  int current_token_length = 0;

  while (lexer->position < lexer->length) {
    char current_character = peek(lexer);

    // Symbols
    if (state == STATE_START && current_character == ';') {
      Token token;
      token.type = TOKEN_SEMICOLON;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == '(') {
      Token token;
      token.type = TOKEN_LEFT_PAREN;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == ')') {
      Token token;
      token.type = TOKEN_RIGHT_PAREN;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == '{') {
      Token token;
      token.type = TOKEN_LEFT_CURLYBRACKET;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == '}') {
      Token token;
      token.type = TOKEN_RIGHT_CURLYBRACKET;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == '*') {
      Token token;
      token.type = TOKEN_MULTIPLY;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == '/') {
      Token token;
      token.type = TOKEN_DIVIDE;
      lexer->position++;
      free(current_token_buffer);
      return token;
    } else if (state == STATE_START && current_character == ',') {
      Token token;
      token.type = TOKEN_COMMA;
      lexer->position++;
      free(current_token_buffer);
      return token;
    }

    // Operators
    else if (state == STATE_START &&
             (current_character == '=' || current_character == '<' ||
              current_character == '>' || current_character == '&' ||
              current_character == '|' || current_character == '!' ||
              current_character == '+' || current_character == '-')) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_OPERATOR;
    } else if (state == STATE_IN_OPERATOR &&
               (current_character == '=' || current_character == '<' ||
                current_character == '>' || current_character == '&' ||
                current_character == '|' || current_character == '!' ||
                current_character == '+' || current_character == '-')) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
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
      } else if (strcmp(current_token_buffer, "+") == 0) {
        token.type = TOKEN_PLUS;
      } else if (strcmp(current_token_buffer, "-") == 0) {
        token.type = TOKEN_MINUS;
      } else if (strcmp(current_token_buffer, "+=") == 0) {
        token.type = TOKEN_PLUS_EQUAL;
      } else if (strcmp(current_token_buffer, "++") == 0) {
        token.type = TOKEN_PLUS_PLUS;
      } else if (strcmp(current_token_buffer, "-=") == 0) {
        token.type = TOKEN_MINUS_EQUAL;
      } else {
        token.type = TOKEN_ILLEGAL;
      }

      free(current_token_buffer);
      return token;
    }

    // Identifiers & Keywords
    else if (state == STATE_START &&
             (current_character == '_' || isalpha(current_character))) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_IDENTIFIER;
    } else if (state == STATE_IN_IDENTIFIER &&
               (current_character == '_' || isalpha(current_character) ||
                isdigit(current_character))) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_IDENTIFIER) {
      current_token_buffer[current_token_length] = '\0';

      Token token;
      token.type = TOKEN_IDENTIFIER;

      if (strcmp(current_token_buffer, "int") == 0) {
        token.type = TOKEN_INT_TYPE;
      } else if (strcmp(current_token_buffer, "string") == 0) {
        token.type = TOKEN_STRING_TYPE;
      } else if (strcmp(current_token_buffer, "void") == 0) {
        token.type = TOKEN_VOID;
      } else if (strcmp(current_token_buffer, "print") == 0) {
        token.type = TOKEN_PRINT;
      } else if (strcmp(current_token_buffer, "if") == 0) {
        token.type = TOKEN_IF;
      } else if (strcmp(current_token_buffer, "else") == 0) {
        token.type = TOKEN_ELSE;
      } else if (strcmp(current_token_buffer, "for") == 0) {
        token.type = TOKEN_FOR;
      } else if (strcmp(current_token_buffer, "func") == 0) {
        token.type = TOKEN_FUNCTION;
      } else if (strcmp(current_token_buffer, "process") == 0) {
        token.type = TOKEN_PROCESS;
      } else if (strcmp(current_token_buffer, "thread") == 0) {
        token.type = TOKEN_THREAD;
      } else if (strcmp(current_token_buffer, "return") == 0) {
        token.type = TOKEN_RETURN;
      } else if (strcmp(current_token_buffer, "await") == 0) {
        token.type = TOKEN_AWAIT;
      }

      if (token.type == TOKEN_IDENTIFIER) {
        token.value.string_value = strdup(current_token_buffer);
        if (!token.value.string_value) {
          fprintf(stderr, "Error: Memory allocation failed\n");
          exit(1);
        }
      }

      free(current_token_buffer);
      return token;
    }

    // Strings
    else if (state == STATE_START && current_character == '"') {
      lexer->position++;
      state = STATE_IN_STRING;
    } else if (state == STATE_IN_STRING && current_character != '"') {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_STRING && current_character == '"') {
      current_token_buffer[current_token_length] = '\0';
      lexer->position++;

      Token token;
      token.type = TOKEN_STRING_VALUE;
      token.value.string_value = strdup(current_token_buffer);
      if (!token.value.string_value) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
      }
      free(current_token_buffer);
      return token;
    }

    // Integers
    else if (state == STATE_START && isdigit(current_character)) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
      state = STATE_IN_INTEGER;
    } else if (state == STATE_IN_INTEGER && isdigit(current_character)) {
      ensure_buffer_capacity(&current_token_buffer, &buffer_capacity,
                             current_token_length);
      current_token_buffer[current_token_length] = current_character;
      current_token_length++;
      lexer->position++;
    } else if (state == STATE_IN_INTEGER) {
      current_token_buffer[current_token_length] = '\0';

      Token token;
      token.type = TOKEN_INT_VALUE;
      token.value.int_value = atoi(current_token_buffer);
      free(current_token_buffer);
      return token;
    }

    // Other (such as whitespaces)
    else {
      if (current_character == ' ' || current_character == '\t' ||
          current_character == '\n' || current_character == '\r') {
        lexer->position++;
      } else {
        fprintf(stderr, "Error: %c is an illegal symbol\n", current_character);
        exit(1);
      }
    }
  }

  Token token;
  token.type = TOKEN_EOF;
  free(current_token_buffer);
  return token;
}
