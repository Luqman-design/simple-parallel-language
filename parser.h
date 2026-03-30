#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"

typedef enum {
  NODE_PROGRAM,
  NODE_BLOCK,
  NODE_VAR_DECLARATION,
  NODE_VAR_UPDATE,
  NODE_IF_STATEMENT,
  NODE_PRINT,
  NODE_FOR_LOOP,
  NODE_FUNCTION,
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
      int statement_count;
    } program;
    struct {
      struct Node **statements;
      int statement_count;
    } block;
    struct {
      TokenType variable_type;
      char *variable_name;
      struct Node *variable_value;
    } var_declaration;
    struct {
      char *variable_name;
      TokenType _operator;
      struct Node *value;
    } var_update;
    struct {
      struct Node *condition;
      struct Node *then_branch;
      struct Node *else_branch;
    } if_statement;
    struct {
      struct Node *initializer;
      struct Node *condition;
      struct Node *updater;
      struct Node *body;
    } for_loop;
    struct {
      struct Node *print_value;
    } print;
    struct {
      TokenType operator_type;
      struct Node *left_operand;
      struct Node *right_operand;
    } binary_operation;
    struct {
      TokenType operator_type;
      struct Node *operand;
    } unary_operation;
    struct {
      int value;
    } int_value;
    struct {
      char *value;
    } string_value;
    struct {
      TokenType type; // Assigned at semantic analysis
      char *name;
    } identifier;
  } body;
} Node;

Node *parse(Lexer *lexer);

#endif
