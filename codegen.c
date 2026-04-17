

/*

function emit_statement(Node, output) {
    if Node == IfStatement {
        output += "
        if (Node.expression) {
            emit_block(Node.then_block)
        }
        "
        if Node.else_branch is not null {
            ...
        }
    }
}

function emit_program(Node) {
  if Node == ProgramNode {
      output += "
          #include<stdlib.h>

          int main() {
          "

      // parse program
      for each statement in Node {
          output += emit_statement(statement, output)
      }

      output += "
          return 0;
          }
          "
  }
}

function codegen {

  tokens = lexer(input)

  AST/Node = parser(tokens)

  output = ""
  emit_program(AST/Node, output)

  output til .c file
  run "TCC file.c -o main"
  run "./main"

}

*/

#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int threaded_worker_counter = 1;
static void emit_threaded_for_loop_worker(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position);

extern char *thread_functions[100];
extern int thread_function_count;

int is_thread_function(const char *name) {
  for (int i = 0; i < thread_function_count; i++) {
    if (strcmp(thread_functions[i], name) == 0)
      return 1;
  }
  return 0;
}

void emit_statement(Node *node, char **output, int *output_length,
                    int *current_output_position);

void emit_parallel_wrapper_functions(Node *node, char **output, int *output_length,
                                     int *current_output_position);

void emit_parallel_statement(Node *node, char **output, int *output_length,
                             int *current_output_position);

void emit_expression(Node *node, char **output, int *output_length,
                     int *current_output_position);

void emit_function(Node *node, char **output, int *output_length,
                   int *current_output_position);

void emit_binary_operation(Node *node, char **output, int *output_length,
                           int *current_output_position);

void emit_thread(Node *node, char **output, int *output_length,
                 int *current_output_position);

void add_to_output(int *current_output_position, int *output_length,
                   char **output, char *string_to_add) {

  int needed = strlen(string_to_add) + *current_output_position;
  if (needed > *output_length) {
    while (*output_length < needed) {
      *output_length *= 2;
    }
    *output = realloc(*output, (*output_length + 1) * sizeof(char));
  }

  strcpy(*output + *current_output_position, string_to_add);
  *current_output_position += strlen(string_to_add);
}

// Note: Remember to add semicolon at the caller.
void emit_function_call(Node *node, char **output, int *output_length,
                        int *current_output_position) {
  char *function_name = node->body.function_call.name;

  if (is_thread_function(function_name)) {
    // Generate thread creation
    add_to_output(current_output_position, output_length, output,
                  "{pthread_t t;pthread_create(&t,NULL,");
    add_to_output(current_output_position, output_length, output,
                  function_name);
    add_to_output(current_output_position, output_length, output, ",NULL);}");
    return;
  }

  if (node->body.function_call.type == 2) { // process
    char buffer[500];
    snprintf(buffer, sizeof(buffer), "if(fork()==0){");
    add_to_output(current_output_position, output_length, output, buffer);
  }

  // Function name
  add_to_output(current_output_position, output_length, output, function_name);

  // (
  add_to_output(current_output_position, output_length, output, "(");

  // Arguments
  for (int i = 0; i < node->body.function_call.argument_count; i++) {
    emit_expression(node->body.function_call.arguments[i], output,
                    output_length, current_output_position);
    if (i < node->body.function_call.argument_count - 1) {
      add_to_output(current_output_position, output_length, output, ", ");
    }
  }

  // )
  add_to_output(current_output_position, output_length, output, ")");

  if (node->body.function_call.type == 2) { // process
    char buffer[500];
    snprintf(buffer, sizeof(buffer), ";fflush(stdout);_exit(0);}");
    add_to_output(current_output_position, output_length, output, buffer);
  }
}

void emit_expression(Node *node, char **output, int *output_length,
                     int *current_output_position) {

  if (!node) {
    fprintf(stderr, "ERROR: NULL expression\n");
    exit(1);
  }

  switch (node->type) {
  case NODE_BINARY_OPERATION:
    emit_binary_operation(node, output, output_length, current_output_position);
    break;

  case NODE_INT_VALUE: {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", node->body.int_value.value);
    add_to_output(current_output_position, output_length, output, buffer);
    break;
  }

  case NODE_STRING_VALUE: {
    char quoted[256];
    snprintf(quoted, sizeof(quoted), "\"%s\"", node->body.string_value.value);
    add_to_output(current_output_position, output_length, output, quoted);
    break;
  }

  case NODE_IDENTIFIER:
    add_to_output(current_output_position, output_length, output,
                  node->body.identifier.name);
    break;

  case NODE_FUNCTION_CALL:
    emit_function_call(node, output, output_length, current_output_position);
    break;

  default:
    fprintf(stderr, "Unsupported expression type: %d\n", node->type);
    exit(1);
  }
}

void emit_binary_operation(Node *node, char **output, int *output_length,
                           int *current_output_position) {

  if (!node || !node->body.binary_operation.left_operand ||
      !node->body.binary_operation.right_operand) {
    fprintf(stderr, "Invalid binary operation node\n");
    exit(1);
  }

  add_to_output(current_output_position, output_length, output, "(");
  if (node->body.binary_operation.left_operand->type == NODE_BINARY_OPERATION) {
    emit_binary_operation(node->body.binary_operation.left_operand, output,
                          output_length, current_output_position);
  } else if (node->body.binary_operation.left_operand->type == NODE_INT_VALUE) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d",
             node->body.binary_operation.left_operand->body.int_value.value);

    add_to_output(current_output_position, output_length, output, buffer);
  } else if (node->body.binary_operation.left_operand
                 ->type == // Tilføjet denne, da ("Hej" (==)/(!=) "Hej") er
                           // tilladt
             NODE_STRING_VALUE) {
    add_to_output(
        current_output_position, output_length, output,
        node->body.binary_operation.left_operand->body.string_value.value);
  } else if (node->body.binary_operation.left_operand->type ==
             NODE_IDENTIFIER) {
    // The validity of the types was checked in the semantic analysis.
    add_to_output(
        current_output_position, output_length, output,
        node->body.binary_operation.left_operand->body.identifier.name);
  }

  switch (node->body.binary_operation.operator_type) {
  case TOKEN_EQUAL_EQUAL:
    add_to_output(current_output_position, output_length, output, "==");
    break;
  case TOKEN_NOT_EQUAL:
    add_to_output(current_output_position, output_length, output, "!=");
    break;
  case TOKEN_GREATER:
    add_to_output(current_output_position, output_length, output, ">");
    break;
  case TOKEN_LESS:
    add_to_output(current_output_position, output_length, output, "<");
    break;
  case TOKEN_GREATER_EQUAL:
    add_to_output(current_output_position, output_length, output, ">=");
    break;
  case TOKEN_LESS_EQUAL:
    add_to_output(current_output_position, output_length, output, "<=");
    break;
  case TOKEN_AND:
    add_to_output(current_output_position, output_length, output, "&&");
    break;
  case TOKEN_OR:
    add_to_output(current_output_position, output_length, output, "||");
    break;
  case TOKEN_PLUS:
    add_to_output(current_output_position, output_length, output, "+");
    break;
  case TOKEN_MINUS:
    add_to_output(current_output_position, output_length, output, "-");
    break;
  case TOKEN_MULTIPLY:
    add_to_output(current_output_position, output_length, output, "*");
    break;
  case TOKEN_DIVIDE:
    add_to_output(current_output_position, output_length, output, "/");
    break;
  default:
    perror("Erorr occurred");
    break;
  }

  if (node->body.binary_operation.right_operand->type ==
      NODE_BINARY_OPERATION) {
    emit_binary_operation(node->body.binary_operation.right_operand, output,
                          output_length, current_output_position);
  } else if (node->body.binary_operation.right_operand->type ==
             NODE_INT_VALUE) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d",
             node->body.binary_operation.right_operand->body.int_value.value);

    add_to_output(current_output_position, output_length, output, buffer);
  } else if (node->body.binary_operation.right_operand
                 ->type == // Tilføjet denne, da ("Hej" (==)/(!=) "Hej") er
                           // tilladt
             NODE_STRING_VALUE) {
    add_to_output(
        current_output_position, output_length, output,
        node->body.binary_operation.right_operand->body.string_value.value);
  } else if (node->body.binary_operation.right_operand->type ==
             NODE_IDENTIFIER) {
    // The validity of the types was checked in the semantic analysis.
    add_to_output(
        current_output_position, output_length, output,
        node->body.binary_operation.right_operand->body.identifier.name);
  }

  add_to_output(current_output_position, output_length, output, ")");
}

void emit_block(Node *node, char **output, int *output_length,
                int *current_output_position) {
  for (int i = 0; i < node->body.block.statement_count; i++) {
    emit_statement(node->body.block.statements[i], output, output_length,
                   current_output_position);
  }
}

void emit_function(Node *node, char **output, int *output_length,
                   int *current_output_position) {
  if (node->body.function.return_type == TOKEN_INT_TYPE) {
    add_to_output(current_output_position, output_length, output, "int ");
  } else if (node->body.function.return_type == TOKEN_STRING_TYPE) {
    add_to_output(current_output_position, output_length, output, "char* ");
  }

  add_to_output(current_output_position, output_length, output,
                node->body.function.name);
  add_to_output(current_output_position, output_length, output, "(");

  for (int i = 0; i < node->body.function.param_count; i++) {
    if (node->body.function.params[i].type == TOKEN_INT_TYPE) {
      add_to_output(current_output_position, output_length, output, "int ");
    } else if (node->body.function.params[i].type == TOKEN_STRING_TYPE) {
      add_to_output(current_output_position, output_length, output, "char* ");
    }
    add_to_output(current_output_position, output_length, output,
                  node->body.function.params[i].name);
    if (i < node->body.function.param_count - 1) {
      add_to_output(current_output_position, output_length, output, ", ");
    }
  }

  add_to_output(current_output_position, output_length, output, ") {");

  for (int i = 0; i < node->body.function.statement_count; i++) {
    emit_statement(node->body.function.statements[i], output, output_length,
                   current_output_position);
  }

  add_to_output(current_output_position, output_length, output,
                "pthread_mutex_unlock(&global_lock);\n}\n");
}

static void emit_threaded_for_loop_worker(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position) {
  if (!node)
    return;

  if (node->type == NODE_FOR_LOOP) {
    if (node->body.for_loop.type == 1) {
      char condition_first_name[64];
      char condition_second_name[64];
      strcpy(condition_first_name,
             node->body.for_loop.condition->body.binary_operation.left_operand
                 ->body.identifier.name);

      if (node->body.for_loop.condition->body.binary_operation.right_operand
              ->type == NODE_IDENTIFIER) {
        snprintf(condition_second_name, sizeof(condition_second_name), "%s",
                 node->body.for_loop.condition->body.binary_operation
                     .right_operand->body.identifier.name);
      } else if (node->body.for_loop.condition->body.binary_operation
                     .right_operand->type == NODE_INT_VALUE) {
        snprintf(condition_second_name, sizeof(condition_second_name), "%d",
                 node->body.for_loop.condition->body.binary_operation
                     .right_operand->body.int_value.value);
      }
      // jeg ved heller ikke hvad den anden fejl ligesom betyder?
      int start_index = node->body.for_loop.initializer->body.var_declaration
                            .variable_value->body.int_value.value;

      char number_of_threads[6];
      snprintf(number_of_threads, sizeof(number_of_threads), "%d",
               node->body.for_loop.thread_amount);

      char operator_buffer[3];
      switch (
          node->body.for_loop.condition->body.binary_operation.operator_type) {
      case TOKEN_EQUAL_EQUAL:
        strcpy(operator_buffer, "==");
        break;
      case TOKEN_NOT_EQUAL:
        strcpy(operator_buffer, "!=");
        break;
      case TOKEN_GREATER_EQUAL:
        strcpy(operator_buffer, ">=");
        break;
      case TOKEN_GREATER:
        strcpy(operator_buffer, ">");
        break;
      case TOKEN_LESS_EQUAL:
        strcpy(operator_buffer, "<=");
        break;
      case TOKEN_LESS:
        strcpy(operator_buffer, "<");
        break;
      default:
        break;
      }

      char buffer[4096];
      snprintf(
          buffer, sizeof(buffer),
          "void *for_loop_worker_%d(void *arg) { int start_index = *(int "
          "*)arg + %d; for (int %s = start_index; %s %s %s; %s = %s + %s) { ",
          threaded_worker_counter, start_index, condition_first_name,
          condition_first_name, operator_buffer, condition_second_name,
          condition_first_name, condition_first_name, number_of_threads);
      add_to_output(current_output_position, output_length, output, buffer);
      emit_block(node->body.for_loop.body, output, output_length,
                 current_output_position);
      add_to_output(current_output_position, output_length, output,
                    "} return NULL; }"); // hvordan sikre jeg mig at den ikke er
                                         // i main? fejl linje 445-454
      threaded_worker_counter++;
    }

    emit_threaded_for_loop_worker(node->body.for_loop.body, output,
                                  output_length, current_output_position);

    return;
  }

  if (node->type == NODE_BLOCK) {
    for (int i = 0; i < node->body.block.statement_count; i++) {
      emit_threaded_for_loop_worker(node->body.block.statements[i], output,
                                    output_length, current_output_position);
    }
    return;
  }

  if (node->type == NODE_PROGRAM) {
    for (int i = 0; i < node->body.program.statement_count; i++) {
      emit_threaded_for_loop_worker(node->body.program.statements[i], output,
                                    output_length, current_output_position);
    }
    return;
  }

  if (node->type == NODE_FUNCTION) {
    for (int i = 0; i < node->body.function.statement_count; i++) {
      emit_threaded_for_loop_worker(node->body.function.statements[i], output,
                                    output_length, current_output_position);
    }
    return;
  }

  if (node->type == NODE_IF_STATEMENT) {
    emit_threaded_for_loop_worker(node->body.if_statement.then_branch, output,
                                  output_length, current_output_position);

    if (node->body.if_statement.else_branch) {
      emit_threaded_for_loop_worker(node->body.if_statement.else_branch, output,
                                    output_length, current_output_position);
    }
    return;
  }
}

void emit_statement(Node *node, char **output, int *output_length,
                    int *current_output_position) {

  if (node->type == NODE_IF_STATEMENT) {
    add_to_output(current_output_position, output_length, output, "if (");
    emit_expression(node->body.if_statement.condition, output, output_length,
                    current_output_position);
    add_to_output(current_output_position, output_length, output, "){");

    emit_block(node->body.if_statement.then_branch, output, output_length,
               current_output_position);
    add_to_output(current_output_position, output_length, output, "}");
    if (node->body.if_statement.else_branch != NULL) {
      add_to_output(current_output_position, output_length, output,
                    "else {"); // tilføjet denne
      emit_block(node->body.if_statement.else_branch, output, output_length,
                 current_output_position);
      add_to_output(current_output_position, output_length, output, "}");
    }
  } else if (node->type == NODE_PRINT) {
    if (node->body.print.print_value->type == NODE_INT_VALUE) {
      add_to_output(current_output_position, output_length, output,
                    "printf(\"%d\",");
      char buffer[20];
      snprintf(buffer, sizeof(buffer), "%d",
               node->body.print.print_value->body.int_value.value);

      add_to_output(current_output_position, output_length, output, buffer);
      add_to_output(current_output_position, output_length, output, ");");
    }
    if (node->body.print.print_value->type == NODE_STRING_VALUE) {
      add_to_output(current_output_position, output_length, output,
                    "printf(\"%s\",");
      emit_expression(node->body.print.print_value, output, output_length,
                      current_output_position);
      add_to_output(current_output_position, output_length, output, ");");
    } else if (node->body.print.print_value->type == NODE_IDENTIFIER) {
      const char *fmt = (node->body.print.print_value->body.identifier.type ==
                         TOKEN_STRING_TYPE)
                            ? "\"%s\""
                            : "\"%d\"";
      char buffer[100];
      snprintf(buffer, sizeof(buffer), "printf(%s, %s);", fmt,
               node->body.print.print_value->body.identifier.name);
      add_to_output(current_output_position, output_length, output, buffer);
    }
  } else if (node->type == NODE_VAR_DECLARATION) {
    if (node->body.var_declaration.variable_type == TOKEN_INT_TYPE)
      add_to_output(current_output_position, output_length, output, "int ");
    else if (node->body.var_declaration.variable_type == TOKEN_STRING_TYPE)
      add_to_output(current_output_position, output_length, output, "char* ");

    add_to_output(current_output_position, output_length, output,
                  node->body.var_declaration.variable_name);
    add_to_output(current_output_position, output_length, output, "=");
    emit_expression(node->body.var_declaration.variable_value, output,
                    output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ";");
  } else if (node->type == NODE_VAR_UPDATE) {

    // lock the variable if it's shared
    if (node->body.var_update.is_shared) {
      char *variable_name = node->body.var_update.variable_name;
      char buffer[100];
      snprintf(buffer, sizeof(buffer), "pthread_mutex_lock(&lock_%s);\n",
               variable_name);
      add_to_output(current_output_position, output_length, output, buffer);
    }

    add_to_output(current_output_position, output_length, output,
                  node->body.var_update.variable_name);

    switch (node->body.var_update._operator) {
    case TOKEN_EQUAL:
      add_to_output(current_output_position, output_length, output, "=");
      break;
    case TOKEN_PLUS_EQUAL:
      add_to_output(current_output_position, output_length, output, "+=");
      break;
    case TOKEN_MINUS_EQUAL:
      add_to_output(current_output_position, output_length, output, "-=");
      break;
    case TOKEN_PLUS_PLUS:
      add_to_output(current_output_position, output_length, output, "++");
      break;
    default:
      perror("Unsupported operator type for variable updating.");
    }

    emit_expression(node->body.var_update.value, output, output_length,
                    current_output_position);

    add_to_output(current_output_position, output_length, output, ";");

    // unclock the variable if it's shared after usage
    if (node->body.var_update.is_shared) {
      char *variable_name = node->body.var_update.variable_name;
      char buffer[100];
      snprintf(buffer, sizeof(buffer), "pthread_mutex_unlock(&lock_%s);\n",
               variable_name);
      add_to_output(current_output_position, output_length, output, buffer);
    }
  } else if (node->type == NODE_FOR_LOOP) {
    if (node->body.for_loop.type == 1) {
      char number_of_threads[6];
      snprintf(number_of_threads, sizeof(number_of_threads), "%d",
               node->body.for_loop.thread_amount);

      char array_of_threads_buffer[48];
      snprintf(array_of_threads_buffer, sizeof(array_of_threads_buffer),
               "pthread_t threads[%d]; int starts[%d]; ",
               node->body.for_loop.thread_amount,
               node->body.for_loop.thread_amount);
      add_to_output(current_output_position, output_length, output,
                    array_of_threads_buffer);

      add_to_output(current_output_position, output_length, output,
                    "for (int i = 0; i < ");
      add_to_output(current_output_position, output_length, output,
                    number_of_threads);

      add_to_output(
          current_output_position, output_length, output,
          "; i++) { starts[i] = i; "); // hvordan fikser jeg identation
                                       // for at undgå fejlen på linje
                                       // 469-476... stadig ikke sikker.

      char thread_creator[128];
      snprintf(thread_creator, sizeof(thread_creator),
               "pthread_create(&threads[i], NULL, for_loop_worker_%d, "
               "&starts[i]); }",
               threaded_worker_counter);
      add_to_output(current_output_position, output_length, output,
                    thread_creator);

      add_to_output(current_output_position, output_length, output,
                    "for (int i = 0; i <");
      add_to_output(current_output_position, output_length, output,
                    number_of_threads);
      add_to_output(current_output_position, output_length, output,
                    "; i++) { pthread_join(threads[i], NULL); }");

      threaded_worker_counter++;
      return; // måske jeg bare skal tilføje et return?? fejl linje 478
    }
    add_to_output(current_output_position, output_length, output, "for (");

    emit_statement(node->body.for_loop.initializer, output, output_length,
                   current_output_position);
    add_to_output(current_output_position, output_length, output, " ");
    emit_expression(node->body.for_loop.condition, output, output_length,
                    current_output_position);
    add_to_output(current_output_position, output_length, output, "; ");

    Node *updater = node->body.for_loop.updater;
    add_to_output(current_output_position, output_length, output,
                  updater->body.var_update.variable_name);
    switch (updater->body.var_update._operator) {
    case TOKEN_EQUAL:
      add_to_output(current_output_position, output_length, output, "=");
      break;
    case TOKEN_PLUS_EQUAL:
      add_to_output(current_output_position, output_length, output, "+=");
      break;
    case TOKEN_MINUS_EQUAL:
      add_to_output(current_output_position, output_length, output, "-=");
      break;
    case TOKEN_PLUS_PLUS:
      add_to_output(current_output_position, output_length, output, "++");
      break;
    default:
      break;
    }
    emit_expression(updater->body.var_update.value, output, output_length,
                    current_output_position);

    add_to_output(current_output_position, output_length, output, ") {");

    emit_block(node->body.for_loop.body, output, output_length,
               current_output_position);

    add_to_output(current_output_position, output_length, output, "}");
  } else if (node->type == NODE_RETURN_STATEMENT) {
    add_to_output(current_output_position, output_length, output, "return ");
    emit_expression(node->body.return_statement.expression, output,
                    output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ";");
  } else if (node->type == NODE_PARALLEL) {
    emit_parallel_statement(node, output, output_length, current_output_position);
  } else if (node->type == NODE_FUNCTION_CALL) {
    emit_function_call(node, output, output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ";");
    // MADS B
  } else if (node->type == NODE_AWAIT) {
    add_to_output(current_output_position, output_length, output, "result = ");
    add_to_output(current_output_position, output_length, output,
                  node->body.thread.name);
    add_to_output(current_output_position, output_length, output, "();\n");
    add_to_output(current_output_position, output_length, output,
                  "pthread_join(");
    add_to_output(current_output_position, output_length, output,
                  node->body.thread.name);
    add_to_output(current_output_position, output_length, output, ", NULL);\n");
  }
  // MADS E
}

void emit_program(Node *node, char **output, int *output_length,
                  int *current_output_position) {

  /* type: NODE_PROGRAM
     body: struct {
       struct Node **statements;
       int statement_count;
     } program;
  */
  if (node->type == NODE_PROGRAM) {
    add_to_output(current_output_position, output_length, output,
                  "#include <stdlib.h> \n\
                   #include <stdio.h> \n\
                   #include <pthread.h> \n\
                   #include <unistd.h>\n");

    threaded_worker_counter = 1;
    emit_threaded_for_loop_worker(node, output, output_length,
                                  current_output_position);

    // Emit mutex declarations + init for shared variables
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.is_shared) {
        char buffer[150];
        snprintf(buffer, sizeof(buffer), "pthread_mutex_t lock_%s;\n",
                 stmt->body.var_declaration.variable_name);
        add_to_output(current_output_position, output_length, output, buffer);

        snprintf(buffer, sizeof(buffer),
                 "pthread_mutex_init(&lock_%s, NULL);\n",
                 stmt->body.var_declaration.variable_name);
        add_to_output(current_output_position, output_length, output, buffer);
      }
    }

    // Emit thread functions
    for (int i = 0; i < node->body.program.statement_count; i++) {
      if (node->body.program.statements[i]->type == NODE_THREAD) {
        emit_thread(node->body.program.statements[i], output, output_length,
                    current_output_position);
        add_to_output(current_output_position, output_length, output, "\n");
      }
    }

    // Emit normal functions
    for (int i = 0; i < node->body.program.statement_count; i++) {
      if (node->body.program.statements[i]->type == NODE_FUNCTION) {
        emit_function(node->body.program.statements[i], output, output_length,
                      current_output_position);
        add_to_output(current_output_position, output_length, output, "\n");
      }
    }

    for (int i = 0; i < node->body.program.statement_count; i++) {
      if (node->body.program.statements[i]->type == NODE_PARALLEL) {
        emit_parallel_wrapper_functions(node->body.program.statements[i], output,
                                        output_length, current_output_position);
      }
    }

    add_to_output(current_output_position, output_length, output,
                  "int main() {\n");
    
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.is_shared) {

        char buffer[100];
        snprintf(buffer, sizeof(buffer),
                 "pthread_mutex_init(&lock_%s, NULL);",
                 stmt->body.var_declaration.variable_name);
        add_to_output(current_output_position, output_length, output, buffer);
      }
    }

    threaded_worker_counter = 1;

    // Emit all non-function, non-thread statements
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type != NODE_FUNCTION && stmt->type != NODE_THREAD) {
        emit_statement(stmt, output, output_length, current_output_position);
      }
    }

    add_to_output(current_output_position, output_length, output,
                  "\n  return 0;\n}\n");
  }
}


void emit_parallel_wrapper_functions(Node *node, char **output, int *output_length,
                                     int *current_output_position) {
  for (int i = 0; i < node->body.parallel.section_count; i++) {
    char buffer[128];

    snprintf(buffer, sizeof(buffer),
             "void* parallel_%d_section_%d(void* arg) {",
             node->body.parallel.parallel_id, i);
    add_to_output(current_output_position, output_length, output, buffer);

    emit_statement(node->body.parallel.sections[i], output, output_length,
                   current_output_position);

    add_to_output(current_output_position, output_length, output, "return NULL;}");
  }
}


void emit_parallel_statement(Node *node, char **output, int *output_length,
                             int *current_output_position) {
  int count = node->body.parallel.section_count;

  for (int i = 0; i < count; i++) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "pthread_t parallel_%d_thread_%d;",
             node->body.parallel.parallel_id, i);
    add_to_output(current_output_position, output_length, output, buffer);
  }

  for (int i = 0; i < count; i++) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "pthread_create(&parallel_%d_thread_%d, NULL, parallel_%d_section_%d, NULL);",
             node->body.parallel.parallel_id, i,
             node->body.parallel.parallel_id, i);
    add_to_output(current_output_position, output_length, output, buffer);
  }

  for (int i = 0; i < count; i++) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "pthread_join(parallel_%d_thread_%d, NULL);",
             node->body.parallel.parallel_id, i);
    add_to_output(current_output_position, output_length, output, buffer);
  }
}


void emit_thread(Node *node, char **output, int *output_length,
                 int *current_output_position) {
  add_to_output(current_output_position, output_length, output, "void* ");
  add_to_output(current_output_position, output_length, output,
                node->body.thread.name);
  add_to_output(current_output_position, output_length, output,
                "(void* arg) {\n");

  for (int i = 0; i < node->body.thread.statement_count; i++) {
    emit_statement(node->body.thread.statements[i], output, output_length,
                   current_output_position);
  }

  add_to_output(current_output_position, output_length, output,
                "  return NULL;\n}\n");
}

// int main(int argc, char *argv[]) {
//   if (argc < 2) {
//     fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
//     return 1;
//   }

//   FILE *input = fopen(argv[1], "r");
//   if (!input) {
//     perror("fopen input file");
//     return 1;
//   }

//   fseek(input, 0, SEEK_END);
//   long file_size = ftell(input);
//   rewind(input);

//   char *str = malloc(file_size + 1);
//   if (!str) {
//     perror("malloc");
//     fclose(input);
//     return 1;
//   }

//   size_t bytes_read = fread(str, 1, file_size, input);
//   str[bytes_read] = '\0';
//   fclose(input);

//   int output_length = 1024;
//   int current_output_position = 0;
//   char *output = (char *)malloc((output_length + 1) * sizeof(char));
//   if (!output) {
//     perror("malloc");
//     free(str);
//     return 1;
//   }
//   output[0] = '\0';

//   Lexer lexer = new_lexer(str);

//   Node *root = parse(&lexer);

//   semantic_analyze(root);

//   emit_program(root, &output, &output_length, &current_output_position);

//   printf("\nResult:\n%s\n", output);

//   FILE *f = fopen("temp.c", "w");
//   if (!f) {
//     perror("fopen temp.c");
//     free(str);
//     free(output);
//     return 1;
//   }

//   fputs(output, f);
//   fclose(f);

int main() {
  char *str =
      "thread=2 for (int i = 0; i < 100; i = i+1) {print(i); print(\" - \");}";

  int output_length = 30;
  int current_output_position = 0;
  char *output = (char *)malloc((output_length + 1) * sizeof(char));

  Lexer lexer = new_lexer(str);

  Node *root = parse(&lexer);

  semantic_analyze(root);

  emit_program(root, &output, &output_length, &current_output_position);

  printf("\nResult:\n%s\n", output);

  FILE *f = fopen("temp.c", "w");
  if (!f) {
    perror("fopen");
    return 1;
  }

  fputs(output, f);
  fclose(f);

  free(output);

#if defined(_WIN32)
  system("gcc temp.c -o temp.exe");
  system("temp.exe");
#elif defined(__linux__) || defined(__APPLE__)
  system("gcc temp.c -o temp");
  system("./temp");
#else
  printf("Unsupported OS\n");
#endif

  return 0;
}

/* Example Paser:
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

/*

## Frontend:

Input:
if a < b
  print(x)

Lexer output:
'if', 'a', '<', 'b'

Parser output (AST):
IfStatement
  condition: a < b
  then_block:
      PrintStatement
          Expression
            value: x

Semantic analyzer:
Example:
int x = 10;
x = "abc"; // Error;


## Backend

CodeGen:

To C:
if a < b ==> if (a < b) ==> run c compiler ==> LLVM (IR) ==> x86 ASM ==>
010101011101001

print(x) ==> printf("%s", x) ==> run c compiler ==> 01010010010

To x86:
if a < b ==> [a], rax; cmp rax, [b]; jl; ==> run assembler ==> 01010010101

*/
