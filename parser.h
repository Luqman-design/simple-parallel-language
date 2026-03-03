#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"

typedef enum {
  NODE_PROGRAM,
  NODE_BLOCK,
  NODE_VAR_DECLARATION,
  NODE_IF_STATEMENT,
  NODE_PRINT,
  NODE_BINARY_OPERATION,
  NODE_UNARY_OPERATION,
  NODE_INT_VALUE,
  NODE_STRING_VALUE,
  NODE_IDENTIFIER,
} NodeType;

typedef struct Node {
  NodeType type;
  union {
    struct {
      struct Node **statements;
      int count;
    } program;
    struct {
      struct Node **statements;
      int count;
    } block;
    struct {
      TokenType var_type;
      char *name;
      struct Node *value;
    } var_declaration;
    struct {
      struct Node *condition;
      struct {
        struct Node **statements;
        int count;
      } then_block;
      struct Node *else_branch;
    } if_statement;
    struct {
      struct Node *value;
    } print;
    struct {
      TokenType _operator;
      struct Node *left;
      struct Node *right;
    } binary_operation;
    struct {
      TokenType _operator;
      struct Node *operand;
    } unary_operation;
    struct {
      int value;
    } int_value;
    struct {
      char *value;
    } string_value;
  } body;
} Node;

Node *parse(Lexer *lexer);

#endif
