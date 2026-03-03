#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

/* Example:
Input: [int, identifier, equal, string, semicolon]

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

primary ::= INT_VALUE | STRING_VALUE | IDENTIFIER | "(" expression ")"
*/

static Token consume(Lexer *lexer) { return next_token(lexer); }

static Node *parse_statement(Lexer *lexer, Token current);
static Node *parse_if_statement(Lexer *lexer);
static Node *parse_print(Lexer *lexer);
static Node *parse_var_declaration(Lexer *lexer, TokenType var_type);
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
  node->body.program.statements = NULL;
  node->body.program.count = 0;
  return node;
}

// statement ::= IfStatement | Print | VarDeclaration
static Node *parse_statement(Lexer *lexer, Token current) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_PRINT;
  return node;
}

// IfStatement ::= "if" "(" expression ")" "{" statement* "}"
//      | "if" "(" expression ")" "{" statement* "}" "else" "{" statement* "}"
//      | "if" "(" expression ")" "{" statement* "}" "else" IfStatement
static Node *parse_if_statement(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_IF_STATEMENT;
  node->body.if_statement.condition = NULL;
  node->body.if_statement.then_block.statements = NULL;
  node->body.if_statement.then_block.count = 0;
  node->body.if_statement.else_branch = NULL;
  return node;
}

// Print ::= "print" "(" expression ")" ";"
static Node *parse_print(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_PRINT;
  node->body.print.value = NULL;
  return node;
}

// VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression ";"
static Node *parse_var_declaration(Lexer *lexer, TokenType var_type) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_VAR_DECLARATION;
  node->body.var_declaration.var_type = var_type;
  node->body.var_declaration.name = "placeholder";
  node->body.var_declaration.value = NULL;
  return node;
}

// Block ::= "{" statement* "}"
static Node *parse_block(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BLOCK;
  node->body.block.statements = NULL;
  node->body.block.count = 0;
  return node;
}

// expression ::= comparison (("&&" | "||") comparison)*
static Node *parse_expression(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = AND;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*
static Node *parse_comparison(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = EQUAL_EQUAL;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// term ::= factor (("+" | "-") factor)*
static Node *parse_term(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = PLUS;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// factor ::= unary (("*" | "/") unary)*
static Node *parse_factor(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = MULTIPLY;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// unary ::= "!" unary | primary
static Node *parse_unary(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_UNARY_OPERATION;
  node->body.unary_operation._operator = NOT;
  node->body.unary_operation.operand = NULL;
  return node;
}

// primary ::= INT_VALUE | STRING_VALUE | IDENTIFIER | "(" expression ")"
static Node *parse_primary(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_INT_VALUE;
  node->body.int_value.value = 0;
  return node;
}
