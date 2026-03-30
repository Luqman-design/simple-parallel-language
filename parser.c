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
 * Validates that the token matches the expected type.
 * @param lexer Pointer to the lexer instance
 * @param expected The expected token type
 * @return The next token from the input
 */
static Token consume(Lexer *lexer, TokenType expected) {
  Token token = next_token(lexer);
  if (token.type != expected) {
    fprintf(stderr, "Parser error: Expected token type %d but got type %d\n",
            expected, token.type);
    exit(1);
  }
  return token;
}

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

static Node *parse_statement(Lexer *lexer);
static Node *parse_if_statement(Lexer *lexer);
static Node *parse_print(Lexer *lexer);
static Node *parse_var_declaration(Lexer *lexer);
static Node *parse_var_update(Lexer *lexer);
static Node *parse_for_loop(Lexer *lexer);
static Node *parse_block(Lexer *lexer);
static Node *parse_expression(Lexer *lexer);
static Node *parse_comparison(Lexer *lexer);
static Node *parse_term(Lexer *lexer);
static Node *parse_factor(Lexer *lexer);
static Node *parse_unary(Lexer *lexer);
static Node *parse_primary(Lexer *lexer);

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
    return parse_var_update(lexer);
  } else if (token.type == TOKEN_FOR) {
    perror("For loop not yet implemented");
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

  consume(lexer, TOKEN_IF); // if
  consume(lexer, TOKEN_LEFT_PAREN); // (
  node->body.if_statement.condition = parse_expression(lexer);
  consume(lexer, TOKEN_RIGHT_PAREN); // )

  // Parse then_block
  node->body.if_statement.then_branch = parse_block(lexer);

  // Parse optional else / else if
  node->body.if_statement.else_branch = NULL;
  if (peek(lexer).type == TOKEN_ELSE) {
    consume(lexer, TOKEN_ELSE); // else

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

  consume(lexer, TOKEN_PRINT); // print
  consume(lexer, TOKEN_LEFT_PAREN); // (
  node->body.print.print_value = parse_expression(lexer);
  consume(lexer, TOKEN_RIGHT_PAREN); // )
  consume(lexer, TOKEN_SEMICOLON); // ;

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

  Token type_token = peek(lexer);
  if (type_token.type == TOKEN_INT_TYPE) {
    consume(lexer, TOKEN_INT_TYPE);
    node->body.var_declaration.variable_type = TOKEN_INT_TYPE;
  } else if (type_token.type == TOKEN_STRING_TYPE) {
    consume(lexer, TOKEN_STRING_TYPE);
    node->body.var_declaration.variable_type = TOKEN_STRING_TYPE;
  }

  Token variable_name = consume(lexer, TOKEN_IDENTIFIER); // identifier
  consume(lexer, TOKEN_EQUAL); // =
  Node *expression = parse_expression(lexer);
  consume(lexer, TOKEN_SEMICOLON); // ;

  node->body.var_declaration.variable_name = variable_name.value.string_value;
  node->body.var_declaration.variable_value = expression;

  return node;
}

// VarUpdate ::= IDENTIFIER ("+" | "-")? "=" expression ";"
static Node *parse_var_update(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_VAR_UPDATE;

  Token variable_name = consume(lexer, TOKEN_IDENTIFIER); // identifier
  Token operator_token = peek(lexer);
  TokenType operator_type;
  if (operator_token.type == TOKEN_PLUS || operator_token.type == TOKEN_MINUS) {
    operator_type = operator_token.type;
    consume(lexer, operator_type);
  } else {
    operator_type = TOKEN_EQUAL;
  }
  consume(lexer, TOKEN_EQUAL);
  Node *expression = parse_expression(lexer);
  consume(lexer, TOKEN_SEMICOLON); // ;

  node->body.var_update.variable_name = variable_name.value.string_value;
  node->body.var_update._operator = operator_type;
  node->body.var_update.value = expression;

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
  consume(lexer,TOKEN_LEFT_CURLYBRACKET); // {
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_CURLYBRACKET) {
    if (statement_count >= statement_capacity) {
      statement_capacity *= 2;
      statements = realloc(statements, sizeof(Node *) * statement_capacity);
    }
    statements[statement_count] = parse_statement(lexer);
    statement_count++;
  }
  consume(lexer,TOKEN_RIGHT_CURLYBRACKET); // }

  node->body.block.statements = statements;
  node->body.block.statement_count = statement_count;

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
    TokenType operator_type = peek(lexer).type;
    consume(lexer, operator_type);
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
    consume(lexer, operator_token.type);
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
    TokenType operator_type = peek(lexer).type;
    consume(lexer, operator_type);
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
    TokenType operator_type = peek(lexer).type;
    consume(lexer, operator_type);
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
    consume(lexer,TOKEN_NOT); // !

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
  Token token = peek(lexer);

  if (token.type == TOKEN_INT_VALUE) {
    consume(lexer, TOKEN_INT_VALUE);
    Node *node = malloc(sizeof(Node));
    node->type = NODE_INT_VALUE;
    node->body.int_value.value = token.value.int_value;
    return node;
  } else if (token.type == TOKEN_STRING_VALUE) {
    consume(lexer, TOKEN_STRING_VALUE);
    Node *node = malloc(sizeof(Node));
    node->type = NODE_STRING_VALUE;
    node->body.string_value.value = token.value.string_value;
    return node;
  } else if (token.type == TOKEN_IDENTIFIER) {
    consume(lexer, TOKEN_IDENTIFIER);
    Node *node = malloc(sizeof(Node));
    node->type = NODE_IDENTIFIER;
    node->body.identifier.name = token.value.string_value;
    // The actual variable type will be assigned in the semantic analyzer
    return node;
  } else if (token.type == TOKEN_LEFT_PAREN) {
    consume(lexer, TOKEN_LEFT_PAREN);
    Node *expression = parse_expression(lexer);
    consume(lexer, TOKEN_RIGHT_PAREN); // )
    return expression;
  }

  // Unexpected token
  fprintf(stderr, "Parser error: Unexpected token\n");
  exit(1);
}
