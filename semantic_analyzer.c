#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uthash.h"

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
#define MAX_STATE 100

typedef struct {
  char name[32];
  TokenType type;
  UT_hash_handle hh;
} VariableEntry;

VariableEntry *states[MAX_STATE];
int state_top = -1;

void enter_state() {
  state_top++;
  states[state_top] = NULL;
}

void exit_state()
{
  VariableEntry *current, *tmp;

  HASH_ITER(hh, states[state_top], current, tmp)
  {
    HASH_DEL(states[state_top], current);
    free(current);
  }

  state_top--;
}

void insert_variable(const char *name, TokenType type) {
 
  VariableEntry *v;
  HASH_FIND_STR(states[state_top], name, v);

  if (v != NULL) {
    printf("Semantic error: Variable %s is already declared\n", name);
    exit(1);
  }
  v = malloc(sizeof(VariableEntry));
  strcpy(v->name, name);
  v->type = type;

  HASH_ADD_STR(states[state_top], name, v);
}

VariableEntry *lookup_variable(const char *name) {
  // Loops through each state, starting from the leaf/top
  for (int i = state_top; i >= 0; i--) {
    VariableEntry *v;
    HASH_FIND_STR(states[i], name, v);
      if (v)
      return v;
  }

  return NULL;
}

TokenType analyze_expression(Node *node) {
  switch (node->type) {
    case NODE_INT_VALUE:
      return TOKEN_INT_TYPE;
    case NODE_STRING_VALUE:
      return TOKEN_STRING_TYPE;
    case NODE_IDENTIFIER: {
      VariableEntry *v = lookup_variable(node->body.identifier.name);
      if (v == NULL) {
        printf("Semantic error: Variable %s is not declared\n", node->body.identifier.name);
        exit(1);
      }
      return v->type;
    }
    case NODE_BINARY_OPERATION: {
      TokenType left_type = analyze_expression(node->body.binary_operation.left_operand);
      TokenType right_type = analyze_expression(node->body.binary_operation.right_operand);
      if (left_type != right_type) { // Skal opdateres til at tjekke operator type
        printf("Semantic error: Type mismatch in binary operation\n");
        exit(1);
      }
      return left_type; 
    }
    case NODE_UNARY_OPERATION: {
      TokenType operand_type = analyze_expression(node->body.unary_operation.operand);
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
      enter_state();
      for (int i = 0; i < node->body.program.statement_count; i++) {
        analyze_node(node->body.program.statements[i]);
      }
      exit_state();
      break;
    case NODE_BLOCK:
      enter_state();
      for (int i = 0; i < node->body.block.statement_count; i++) {
        analyze_node(node->body.block.statements[i]);
      }
      exit_state();
      break;
    case NODE_VAR_DECLARATION: {
      TokenType variable_type = node->body.var_declaration.variable_type;
      char *variable_name = node->body.var_declaration.variable_name;
      TokenType expr_type = analyze_expression(node->body.var_declaration.variable_value);

      insert_variable(variable_name, variable_type);

      if (expr_type != variable_type) {
        printf("Semantic error: Type mismatch in assignment to %s\n", variable_name);
        exit(1);
      }
      break;
    }
    case NODE_IF_STATEMENT: { 
      TokenType cond_type = analyze_expression(node->body.if_statement.condition);

      if (cond_type != TOKEN_INT_TYPE) {
        printf("Semantic error: if condition must be integer\n");
        exit(1);
      }
      analyze_node(node->body.if_statement.then_branch);
      if (node->body.if_statement.else_branch) {
        analyze_node(node->body.if_statement.else_branch);
      }
      break;
    }
    case NODE_PRINT:
      analyze_expression(node->body.print.print_value);
      break;
    default:
      printf("Semantic error: Unsupported node type in semantic analysis\n");
      exit(1);
  }
}

void semantic_analysis(Node *root) {
    analyze_node(root);
}


/*
  TO DO: 
  Lige så compare den bare: left_type == right_type.

  Vi skal tjekke operator type og så tjekke at det er de rigtige typer for den operator. F.eks.:

  1. Arithmetic (+, -, *, /)
     Kun integers tilladt
     Return type: int

  2. Comparison (<, >, <=, >=)
     Kun integers tilladt
     Return type: int (bruger int, da vi ikke har boolean)

  3. Equality (==, !=)
     Egens typer (int == int, string == string)
     Return type: int

  4. Logical (&&, ||)
     Kun integers tilladt
     Return type: int
*/ 




