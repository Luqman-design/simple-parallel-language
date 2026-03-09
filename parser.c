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

  parse_statement(lexer);

  node->body.program.statements = NULL;
  node->body.program.count = 0;
  return node;
}

// statement ::= IfStatement | Print | VarDeclaration
static Node *parse_statement(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));

  Token token = peek(lexer);
  if (token.type == PRINT) {
    Node *node = parse_print(lexer);
  } else if (token.type == IF) {
    Node *node = parse_if_statement(lexer);
  } else if (token.type == INT_TYPE || token.type == STRING_TYPE) {
    Node *node = parse_var_declaration(lexer);
  }

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
  printf("PRINT STATEMENT\n");

  Node *node = malloc(sizeof(Node));
  node->type = NODE_PRINT;

  consume(lexer); // print
  consume(lexer); // (
  Node *expression = parse_expression(lexer);
  node->body.print.value = expression;
  consume(lexer); // )
  consume(lexer); // ;

  if (expression->type == NODE_INT_VALUE) {
    printf("print(%d);\n", node->body.print.value->body.int_value.value);
  } else if (expression->type == NODE_STRING_VALUE) {
    printf("print(%s);\n", node->body.print.value->body.string_value.value);
  } else if (expression->type == NODE_IDENTIFIER) {
    printf("print(%s);\n", node->body.print.value->body.string_value.value);
  } 

  return node;
}

// VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression ";"
static Node *parse_var_declaration(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_VAR_DECLARATION;
  // node->body.var_declaration.var_type = var_type;

  Token token = consume(lexer); // Int / String

  if (token.type == INT_TYPE) {
    node->body.var_declaration.var_type = INT_TYPE;
  } else if (token.type == STRING_TYPE) {
    node->body.var_declaration.var_type = STRING_TYPE;
  }

  Token varName = consume(lexer); // Variable name
  consume(lexer); // =
  Node *expression = parse_expression(lexer);
  consume(lexer); // ;

  node->body.var_declaration.name = varName.value.string_value;
  node->body.var_declaration.value = expression;

  printf("%d %s = ", node->body.var_declaration.var_type, node->body.var_declaration.name);


  if (expression->type == NODE_INT_VALUE) {
    printf("%d;\n", node->body.var_declaration.value->body.int_value.value);
  } else if (expression->type == NODE_STRING_VALUE) {
    printf("%s;\n", node->body.var_declaration.value->body.string_value.value);
  } else if (expression->type == NODE_IDENTIFIER) {
    printf("%s;\n", node->body.var_declaration.value->body.string_value.value);
  } 
  return node;
}

// Block ::= "{" statement* "}"
static Node *parse_block(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_BLOCK;

  int amount_of_stm = 0;
  int stm_capacity = 3;
  consume(lexer); // {
  Node *statements[]= malloc(sizeof(Node)*stm_capacity);
  while (peek(lexer).type != RIGHT_CURLYBRACKET) {
    if (amount_of_stm >= stm_capacity) {
      stm_capacity *= 2;
      *statements = realloc(statements,sizeof(Node)*stm_capacity);
    }
    Node *statement = parse_statement(lexer);
    statements[amount_of_stm] = statement;
    amount_of_stm++;
  }
  consume(lexer); // }

  node->body.block.statements = statements;
  node->body.block.count = amount_of_stm;

  return node;
}

// expression ::= comparison (("&&" | "||") comparison)*
static Node *parse_expression(Lexer *lexer) {
  printf("EXPRESSION\n");

  Node *node = malloc(sizeof(Node));

  
  Node *comparison = parse_comparison(lexer);
  if (peek(lexer).type == AND || peek(lexer).type == OR) {
    consume(lexer);
    Node *comparison2 = parse_comparison(lexer);
    

  }


  return parse_comparison(lexer);

  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = AND;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*
static Node *parse_comparison(Lexer *lexer) {
  printf("COMPARISON\n");

  Node *node = malloc(sizeof(Node));

  return parse_term(lexer);

  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = EQUAL_EQUAL;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// term ::= factor (("+" | "-") factor)*
static Node *parse_term(Lexer *lexer) {
  printf("TERM\n");

  Node *node = malloc(sizeof(Node));

  return parse_factor(lexer);

  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = PLUS;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// factor ::= unary (("*" | "/") unary)*
static Node *parse_factor(Lexer *lexer) {
  printf("FACTOR\n");

  Node *node = malloc(sizeof(Node));

  return parse_unary(lexer);

  node->type = NODE_BINARY_OPERATION;
  node->body.binary_operation._operator = MULTIPLY;
  node->body.binary_operation.left = NULL;
  node->body.binary_operation.right = NULL;
  return node;
}

// unary ::= "!" unary | primary
static Node *parse_unary(Lexer *lexer) {
  printf("UNARY\n");

  Node *node = malloc(sizeof(Node));
  
  return parse_primary(lexer);
  
  node->type = NODE_UNARY_OPERATION;
  node->body.unary_operation._operator = NOT;
  node->body.unary_operation.operand = NULL;
  return node;
}

// primary ::= INT_VALUE | STRING_VALUE | IDENTIFIER | "(" expression ")"
static Node *parse_primary(Lexer *lexer) {
  printf("PRIMARY\n");

  Node *node = malloc(sizeof(Node));
  
  Token token = consume(lexer);
  
  if (token.type == INT_VALUE) {
    node->type = NODE_INT_VALUE;
    node->body.int_value.value = token.value.int_value;
  } else if (token.type == STRING_VALUE) {
    node->type = NODE_STRING_VALUE;
    node->body.string_value.value = token.value.string_value;
  } else if (token.type == IDENTIFIER) {
    node->type = NODE_IDENTIFIER;
    node->body.string_value.value = token.value.string_value;
  } else if (token.type == LEFT_PAREN) {
    Node *expression = parse_expression(lexer);
    consume(lexer); // )
    return expression;
  }

  return node;
}
