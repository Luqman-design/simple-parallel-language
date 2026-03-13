#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

/* Example:
Input: [TOKEN_INT_TYPE, TOKEN_IDENTIFIER, TOKEN_EQUAL, TOKEN_STRING_VALUE,
TOKEN_SEMICOLON]

Output:
Program(
    VarDeclaration(
        name: x
        type: string
        value: "abc"
    )
    Print(
        "Hello World!"
    )
    IfStatement(
        condition: 5 > 4
        Block(
            Print(
                "Hello again."
            )
        )
    )
)
*/

/* Grammar:
Program ::= statement*

statement ::= IfStatement | Print | VarDeclaration

IfStatement ::= "if" "(" expression ")" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" IfStatement

Print ::= "print" "(" expression ")" ";"

VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression ";"

expression ::= comparison (("&&" | "||") comparison)*

comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*

term ::= factor (("+" | "-") factor)*

factor ::= unary (("*" | "/") unary)*

unary ::= "!" unary | primary

primary ::= TOKEN_INT_VALUE | TOKEN_STRING_VALUE | TOKEN_IDENTIFIER | "("
expression ")"
*/

static Token consume(Lexer *lexer) { return next_token(lexer); }
static Token peek(Lexer *lexer) {
  Lexer copy = *lexer;
  return next_token(&copy);
}

static Node *parse_statement(Lexer *lexer);
static Node *parse_if_statement(Lexer *lexer);
static Node *parse_print(Lexer *lexer);
static Node *parse_var_declaration(Lexer *lexer);
static Node *parse_block(Lexer *lexer);
static Node *parse_expression(Lexer *lexer);
static Node *parse_comparison(Lexer *lexer);
static Node *parse_term(Lexer *lexer);
static Node *parse_factor(Lexer *lexer);
static Node *parse_unary(Lexer *lexer);
static Node *parse_primary(Lexer *lexer);

// Program ::= statement*
Node *parse(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_PROGRAM;

  int statement_count = 0;
  int statement_capacity = 4;
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (lexer->position < lexer->length - 1) {
    Token token = peek(lexer);
    if (token.type == TOKEN_ILLEGAL)
      break;

    if (statement_count >= statement_capacity) {
      statement_capacity *= 2;
      statements = realloc(statements, sizeof(Node *) * statement_capacity);
    }

    statements[statement_count] = parse_statement(lexer);
    statement_count++;
  }

  node->body.program.statements = statements;
  node->body.program.statement_count = statement_count;
  return node;
}

// statement ::= IfStatement | Print | VarDeclaration
static Node *parse_statement(Lexer *lexer) {
  Token token = peek(lexer);

  if (token.type == TOKEN_PRINT) {
    return parse_print(lexer);
  } else if (token.type == TOKEN_IF) {
    return parse_if_statement(lexer);
  } else if (token.type == TOKEN_INT_TYPE || token.type == TOKEN_STRING_TYPE) {
    return parse_var_declaration(lexer);
  }

  fprintf(stderr, "Error: Unexpected token '%s'\n", token.value.string_value);
  exit(1);
}

// IfStatement ::= "if" "(" expression ")" "{" statement* "}"
//      | "if" "(" expression ")" "{" statement* "}" "else" "{" statement* "}"
//      | "if" "(" expression ")" "{" statement* "}" "else" IfStatement
static Node *parse_if_statement(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_IF_STATEMENT;

  consume(lexer); // if
  consume(lexer); // (
  node->body.if_statement.condition = parse_expression(lexer);
  consume(lexer); // )

  // Parse then_block
  node->body.if_statement.then_branch = parse_block(lexer);

  // Parse optional else / else if
  node->body.if_statement.else_branch = NULL;
  if (peek(lexer).type == TOKEN_ELSE) {
    consume(lexer); // else

    if (peek(lexer).type == TOKEN_IF) {
      // else if
      node->body.if_statement.else_branch = parse_if_statement(lexer);
    } else {
      // else
      node->body.if_statement.else_branch = parse_block(lexer);
    }
  }

  return node;
}

// Print ::= "print" "(" expression ")" ";"
static Node *parse_print(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_PRINT;

  consume(lexer); // print
  consume(lexer); // (
  node->body.print.print_value = parse_expression(lexer);
  consume(lexer); // )
  consume(lexer); // ;

  return node;
}

// VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression ";"
static Node *parse_var_declaration(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_VAR_DECLARATION;

  Token type_token = consume(lexer); // int / string

  if (type_token.type == TOKEN_INT_TYPE) {
    node->body.var_declaration.variable_type = TOKEN_INT_TYPE;
  } else if (type_token.type == TOKEN_STRING_TYPE) {
    node->body.var_declaration.variable_type = TOKEN_STRING_TYPE;
  }

  Token variable_name = consume(lexer); // identifier
  consume(lexer);                       // =
  Node *expression = parse_expression(lexer);
  consume(lexer); // ;

  node->body.var_declaration.variable_name = variable_name.value.string_value;
  node->body.var_declaration.variable_value = expression;

  return node;
}

// Block ::= "{" statement* "}"
static Node *parse_block(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BLOCK;

  int statement_count = 0;
  int statement_capacity = 4;
  consume(lexer); // {
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_CURLYBRACKET) {
    if (statement_count >= statement_capacity) {
      statement_capacity *= 2;
      statements = realloc(statements, sizeof(Node *) * statement_capacity);
    }
    statements[statement_count] = parse_statement(lexer);
    statement_count++;
  }
  consume(lexer); // }

  node->body.block.statements = statements;
  node->body.block.statement_count = statement_count;

  return node;
}

// expression ::= comparison (("&&" | "||") comparison)*
static Node *parse_expression(Lexer *lexer) {
  Node *left_operand = parse_comparison(lexer);

  while (peek(lexer).type == TOKEN_AND || peek(lexer).type == TOKEN_OR) {
    TokenType operator_type = consume(lexer).type;
    Node *right_operand = parse_comparison(lexer);

    Node *node = malloc(sizeof(Node));
    node->type = NODE_BINARY_OPERATION;
    node->body.binary_operation.operator_type = operator_type;
    node->body.binary_operation.left_operand = left_operand;
    node->body.binary_operation.right_operand = right_operand;
    left_operand = node;
  }

  return left_operand;
}

// comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*
static Node *parse_comparison(Lexer *lexer) {
  Node *left_operand = parse_term(lexer);

  Token operator_token = peek(lexer);
  while (operator_token.type == TOKEN_EQUAL_EQUAL ||
         operator_token.type == TOKEN_NOT_EQUAL ||
         operator_token.type == TOKEN_LESS ||
         operator_token.type == TOKEN_GREATER ||
         operator_token.type == TOKEN_LESS_EQUAL ||
         operator_token.type == TOKEN_GREATER_EQUAL) {
    consume(lexer);
    Node *right_operand = parse_term(lexer);

    Node *node = malloc(sizeof(Node));
    node->type = NODE_BINARY_OPERATION;
    node->body.binary_operation.operator_type = operator_token.type;
    node->body.binary_operation.left_operand = left_operand;
    node->body.binary_operation.right_operand = right_operand;
    left_operand = node;

    operator_token = peek(lexer);
  }

  return left_operand;
}

// term ::= factor (("+" | "-") factor)*
static Node *parse_term(Lexer *lexer) {
  Node *left_operand = parse_factor(lexer);

  while (peek(lexer).type == TOKEN_PLUS || peek(lexer).type == TOKEN_MINUS) {
    TokenType operator_type = consume(lexer).type;
    Node *right_operand = parse_factor(lexer);

    Node *node = malloc(sizeof(Node));
    node->type = NODE_BINARY_OPERATION;
    node->body.binary_operation.operator_type = operator_type;
    node->body.binary_operation.left_operand = left_operand;
    node->body.binary_operation.right_operand = right_operand;
    left_operand = node;
  }

  return left_operand;
}

// factor ::= unary (("*" | "/") unary)*
static Node *parse_factor(Lexer *lexer) {
  Node *left_operand = parse_unary(lexer);

  while (peek(lexer).type == TOKEN_MULTIPLY ||
         peek(lexer).type == TOKEN_DIVIDE) {
    TokenType operator_type = consume(lexer).type;
    Node *right_operand = parse_unary(lexer);

    Node *node = malloc(sizeof(Node));
    node->type = NODE_BINARY_OPERATION;
    node->body.binary_operation.operator_type = operator_type;
    node->body.binary_operation.left_operand = left_operand;
    node->body.binary_operation.right_operand = right_operand;
    left_operand = node;
  }

  return left_operand;
}

// unary ::= "!" unary | primary
static Node *parse_unary(Lexer *lexer) {
  if (peek(lexer).type == TOKEN_NOT) {
    consume(lexer); // !

    Node *node = malloc(sizeof(Node));
    node->type = NODE_UNARY_OPERATION;
    node->body.unary_operation.operator_type = TOKEN_NOT;
    node->body.unary_operation.operand = parse_unary(lexer);
    return node;
  }

  return parse_primary(lexer);
}

// primary ::= TOKEN_INT_VALUE | TOKEN_STRING_VALUE | TOKEN_IDENTIFIER | "("
// expression ")"
static Node *parse_primary(Lexer *lexer) {
  Token token = consume(lexer);

  if (token.type == TOKEN_INT_VALUE) {
    Node *node = malloc(sizeof(Node));
    node->type = NODE_INT_VALUE;
    node->body.int_value.value = token.value.int_value;
    return node;
  } else if (token.type == TOKEN_STRING_VALUE) {
    Node *node = malloc(sizeof(Node));
    node->type = NODE_STRING_VALUE;
    node->body.string_value.value = token.value.string_value;
    return node;
  } else if (token.type == TOKEN_IDENTIFIER) {
    Node *node = malloc(sizeof(Node));
    node->type = NODE_IDENTIFIER;
    node->body.string_value.value = token.value.string_value;
    return node;
  } else if (token.type == TOKEN_LEFT_PAREN) {
    Node *expression = parse_expression(lexer);
    consume(lexer); // )
    return expression;
  }

  // Unexpected token
  return NULL;
}
