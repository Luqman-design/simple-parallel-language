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

statement ::= IfStatement | Print | VarDeclaration | ForLoop | VarUpdate |
Function | FunctionCall | ReturnStatement

Function ::= "func" ("int" | "string") IDENTIFIER "(" ParameterList ")" "{"
statement* ReturnStatement "}"

ParameterList ::= ( ("int" | "string") IDENTIFIER ("," ("int" | "string")
IDENTIFIER)* )?

ReturnStatement ::= "return" expression ";"

CallExpression ::= (thread | process)? IDENTIFIER "(" ArgumentList ")"
FunctionCall    ::= CallExpression ";"
AwaitStatement ::= "await" "{" (IDENTIFIER ";")* "}" // Note: IDENTIFIER should
only work for function names.

ArgumentList ::= (expression ("," expression)*)?

IfStatement ::= "if" "(" expression ")" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" "{" statement* "}"
         | "if" "(" expression ")" "{" statement* "}" "else" IfStatement

Print ::= "print" "(" expression ")" ";"

VarDeclaration ::= ("int" | "string") IDENTIFIER "=" expression | CallExpression
";"

VarUpdate ::= IDENTIFIER ("+" | "-")? "=" expression | CallExpression ";"

ForLoop ::= "for" "(" VarDeclaration ";" expression ";" expression ")" "{"
statement*
"}"

expression ::= comparison (("&&" | "||") comparison)*

comparison ::= term (("==" | "!=" | "<" | ">" | "<=" | ">=") term)*

term ::= factor (("+" | "-") factor)*

factor ::= unary (("*" | "/") unary)*

unary ::= "!" unary | primary

primary ::= TOKEN_INT_VALUE | TOKEN_STRING_VALUE | IDENTIFIER | "(" expression
")"
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
static Node *parse_var_update_with_semicolon(Lexer *lexer);
static Node *parse_for_loop(Lexer *lexer);
static Node *parse_function(Lexer *lexer);
static Node *parse_return_statement(Lexer *lexer);
static Node *parse_function_call(Lexer *lexer);
static Node *parse_block(Lexer *lexer);
static Node *parse_expression(Lexer *lexer);
static Node *parse_comparison(Lexer *lexer);
static Node *parse_term(Lexer *lexer);
static Node *parse_factor(Lexer *lexer);
static Node *parse_unary(Lexer *lexer);
static Node *parse_primary(Lexer *lexer);

static int peek_after_identifier_is_left_paren(Lexer *lexer) {
  Lexer copy = *lexer;
  next_token(&copy);
  return next_token(&copy).type == TOKEN_LEFT_PAREN;
}

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
    if (peek_after_identifier_is_left_paren(lexer)) {
      return parse_function_call(lexer);
    }
    return parse_var_update_with_semicolon(lexer);
  } else if (token.type == TOKEN_FOR) {
    return parse_for_loop(lexer);
  } else if (token.type == TOKEN_FUNCTION) {
    return parse_function(lexer);
  } else if (token.type == TOKEN_RETURN) {
    return parse_return_statement(lexer);
  } else if (token.type == TOKEN_PROCESS) {
    return parse_function_call(lexer);
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

  consume(lexer, TOKEN_IF);         // if
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

  consume(lexer, TOKEN_PRINT);      // print
  consume(lexer, TOKEN_LEFT_PAREN); // (
  node->body.print.print_value = parse_expression(lexer);
  consume(lexer, TOKEN_RIGHT_PAREN); // )
  consume(lexer, TOKEN_SEMICOLON);   // ;

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
  consume(lexer, TOKEN_EQUAL);                            // =
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

  Token variable_name = consume(lexer, TOKEN_IDENTIFIER);
  Token operator_token = peek(lexer);
  TokenType operator_type;
  if (operator_token.type == TOKEN_PLUS_EQUAL) {
    operator_type = TOKEN_PLUS_EQUAL;
    consume(lexer, TOKEN_PLUS_EQUAL);
  } else if (operator_token.type == TOKEN_MINUS_EQUAL) {
    operator_type = TOKEN_MINUS_EQUAL;
    consume(lexer, TOKEN_MINUS_EQUAL);
  } else if (operator_token.type == TOKEN_PLUS_PLUS) {
    operator_type = TOKEN_PLUS_PLUS;
    consume(lexer, TOKEN_PLUS_PLUS);
  } else {
    operator_type = TOKEN_EQUAL;
    consume(lexer, TOKEN_EQUAL);
  }
  Node *expression = parse_expression(lexer);

  node->body.var_update.variable_name = variable_name.value.string_value;
  node->body.var_update._operator = operator_type;
  node->body.var_update.value = expression;

  return node;
}

static Node *parse_var_update_with_semicolon(Lexer *lexer) {
  Node *node = parse_var_update(lexer);
  consume(lexer, TOKEN_SEMICOLON);
  return node;
}

// for_loop ::= "for" "(" varDeclaration ";" expression ";" varUpdate ")" Block
static Node *parse_for_loop(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_FOR_LOOP;

  consume(lexer, TOKEN_FOR);
  consume(lexer, TOKEN_LEFT_PAREN);
  node->body.for_loop.initializer = parse_var_declaration(lexer);
  node->body.for_loop.condition = parse_expression(lexer);
  consume(lexer, TOKEN_SEMICOLON);
  node->body.for_loop.updater = parse_var_update(lexer);
  consume(lexer, TOKEN_RIGHT_PAREN);

  // parse for_loop block
  node->body.for_loop.body = parse_block(lexer);

  return node;
}

static Node *parse_function(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_FUNCTION;

  consume(lexer, TOKEN_FUNCTION);

  Token return_type_token = peek(lexer);
  if (return_type_token.type == TOKEN_INT_TYPE) {
    consume(lexer, TOKEN_INT_TYPE);
    node->body.function.return_type = TOKEN_INT_TYPE;
  } else if (return_type_token.type == TOKEN_STRING_TYPE) {
    consume(lexer, TOKEN_STRING_TYPE);
    node->body.function.return_type = TOKEN_STRING_TYPE;
  }

  Token name_token = consume(lexer, TOKEN_IDENTIFIER);
  node->body.function.name = name_token.value.string_value;

  consume(lexer, TOKEN_LEFT_PAREN);

  int param_count = 0;
  int param_capacity = 4;
  FunctionParam *params = malloc(sizeof(*params) * param_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_PAREN) {
    if (param_count >= param_capacity) {
      param_capacity *= 2;
      params = realloc(params, sizeof(*params) * param_capacity);
    }

    Token param_type_token = peek(lexer);
    if (param_type_token.type == TOKEN_INT_TYPE) {
      consume(lexer, TOKEN_INT_TYPE);
      params[param_count].type = TOKEN_INT_TYPE;
    } else if (param_type_token.type == TOKEN_STRING_TYPE) {
      consume(lexer, TOKEN_STRING_TYPE);
      params[param_count].type = TOKEN_STRING_TYPE;
    }

    Token param_name_token = consume(lexer, TOKEN_IDENTIFIER);
    params[param_count].name = param_name_token.value.string_value;
    param_count++;

    if (peek(lexer).type == TOKEN_COMMA) {
      consume(lexer, TOKEN_COMMA);
    }
  }

  consume(lexer, TOKEN_RIGHT_PAREN);

  node->body.function.params = params;
  node->body.function.param_count = param_count;

  consume(lexer, TOKEN_LEFT_CURLYBRACKET);

  int statement_count = 0;
  int statement_capacity = 4;
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_CURLYBRACKET) {
    if (statement_count >= statement_capacity) {
      statement_capacity *= 2;
      statements = realloc(statements, sizeof(Node *) * statement_capacity);
    }
    statements[statement_count] = parse_statement(lexer);
    statement_count++;
  }

  consume(lexer, TOKEN_RIGHT_CURLYBRACKET);

  node->body.function.statements = statements;
  node->body.function.statement_count = statement_count;

  return node;
}

static Node *parse_return_statement(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_RETURN_STATEMENT;

  consume(lexer, TOKEN_RETURN);
  node->body.return_statement.expression = parse_expression(lexer);
  consume(lexer, TOKEN_SEMICOLON);

  return node;
}

static Node *parse_function_call(Lexer *lexer) {
  Node *node = malloc(sizeof(Node));
  node->type = NODE_FUNCTION_CALL;

  node->body.function_call.type = 0;
  if (peek(lexer).type == TOKEN_PROCESS) {
    node->body.function_call.type = 2;
    consume(lexer, TOKEN_PROCESS);
  }

  Token name_token = consume(lexer, TOKEN_IDENTIFIER);
  node->body.function_call.name = name_token.value.string_value;

  consume(lexer, TOKEN_LEFT_PAREN);

  int arg_count = 0;
  int arg_capacity = 4;
  Node **arguments = malloc(sizeof(Node *) * arg_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_PAREN) {
    if (arg_count >= arg_capacity) {
      arg_capacity *= 2;
      arguments = realloc(arguments, sizeof(Node *) * arg_capacity);
    }
    arguments[arg_count] = parse_expression(lexer);
    arg_count++;

    if (peek(lexer).type == TOKEN_COMMA) {
      consume(lexer, TOKEN_COMMA);
    }
  }

  consume(lexer, TOKEN_RIGHT_PAREN);
  consume(lexer, TOKEN_SEMICOLON);

  node->body.function_call.arguments = arguments;
  node->body.function_call.argument_count = arg_count;

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
  consume(lexer, TOKEN_LEFT_CURLYBRACKET); // {
  Node **statements = malloc(sizeof(Node *) * statement_capacity);

  while (peek(lexer).type != TOKEN_RIGHT_CURLYBRACKET) {
    if (statement_count >= statement_capacity) {
      statement_capacity *= 2;
      statements = realloc(statements, sizeof(Node *) * statement_capacity);
    }
    statements[statement_count] = parse_statement(lexer);
    statement_count++;
  }
  consume(lexer, TOKEN_RIGHT_CURLYBRACKET); // }

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
    consume(lexer, TOKEN_NOT); // !

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
    if (peek(lexer).type == TOKEN_LEFT_PAREN) {
      Node *node = malloc(sizeof(Node));
      node->type = NODE_FUNCTION_CALL;
      node->body.function_call.name = token.value.string_value;

      consume(lexer, TOKEN_LEFT_PAREN);

      int arg_count = 0;
      int arg_capacity = 4;
      Node **arguments = malloc(sizeof(Node *) * arg_capacity);

      while (peek(lexer).type != TOKEN_RIGHT_PAREN) {
        if (arg_count >= arg_capacity) {
          arg_capacity *= 2;
          arguments = realloc(arguments, sizeof(Node *) * arg_capacity);
        }
        arguments[arg_count] = parse_expression(lexer);
        arg_count++;

        if (peek(lexer).type == TOKEN_COMMA) {
          consume(lexer, TOKEN_COMMA);
        }
      }

      consume(lexer, TOKEN_RIGHT_PAREN);

      node->body.function_call.arguments = arguments;
      node->body.function_call.argument_count = arg_count;

      return node;
    }
    Node *node = malloc(sizeof(Node));
    node->type = NODE_IDENTIFIER;
    node->body.identifier.name = token.value.string_value;
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
