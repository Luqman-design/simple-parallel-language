/**
 * @file parser.c
 * @brief Top-Down Recursive descent parser for FluCs language.
 *
 * The parser takes a sequence of tokens from the lexer and builds an
 * Abstract Syntax Tree (AST). It uses recursive descent parsing with
 * precedence climbing for expressions. The grammar supports:
 * - Variable declarations (int/string)
 * - If/else statements (including else-if chains)
 * - Print statements
 * - Arithmetic, comparison, and logical operators
 * - Parenthesized expressions
 */

#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

statement ::= IfStatement | Print | VarDeclaration | ForLoop | VarUpdate

IfStatement ::= "if" "(" expression ")" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" IfStatement

Print ::= "print" "(" expression ")" ";"

VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression ";"

VarUpdate ::= IDENTIFIER ("+" | "-")? "=" expression ";"

ForLoop ::= "for" "(" VarDeclaration ";" expression ";" expression ")" "{"
statement*
"}"

expression ::= comparison (("&&" | "||") comparison)*

comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*

term ::= factor (("+" | "-") factor)*

factor ::= unary (("*" | "/") unary)*

unary ::= "!" unary | primary

primary ::= TOKEN_INT_VALUE | TOKEN_STRING_VALUE | TOKEN_IDENTIFIER | "("
expression ")"
 */

/**
 * Consumes and returns the next token, advancing the lexer position.
 * @param lexer Pointer to the lexer instance
 * @return The next token from the input
 */
static Token consume(Lexer *lexer) { return next_token(lexer); }

/**
 * Returns the next token without advancing the lexer position (lookahead).
 * Creates a copy of the lexer to preserve the original position.
 * @param lexer Pointer to the lexer instance
 * @return The next token (without consuming it)
 */
static Token peek(Lexer *lexer) {
  Lexer copy = *lexer;
  return next_token(&copy);
}

typedef struct {
  TokenType parameter_type;
  char *parameter_name;
} parameter;

static Node *parse_statement(Lexer *lexer);
static Node *parse_if_statement(Lexer *lexer);
static Node *parse_print(Lexer *lexer);
static Node *parse_var_declaration(Lexer *lexer);
static Node *parse_var_update(Lexer *lexer);
static Node *parse_for_loop(Lexer *lexer);
static Node *parse_function_declaration(Lexer *lexer);
static Node *parse_function_call(Lexer *lexer);
static Node *parse_function(Lexer *lexer);
static Node *parse_block(Lexer *lexer);
static Node *parse_expression(Lexer *lexer);
static Node *parse_comparison(Lexer *lexer);
static Node *parse_term(Lexer *lexer);
static Node *parse_factor(Lexer *lexer);
static Node *parse_unary(Lexer *lexer);
static Node *parse_primary(Lexer *lexer);
static parameter parse_parameter(Lexer *lexer);
static Node *parse_parameter_list(Lexer *lexer);

/**
 * Parses the entire program as a sequence of statements.
 * Continues parsing statements until end of input or illegal token.
 * @param lexer Pointer to the lexer instance
 * @return A NODE_PROGRAM node containing all parsed statements
 */
// Program ::= statement*
Node *parse(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_PROGRAM;

  int statement_count = 0;
  int statement_capacity = 4;
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (1) {
    Token token = peek(lexer);

    if (token.type == TOKEN_ILLEGAL || token.type == TOKEN_EOF) {
      break;
    }

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

/**
 * Parses a single statement by looking ahead to determine the statement type.
 * Dispatches to the appropriate statement parser based on the next token.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the parsed statement
 */
// statement ::= IfStatement | Print | VarDeclaration
static Node *parse_statement(Lexer *lexer) {
  Token token = peek(lexer);

  if (token.type == TOKEN_PRINT) {
    return parse_print(lexer);
  } else if (token.type == TOKEN_IF) {
    return parse_if_statement(lexer);
  } else if (token.type == TOKEN_INT_TYPE || token.type == TOKEN_STRING_TYPE) {
    return parse_var_declaration(lexer);
  } else if (token.type == TOKEN_IDENTIFIER) {
    Token next = peek_next(lexer);

    if (next.type == TOKEN_LEFT_PAREN) {
      Node *call = parse_function_call(lexer);
      consume(lexer); // ;
      return call;
    } else {
      return parse_var_update(lexer);
    }
  } else if (token.type == TOKEN_FUNCTION) {
    return parse_function(lexer);
  } else if (token.type == TOKEN_FOR) {
    return parse_for_loop(lexer);
  }

  fprintf(stderr, "Error: Unexpected token '%s' of type '%d'\n",
          token.value.string_value, token.type);
  exit(1);
}

/**
 * Parses an if statement including optional else branch.
 * Supports else-if chains by recursively parsing else-if as an IfStatement.
 * @param lexer Pointer to the lexer instance
 * @return A NODE_IF_STATEMENT node with condition, then_branch, and optional
 * else_branch
 */
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

/**
 * Parses a print statement: print(expression);
 * @param lexer Pointer to the lexer instance
 * @return A NODE_PRINT node containing the expression to print
 */
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

/**
 * Parses a variable declaration: type name = expression;
 * Supports both int and string types.
 * @param lexer Pointer to the lexer instance
 * @return A NODE_VAR_DECLARATION node with type, name, and value
 */
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

// VarUpdate ::= IDENTIFIER ("+" | "-")? "=" expression ";"
static Node *parse_var_update(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_VAR_UPDATE;

  Token variable_name = consume(lexer); // identifier
  TokenType operator_type = consume(lexer).type;
  Node *expression = parse_expression(lexer);
  consume(lexer); // ;

  node->body.var_update.variable_name = variable_name.value.string_value;
  node->body.var_update._operator = operator_type;
  node->body.var_update.value = expression;

  return node;
}

static Node *parse_function_call(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->body.function_call.argument_count = 0;

  node->body.function_call.function_name = consume(lexer).value.string_value;
  consume(lexer); // (

  if (peek(lexer).type == TOKEN_RIGHT_PAREN) {
    consume(lexer); // )
    return node;
  }

  node->body.function_call.arguments[0] = parse_expression(lexer);
  node->body.function_call.argument_count++;
  while (peek(lexer).type != TOKEN_RIGHT_PAREN) {
    int i = 1;

    node->body.function_call.arguments[i] = parse_expression(lexer);

    i++;
  }

  return node;
}

static Node *parse_function_declaration(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_FUNCTION_DECLARATION;
  char *function_name;

  consume(lexer);                                           // func
  strcpy(consume(lexer).value.string_value, function_name); // function name
  consume(lexer);                                           // (
  node = parse_parameter_list(lexer);
  consume(lexer); // )
  consume(lexer); // {
  node->body.function_declaration.body = parse_block(lexer);
  consume(lexer); // }
  strcpy(function_name, node->body.function_declaration.function_name);

  return node;
}

// for_loop ::= "for" "(" varDeclaration ";" expression ";" varUpdate ";" ")"
// Block
static Node *parse_for_loop(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_FOR_LOOP;

  consume(lexer); // for
  consume(lexer); // (
  node->body.for_loop.initializer = parse_var_declaration(lexer);
  consume(lexer); // ;
  node->body.for_loop.condition = parse_expression(lexer);
  consume(lexer); // ;
  node->body.for_loop.updater = parse_var_update(lexer);
  consume(lexer); // )

  // parse for_loop block
  node->body.for_loop.body = parse_block(lexer);

  return node;
}

/**
 * Parses a block of statements enclosed in curly braces { }.
 * @param lexer Pointer to the lexer instance
 * @return A NODE_BLOCK node containing the statements within the block
 */
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

static parameter parse_parameter(Lexer *lexer) {
  parameter parameter;

  Token parameter_type = peek(lexer);
  if (parameter_type.type != TOKEN_INT_TYPE &&
      parameter_type.type != TOKEN_STRING_TYPE) {
    perror("Error while parsing parameter");
  }

  parameter_type = consume(lexer);

  Token parameter_name = peek(lexer);

  if (parameter_name.type != TOKEN_IDENTIFIER) {
    perror("Error while parsing parameter");
  }

  parameter_name = consume(lexer);

  parameter.parameter_type = parameter_type.type;
  parameter.parameter_name = parameter_name.value.string_value;

  return parameter;
}

static Node *parse_parameter_list(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->body.function_declaration.parameter_count = 0;

  if (peek(lexer).type == TOKEN_RIGHT_PAREN) {
    return node;
  }
  parameter parameters[20];
  parameters[0] = parse_parameter(lexer);

  while (peek(lexer).type != TOKEN_RIGHT_PAREN) {
    int i = 1;

    parameters[i] = parse_parameter(lexer);

    node->body.function_declaration.parameter_count++;
    i++;
  }

  for (int i = 0; i < 20; i++) {
    memcpy(node->body.function_declaration.parameters, parameters,
           sizeof(parameters));
  }

  return node;
}

/**
 * Parses logical expressions (&&, ||).
 * Handles left-associative binary operations.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the expression (binary operation or single
 * operand)
 */
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

/**
 * Parses comparison expressions (==, !=, <, >, <=, >=).
 * Handles left-associative binary operations.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the comparison expression
 */
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

/**
 * Parses arithmetic terms (+, -).
 * Handles left-associative binary operations.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the term expression
 */
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

/**
 * Parses factors (*, /).
 * Handles left-associative binary operations.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the factor expression
 */
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

/**
 * Parses unary expressions (! operator).
 * Handles right-associative unary operations recursively.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the unary expression
 */
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

/**
 * Parses primary values: integers, strings, identifiers, and parenthesized
 * expressions. This is the base case for the expression parsing recursion.
 * @param lexer Pointer to the lexer instance
 * @return A Node representing the primary value
 */
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
    node->body.identifier.name = token.value.string_value;
    // The actual variable type will be assigned in the semantic analyzer
    return node;
  } else if (token.type == TOKEN_LEFT_PAREN) {
    Node *expression = parse_expression(lexer);
    consume(lexer); // )
    return expression;
  }

  // Unexpected token
  fprintf(stderr, "Parser error: Unexpected token\n");
  exit(1);
}
