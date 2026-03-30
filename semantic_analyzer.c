// Will: I think there has been some confusion, the TOKEN_INT_TYPE is to
// describe "int" keyword and the variable 'x', and TOKEN_INT_VALUE is to
// describe "123". But i guess it doesnt matter.

#include "lexer.h"
#include "parser.h"
#include "uthash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Case 1:
string a--- program ---a
            /      \
          if       if ---- a
         /  \     /  \
      int a  a  int a \
                    float a

Case 2:
string a,
string b,
if {
  int a
  a = 10
  b = "gg"
}
if {
  string a = "abc"
}
if {
  a = "test"
},
a

Solution steps to case 2:
steps:
1. global scope
[
  scope 0:
  { (a: str), (b: str) }
]

2. enters if
[
  scope 0:
  { (a: str), (b: str) },

  scope 1: <- lookup here first, the lookup parent scopes.
  { (a: int) }
]

3. exits if
[
  scope 0:
  { (a: str), (b: str) }
]

*/
#define MAX_SCOPE 100

TokenType analyze_expression(Node *node);
TokenType analyze_binary_operation(Node *node);

typedef struct {
  char name[32];
  TokenType type;
  UT_hash_handle hh;
} VariableEntry;

VariableEntry *scopes[MAX_SCOPE];
int scope_top = -1;

void enter_scope() {
  scope_top++;
  scopes[scope_top] = NULL;
}

void exit_scope() {
  VariableEntry *current, *tmp;

  HASH_ITER(hh, scopes[scope_top], current, tmp) {
    HASH_DEL(scopes[scope_top], current);
    free(current);
  }

  scope_top--;
}

void insert_variable(const char *name, TokenType type) {
  VariableEntry *variable;
  HASH_FIND_STR(scopes[scope_top], name, variable);

  if (variable != NULL) {
    printf("Semantic error: Variable %s is already declared\n", name);
    exit(1);
  }

  variable = malloc(sizeof(VariableEntry));
  strcpy(variable->name, name);
  variable->type = type;

  HASH_ADD_STR(scopes[scope_top], name, variable);
}

VariableEntry *lookup_variable(const char *name) {
  // Loops through each scope, starting from the leaf/top (most nested scope)
  for (int i = scope_top; i >= 0; i--) {
    VariableEntry *variable;
    HASH_FIND_STR(scopes[i], name, variable);
    if (variable) {
      return variable;
    }
  }

  return NULL;
}

void check_operators(Node *node, char *error_message) {
  TokenType left_operand =
      analyze_expression(node->body.binary_operation.left_operand);
  TokenType right_operand =
      analyze_expression(node->body.binary_operation.right_operand);

  if (left_operand != TOKEN_INT_TYPE &&
      right_operand != TOKEN_INT_TYPE) { // Ved ikke om dette virke for hvis det
    // er en identifier?? Ex. int x, y =
    // 10; x + y; (fejl måske????)
    // Will: i analyze_expression under NODE_IDENTIFIER, giver vi 'x' or 'y'
    // deres int typer. Det skal også tjekkes her. (Tror ikke de har fået deres
    // typer endnu på det her tidspunkt i koden. Det skal nok gøres her.)
    printf("%s", error_message);
    exit(1);
  }
}

TokenType analyze_binary_operation(Node *node) {
  TokenType left_operand =
      analyze_expression(node->body.binary_operation.left_operand);
  TokenType right_operand =
      analyze_expression(node->body.binary_operation.right_operand);

  switch (node->body.binary_operation.operator_type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_MULTIPLY:
  case TOKEN_DIVIDE:
    if (left_operand != TOKEN_INT_TYPE || right_operand != TOKEN_INT_TYPE) {
      printf(
          "Semantic error: Arithmetic operations only allowed for type int\n");
      exit(1);
    }
    break;
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_LESS:
    if (left_operand != TOKEN_INT_TYPE || right_operand != TOKEN_INT_TYPE) {
      printf("Semantic error: comparisons only allowed for type int\n");
      exit(1);
    }
    break;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_NOT_EQUAL:
    if (left_operand != right_operand) {
      printf("Semantic error: equality operations must be on same type\n");
      exit(1);
    }
    break;
  case TOKEN_AND:
  case TOKEN_OR:
    if (left_operand != TOKEN_INT_TYPE || right_operand != TOKEN_INT_TYPE) {
      printf("Semantic error: logical operations only valid for type int on "
             "type int\n");
      exit(1);
    }
    break;
  default:
    printf("Semantic error: Unsupported operator in binary operation\n");
    exit(1);
  }

  return TOKEN_INT_TYPE;
}

TokenType analyze_expression(Node *node) {
  switch (node->type) {
  case NODE_INT_VALUE:
    return TOKEN_INT_TYPE;
  case NODE_STRING_VALUE:
    return TOKEN_STRING_TYPE;
  case NODE_IDENTIFIER: {
    VariableEntry *variable = lookup_variable(node->body.identifier.name);
    if (variable == NULL) {
      printf("Semantic error: Variable %s is not declared\n",
             node->body.identifier.name);
      exit(1);
    }

    // The parser does not know what type 'x' is.
    // Therefore after we have found the variable declaration,
    // we can then assign the type to all calls to 'int x;'.
    // Example of problem:
    // int x;
    // x <-- what type is x?
    // So this is the most important part of the semantic analysis:
    node->body.identifier.type = variable->type;

    return variable->type;
  }
  case NODE_BINARY_OPERATION:
    return analyze_binary_operation(node);
  case NODE_UNARY_OPERATION: {
    TokenType operand_type =
        analyze_expression(node->body.unary_operation.operand);
    if (operand_type != TOKEN_INT_TYPE) {
      printf("Semantic error: Unary operator ! requires an integer operand\n");
      exit(1);
    }
    return TOKEN_INT_TYPE;
  }
  default:
    printf("Semantic error: Unsupported node type in expression analysis\n");
    exit(1);
  }
}

void analyze_node(Node *node) {
  switch (node->type) {
  case NODE_PROGRAM:
    enter_scope();
    for (int i = 0; i < node->body.program.statement_count; i++) {
      analyze_node(node->body.program.statements[i]);
    }
    exit_scope();
    break;
  case NODE_BLOCK:
    enter_scope();
    for (int i = 0; i < node->body.block.statement_count; i++) {
      analyze_node(node->body.block.statements[i]);
    }
    exit_scope();
    break;
  case NODE_VAR_DECLARATION: {
    TokenType variable_type = node->body.var_declaration.variable_type;
    char *variable_name = node->body.var_declaration.variable_name;
    TokenType expression_type =
        analyze_expression(node->body.var_declaration.variable_value);

    insert_variable(variable_name, variable_type);

    if (expression_type != variable_type) {
      printf("Semantic error: Type mismatch in assignment to %s\n",
             variable_name);
      exit(1);
    }
    break;
  }
  case NODE_VAR_UPDATE: {
    TokenType variable_type = analyze_expression(node->body.var_update.value);
    char *variable_name = node->body.var_update.variable_name;

    VariableEntry *variable = lookup_variable(variable_name);

    if (variable_type != variable->type) {
      printf("Semantic error: Type mismatch in variable update on %s\n",
             variable_name);
      exit(1);
    }
    break;
  }

  case NODE_IF_STATEMENT: {
    TokenType condition_type =
        analyze_expression(node->body.if_statement.condition);

    if (condition_type != TOKEN_INT_TYPE) {
      printf("Semantic error: if condition must be integer\n");
      exit(1);
    }
    analyze_node(node->body.if_statement.then_branch);
    if (node->body.if_statement.else_branch) {
      analyze_node(node->body.if_statement.else_branch);
    }
    break;
  }

  case NODE_FOR_LOOP:
    TokenType for_loop_initiializer =
        analyze_expression(node->body.for_loop.initializer);

  case NODE_PRINT:
    analyze_expression(node->body.print.print_value);
    break;
  default:
    printf("Semantic error: Unsupported node type in semantic analysis\n");
    exit(1);
  }
}

void semantic_analysis(Node *root) { analyze_node(root); }

/*
  TO DO:
  Lige så compare den bare: left_type == right_type.

  Vi skal tjekke operator type og så tjekke at det er de rigtige typer for den
  operator. F.eks.:

  1. Arithmetic (+, -, *, /)
     Kun integers tilladt
     Return type: int

  2. Comparison (<, >, <=, >=)
     Kun integers tilladt
     Return type: int (bruger int, da vi ikke har boolean)

  3. Equality (==, !=)
     Ens typer (int == int, string == string)
     Return type: int

  4. Logical (&&, ||)
     Kun integers tilladt
     Return type: int
*/
