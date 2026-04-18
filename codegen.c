#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int threaded_worker_counter = 1;

/* Forward declarations */
static void emit_statement(Node *node, char **output, int *output_length,
                           int *current_output_position, Node *program_node);
static void emit_expression(Node *node, char **output, int *output_length,
                            int *current_output_position);
static void emit_function(Node *node, char **output, int *output_length,
                          int *current_output_position);
static void emit_block(Node *node, char **output, int *output_length,
                       int *current_output_position, Node *program_node);
static void emit_operand(Node *node, char **output, int *output_length,
                         int *current_output_position);
static void emit_thread_call_wrapper(Node *node, const char *result_var,
                                     int wrapper_id, char **output,
                                     int *output_length,
                                     int *current_output_position,
                                     Node *program_node);
static void emit_thread_call_inline(Node *node, const char *result_var,
                                    int wrapper_id, char **output,
                                    int *output_length,
                                    int *current_output_position);
static void emit_all_thread_call_wrappers(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position);
static void emit_threaded_for_loop_worker(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position);

static void add_to_output(int *pos, int *len, char **out, const char *str) {
  int needed = (int)strlen(str) + *pos;
  while (needed > *len) {
    *len *= 2;
  }
  *out = realloc(*out, (size_t)(*len + 1));
  strcpy(*out + *pos, str);
  *pos += (int)strlen(str);
}

static const char *escape_string(const char *str, char *buf, int buf_size) {
  int j = 0;
  for (int i = 0; str[i] && j < buf_size - 2; i++) {
    if (str[i] == '\\' && str[i + 1]) {
      /* Pass through known escape sequences the lexer already captured */
      buf[j++] = str[i++];
      buf[j++] = str[i];
    } else if (str[i] == '"') {
      buf[j++] = '\\';
      buf[j++] = '"';
    } else if (str[i] == '\n') {
      buf[j++] = '\\';
      buf[j++] = 'n';
    } else if (str[i] == '\t') {
      buf[j++] = '\\';
      buf[j++] = 't';
    } else if (str[i] == '\r') {
      buf[j++] = '\\';
      buf[j++] = 'r';
    } else {
      buf[j++] = str[i];
    }
  }
  buf[j] = '\0';
  return buf;
}

static const char *operator_to_string(TokenType type) {
  switch (type) {
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_MULTIPLY:
    return "*";
  case TOKEN_DIVIDE:
    return "/";
  case TOKEN_EQUAL_EQUAL:
    return "==";
  case TOKEN_NOT_EQUAL:
    return "!=";
  case TOKEN_GREATER:
    return ">";
  case TOKEN_LESS:
    return "<";
  case TOKEN_GREATER_EQUAL:
    return ">=";
  case TOKEN_LESS_EQUAL:
    return "<=";
  case TOKEN_AND:
    return "&&";
  case TOKEN_OR:
    return "||";
  default:
    return "?";
  }
}

static void emit_operand(Node *node, char **output, int *output_length,
                         int *current_output_position) {
  switch (node->type) {
  case NODE_BINARY_OPERATION:
    add_to_output(current_output_position, output_length, output, "(");
    emit_operand(node->body.binary_operation.left_operand, output,
                 output_length, current_output_position);
    add_to_output(
        current_output_position, output_length, output,
        operator_to_string(node->body.binary_operation.operator_type));
    emit_operand(node->body.binary_operation.right_operand, output,
                 output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ")");
    break;
  case NODE_INT_VALUE: {
    char buf[20];
    snprintf(buf, sizeof(buf), "%d", node->body.int_value.value);
    add_to_output(current_output_position, output_length, output, buf);
    break;
  }
  case NODE_STRING_VALUE:
    add_to_output(current_output_position, output_length, output,
                  node->body.string_value.value);
    break;
  case NODE_IDENTIFIER:
    add_to_output(current_output_position, output_length, output,
                  node->body.identifier.name);
    break;
  default:
    fprintf(stderr, "Unsupported operand type: %d\n", node->type);
    exit(1);
  }
}

static int is_process_variable(Node *program_node, const char *name) {
  if (program_node->type != NODE_PROGRAM)
    return 0;
  for (int i = 0; i < program_node->body.program.statement_count; i++) {
    Node *stmt = program_node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        strcmp(stmt->body.var_declaration.variable_name, name) == 0 &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_PROCESS) {
      return 1;
    }
  }
  return 0;
}

static int find_function_return_type(Node *program_node, const char *fn_name) {
  if (program_node->type != NODE_PROGRAM)
    return TOKEN_VOID;
  for (int i = 0; i < program_node->body.program.statement_count; i++) {
    Node *stmt = program_node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION &&
        strcmp(stmt->body.function.name, fn_name) == 0) {
      return stmt->body.function.return_type;
    }
  }
  return TOKEN_VOID;
}

static void emit_thread_call_wrapper(Node *node, const char *result_var,
                                     int wrapper_id, char **output,
                                     int *output_length,
                                     int *current_output_position,
                                     Node *program_node) {
  const char *fn = node->body.function_call.name;
  int argc = node->body.function_call.argument_count;

  char buf[256];
  snprintf(buf, sizeof(buf), "void* thread_call_%d(void* arg) {", wrapper_id);
  add_to_output(current_output_position, output_length, output, buf);

  if (argc > 0) {
    add_to_output(current_output_position, output_length, output,
                  "intptr_t* args=(intptr_t*)arg;");
  }

  if (result_var) {
    int ret_type = find_function_return_type(program_node, fn);
    if (ret_type != TOKEN_VOID) {
      add_to_output(current_output_position, output_length, output, result_var);
      add_to_output(current_output_position, output_length, output, "=");
    }
  }

  add_to_output(current_output_position, output_length, output, fn);
  add_to_output(current_output_position, output_length, output, "(");
  for (int i = 0; i < argc; i++) {
    char ab[64];
    snprintf(ab, sizeof(ab), "(int)args[%d]", i);
    add_to_output(current_output_position, output_length, output, ab);
    if (i < argc - 1)
      add_to_output(current_output_position, output_length, output, ", ");
  }
  add_to_output(current_output_position, output_length, output, ");");
  add_to_output(current_output_position, output_length, output,
                "return NULL;}\n");
}

static void emit_thread_call_inline(Node *node, const char *result_var,
                                    int wrapper_id, char **output,
                                    int *output_length,
                                    int *current_output_position) {
  int argc = node->body.function_call.argument_count;

  char buf[128];
  snprintf(buf, sizeof(buf),
           "pthread_t _thread_%s; pid_t _process_%s = -1;", result_var,
           result_var);
  add_to_output(current_output_position, output_length, output, buf);

  if (argc > 0) {
    char arr_name[64];
    snprintf(arr_name, sizeof(arr_name), "_args_%s", result_var);
    snprintf(buf, sizeof(buf), "intptr_t %s[%d]={", arr_name, argc);
    add_to_output(current_output_position, output_length, output, buf);
    for (int i = 0; i < argc; i++) {
      emit_expression(node->body.function_call.arguments[i], output,
                      output_length, current_output_position);
      if (i < argc - 1)
        add_to_output(current_output_position, output_length, output, ", ");
    }
    add_to_output(current_output_position, output_length, output, "};");
  }

  snprintf(buf, sizeof(buf), "pthread_create(&_thread_%s,NULL,thread_call_%d,",
           result_var, wrapper_id);
  add_to_output(current_output_position, output_length, output, buf);

  if (argc > 0) {
    char arr_name[64];
    snprintf(arr_name, sizeof(arr_name), "_args_%s", result_var);
    snprintf(buf, sizeof(buf), "%s);", arr_name);
    add_to_output(current_output_position, output_length, output, buf);
  } else {
    add_to_output(current_output_position, output_length, output, "NULL);");
  }
}

static void emit_all_thread_call_wrappers(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position) {
  if (node->type != NODE_PROGRAM)
    return;
  int wrapper_id = 1;
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION_CALL &&
        stmt->body.function_call.type == PARALLEL_TYPE_THREAD) {
      emit_thread_call_wrapper(stmt, NULL, wrapper_id, output, output_length,
                               current_output_position, node);
      wrapper_id++;
    }
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD) {
      const char *result_var = stmt->body.var_declaration.variable_name;
      emit_thread_call_wrapper(stmt->body.var_declaration.variable_value,
                               result_var, wrapper_id, output, output_length,
                               current_output_position, node);
      wrapper_id++;
    }
    if (stmt->type == NODE_THREAD) {
      char buf[256];
      snprintf(buf, sizeof(buf), "void* thread_call_%d(void* arg) {",
               wrapper_id);
      add_to_output(current_output_position, output_length, output, buf);
      for (int j = 0; j < stmt->body.thread.statement_count; j++) {
        emit_statement(stmt->body.thread.statements[j], output, output_length,
                       current_output_position, NULL);
      }
      add_to_output(current_output_position, output_length, output,
                    "return NULL;}\n");
      wrapper_id++;
    }
  }
}

static void emit_all_thread_call_inlines(Node *node, char **output,
                                         int *output_length,
                                         int *current_output_position) {
  if (node->type != NODE_PROGRAM)
    return;
  int wrapper_id = 1;
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION_CALL &&
        stmt->body.function_call.type == PARALLEL_TYPE_THREAD) {
      char handle[32];
      snprintf(handle, sizeof(handle), "_t%d", wrapper_id);
      emit_thread_call_inline(stmt, handle, wrapper_id, output, output_length,
                              current_output_position);
      wrapper_id++;
    }
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD) {
      const char *result_var = stmt->body.var_declaration.variable_name;
      emit_thread_call_inline(stmt->body.var_declaration.variable_value,
                              result_var, wrapper_id, output, output_length,
                              current_output_position);
      wrapper_id++;
    }
    if (stmt->type == NODE_THREAD) {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "pthread_t _thread__t%d;pthread_create(&_thread__t%d,NULL,"
               "thread_call_%d,NULL);",
               wrapper_id, wrapper_id, wrapper_id);
      add_to_output(current_output_position, output_length, output, buf);
      wrapper_id++;
    }
  }
}

static void emit_function_call(Node *node, char **output, int *output_length,
                               int *current_output_position) {
  const char *fn = node->body.function_call.name;

  if (node->body.function_call.type == PARALLEL_TYPE_THREAD) {
    /* Standalone thread call (no result var) - handled by emit_program */
    return;
  }

  if (node->body.function_call.type == PARALLEL_TYPE_PROCESS) {
    /* Standalone process call (no result var) - handled by emit_program */
    return;
  }

  add_to_output(current_output_position, output_length, output, fn);
  add_to_output(current_output_position, output_length, output, "(");

  for (int i = 0; i < node->body.function_call.argument_count; i++) {
    emit_expression(node->body.function_call.arguments[i], output,
                    output_length, current_output_position);
    if (i < node->body.function_call.argument_count - 1)
      add_to_output(current_output_position, output_length, output, ", ");
  }

  add_to_output(current_output_position, output_length, output, ")");
}

static void emit_expression(Node *node, char **output, int *output_length,
                            int *current_output_position) {
  if (!node) {
    fprintf(stderr, "ERROR: NULL expression\n");
    exit(1);
  }

  switch (node->type) {
  case NODE_BINARY_OPERATION:
    emit_operand(node, output, output_length, current_output_position);
    break;
  case NODE_INT_VALUE: {
    char buf[20];
    snprintf(buf, sizeof(buf), "%d", node->body.int_value.value);
    add_to_output(current_output_position, output_length, output, buf);
    break;
  }
  case NODE_STRING_VALUE: {
    char escaped[512];
    escape_string(node->body.string_value.value, escaped, sizeof(escaped));
    char buf[540];
    snprintf(buf, sizeof(buf), "\"%s\"", escaped);
    add_to_output(current_output_position, output_length, output, buf);
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

static void emit_block(Node *node, char **output, int *output_length,
                       int *current_output_position, Node *program_node) {
  for (int i = 0; i < node->body.block.statement_count; i++) {
    emit_statement(node->body.block.statements[i], output, output_length,
                   current_output_position, program_node);
  }
}

static const char *c_type(TokenType t) {
  switch (t) {
  case TOKEN_INT_TYPE:
    return "int";
  case TOKEN_STRING_TYPE:
    return "char*";
  case TOKEN_VOID:
    return "void";
  default:
    return "void";
  }
}

static void emit_function(Node *node, char **output, int *output_length,
                          int *current_output_position) {
  add_to_output(current_output_position, output_length, output,
                c_type(node->body.function.return_type));
  add_to_output(current_output_position, output_length, output, " ");
  add_to_output(current_output_position, output_length, output,
                node->body.function.name);
  add_to_output(current_output_position, output_length, output, "(");

  for (int i = 0; i < node->body.function.param_count; i++) {
    add_to_output(current_output_position, output_length, output,
                  c_type(node->body.function.params[i].type));
    add_to_output(current_output_position, output_length, output, " ");
    add_to_output(current_output_position, output_length, output,
                  node->body.function.params[i].name);
    if (i < node->body.function.param_count - 1)
      add_to_output(current_output_position, output_length, output, ", ");
  }

  add_to_output(current_output_position, output_length, output, ") {");

  for (int i = 0; i < node->body.function.statement_count; i++) {
    emit_statement(node->body.function.statements[i], output, output_length,
                   current_output_position, NULL);
  }

  add_to_output(current_output_position, output_length, output, "}\n");
}

static void emit_threaded_for_loop_worker(Node *node, char **output,
                                          int *output_length,
                                          int *current_output_position) {
  if (!node)
    return;

  if (node->type == NODE_FOR_LOOP && node->body.for_loop.type == 1) {
    Node *cond = node->body.for_loop.condition;
    const char *var =
        cond->body.binary_operation.left_operand->body.identifier.name;

    char rhs[64];
    Node *rhs_node = cond->body.binary_operation.right_operand;
    if (rhs_node->type == NODE_IDENTIFIER) {
      snprintf(rhs, sizeof(rhs), "%s", rhs_node->body.identifier.name);
    } else {
      snprintf(rhs, sizeof(rhs), "%d", rhs_node->body.int_value.value);
    }

    int start = node->body.for_loop.initializer->body.var_declaration
                    .variable_value->body.int_value.value;
    int nthreads = node->body.for_loop.thread_amount;

    char buf[4096];
    snprintf(buf, sizeof(buf),
             "void *for_loop_worker_%d(void *arg) { "
             "int start_index = *(int *)arg + %d; "
             "for (int %s = start_index; %s %s %s; %s = %s + %d) { ",
             threaded_worker_counter, start, var, var,
             operator_to_string(cond->body.binary_operation.operator_type), rhs,
             var, var, nthreads);
    add_to_output(current_output_position, output_length, output, buf);
    emit_block(node->body.for_loop.body, output, output_length,
               current_output_position, NULL);
    add_to_output(current_output_position, output_length, output,
                  "} return NULL; }");
    threaded_worker_counter++;
  }

  /* Recurse into children to find nested threaded for-loops */
  switch (node->type) {
  case NODE_FOR_LOOP:
    emit_threaded_for_loop_worker(node->body.for_loop.body, output,
                                  output_length, current_output_position);
    break;
  case NODE_BLOCK:
    for (int i = 0; i < node->body.block.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.block.statements[i], output,
                                    output_length, current_output_position);
    break;
  case NODE_PROGRAM:
    for (int i = 0; i < node->body.program.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.program.statements[i], output,
                                    output_length, current_output_position);
    break;
  case NODE_FUNCTION:
    for (int i = 0; i < node->body.function.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.function.statements[i], output,
                                    output_length, current_output_position);
    break;
  case NODE_IF_STATEMENT:
    emit_threaded_for_loop_worker(node->body.if_statement.then_branch, output,
                                  output_length, current_output_position);
    if (node->body.if_statement.else_branch)
      emit_threaded_for_loop_worker(node->body.if_statement.else_branch, output,
                                    output_length, current_output_position);
    break;
  default:
    break;
  }
}

static void emit_statement(Node *node, char **output, int *output_length,
                           int *current_output_position,
                           Node *program_node) {
  switch (node->type) {
  case NODE_IF_STATEMENT: {
    add_to_output(current_output_position, output_length, output, "if (");
    emit_expression(node->body.if_statement.condition, output, output_length,
                    current_output_position);
    add_to_output(current_output_position, output_length, output, "){");
    emit_block(node->body.if_statement.then_branch, output, output_length,
               current_output_position, program_node);
    add_to_output(current_output_position, output_length, output, "}");
    if (node->body.if_statement.else_branch) {
      add_to_output(current_output_position, output_length, output, "else {");
      emit_block(node->body.if_statement.else_branch, output, output_length,
                 current_output_position, program_node);
      add_to_output(current_output_position, output_length, output, "}");
    }
    break;
  }

  case NODE_PRINT: {
    Node *val = node->body.print.print_value;
    if (val->type == NODE_INT_VALUE) {
      char buf[32];
      snprintf(buf, sizeof(buf), "printf(\"%%d\",%d);",
               val->body.int_value.value);
      add_to_output(current_output_position, output_length, output, buf);
    } else if (val->type == NODE_STRING_VALUE) {
      char escaped[512];
      escape_string(val->body.string_value.value, escaped, sizeof(escaped));
      char buf[540];
      snprintf(buf, sizeof(buf), "printf(\"%%s\",\"%s\");", escaped);
      add_to_output(current_output_position, output_length, output, buf);
    } else if (val->type == NODE_IDENTIFIER) {
      const char *fmt = (val->body.identifier.type == TOKEN_STRING_TYPE)
                            ? "\"%s\""
                            : "\"%d\"";
      char buf[100];
      snprintf(buf, sizeof(buf), "printf(%s,%s);", fmt,
               val->body.identifier.name);
      add_to_output(current_output_position, output_length, output, buf);
    }
    break;
  }

  case NODE_VAR_DECLARATION: {
    const char *var_name = node->body.var_declaration.variable_name;

    if (node->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD ||
        node->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_PROCESS) {
      /* Thread call with result — handled by emit_all_thread_call_inlines */
    } else {
      add_to_output(current_output_position, output_length, output,
                    c_type(node->body.var_declaration.variable_type));
      add_to_output(current_output_position, output_length, output, " ");
      add_to_output(current_output_position, output_length, output, var_name);
      add_to_output(current_output_position, output_length, output, "=");
      emit_expression(node->body.var_declaration.variable_value, output,
                      output_length, current_output_position);
      add_to_output(current_output_position, output_length, output, ";");
    }
    break;
  }

  case NODE_VAR_UPDATE: {
    if (node->body.var_update.is_shared) {
      char buf[100];
      snprintf(buf, sizeof(buf), "pthread_mutex_lock(&lock_%s);",
               node->body.var_update.variable_name);
      add_to_output(current_output_position, output_length, output, buf);
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
      break;
    }

    if (node->body.var_update.value) {
      emit_expression(node->body.var_update.value, output, output_length,
                      current_output_position);
    }
    add_to_output(current_output_position, output_length, output, ";");

    if (node->body.var_update.is_shared) {
      char buf[100];
      snprintf(buf, sizeof(buf), "pthread_mutex_unlock(&lock_%s);",
               node->body.var_update.variable_name);
      add_to_output(current_output_position, output_length, output, buf);
    }
    break;
  }

  case NODE_FOR_LOOP: {
    if (node->body.for_loop.type == PARALLEL_TYPE_THREAD) {
      char buf[512];
      int n = node->body.for_loop.thread_amount;
      snprintf(
          buf, sizeof(buf),
          "pthread_t threads[%d]; int starts[%d]; "
          "for (int i = 0; i < %d; i++) { starts[i] = i; "
          "pthread_create(&threads[i], NULL, for_loop_worker_%d, "
          "&starts[i]); } "
          "for (int i = 0; i < %d; i++) { pthread_join(threads[i], NULL); }",
          n, n, n, threaded_worker_counter, n);
      add_to_output(current_output_position, output_length, output, buf);
      threaded_worker_counter++;
      return;
    }

    add_to_output(current_output_position, output_length, output, "for (");
    emit_statement(node->body.for_loop.initializer, output, output_length,
                   current_output_position, program_node);
    add_to_output(current_output_position, output_length, output, " ");
    emit_expression(node->body.for_loop.condition, output, output_length,
                    current_output_position);
    add_to_output(current_output_position, output_length, output, "; ");

    Node *up = node->body.for_loop.updater;
    add_to_output(current_output_position, output_length, output,
                  up->body.var_update.variable_name);
    switch (up->body.var_update._operator) {
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
    if (up->body.var_update.value) {
      emit_expression(up->body.var_update.value, output, output_length,
                      current_output_position);
    }
    add_to_output(current_output_position, output_length, output, ") {");
    emit_block(node->body.for_loop.body, output, output_length,
               current_output_position, program_node);
    add_to_output(current_output_position, output_length, output, "}");
    break;
  }

  case NODE_RETURN_STATEMENT: {
    add_to_output(current_output_position, output_length, output, "return ");
    emit_expression(node->body.return_statement.expression, output,
                    output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ";");
    break;
  }

  case NODE_FUNCTION_CALL: {
    emit_function_call(node, output, output_length, current_output_position);
    add_to_output(current_output_position, output_length, output, ";");
    break;
  }

  case NODE_AWAIT: {
    for (int i = 0; i < node->body.thread.statement_count; i++) {
      Node *id_node = (Node *)node->body.thread.statements[i];
      const char *name = id_node->body.identifier.name;
      char buf[256];
      if (is_process_variable(program_node, name)) {
        snprintf(buf, sizeof(buf),
                 "waitpid(_process_%s, NULL, 0); %s = *%s_ptr;",
                 name, name, name);
      } else {
        snprintf(buf, sizeof(buf), "pthread_join(_thread_%s, NULL);", name);
      }
      add_to_output(current_output_position, output_length, output, buf);
    }
    break;
  }

  default:
    break;
  }
}

static void emit_program(Node *node, char **output, int *output_length,
                         int *current_output_position) {
  if (node->type != NODE_PROGRAM)
    return;

  add_to_output(current_output_position, output_length, output,
                "#include <stdlib.h>\n"
                "#include <stdio.h>\n"
                "#include <stdint.h>\n"
                "#include <pthread.h>\n"
                "#include <unistd.h>\n"
                "#include <sys/wait.h>\n"
                "#include <sys/mman.h>\n");

  /* Emit global result variables for thread/process calls with return values */
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        (stmt->body.var_declaration.variable_parallel_type ==
             PARALLEL_TYPE_THREAD ||
         stmt->body.var_declaration.variable_parallel_type ==
             PARALLEL_TYPE_PROCESS)) {
      const char *name = stmt->body.var_declaration.variable_name;
      char buf[64];
      snprintf(buf, sizeof(buf), "%s %s = 0;\n",
               c_type(stmt->body.var_declaration.variable_type), name);
      add_to_output(current_output_position, output_length, output, buf);
      if (stmt->body.var_declaration.variable_parallel_type ==
          PARALLEL_TYPE_PROCESS) {
        snprintf(buf, sizeof(buf), "int* %s_ptr;\n", name);
        add_to_output(current_output_position, output_length, output, buf);
      }
    }
  }

  /* Emit top-level variable declarations globally so they're accessible
     from threaded worker functions */
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_REGULAR) {
      char buf[64];
      snprintf(buf, sizeof(buf),
               "%s %s = ", c_type(stmt->body.var_declaration.variable_type),
               stmt->body.var_declaration.variable_name);
      add_to_output(current_output_position, output_length, output, buf);
      emit_expression(stmt->body.var_declaration.variable_value, output,
                      output_length, current_output_position);
      add_to_output(current_output_position, output_length, output, ";\n");
    }
  }

  /* Emit mutex declarations for shared variables before workers */
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.is_shared) {
      char buf[150];
      snprintf(buf, sizeof(buf), "pthread_mutex_t lock_%s;\n",
               stmt->body.var_declaration.variable_name);
      add_to_output(current_output_position, output_length, output, buf);
    }
  }

  threaded_worker_counter = 1;
  emit_threaded_for_loop_worker(node, output, output_length,
                                current_output_position);

  /* Emit function definitions */
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION) {
      emit_function(stmt, output, output_length, current_output_position);
      add_to_output(current_output_position, output_length, output, "\n");
    }
  }

  emit_all_thread_call_wrappers(node, output, output_length,
                                current_output_position);

  /* Emit process wrapper functions */
  {
    int process_id = 1;
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.variable_parallel_type ==
              PARALLEL_TYPE_PROCESS) {
        Node *call = stmt->body.var_declaration.variable_value;
        const char *fn = call->body.function_call.name;
        int argc = call->body.function_call.argument_count;
        const char *result_var = stmt->body.var_declaration.variable_name;

        char buf[256];
        snprintf(buf, sizeof(buf), "void process_call_%d(int* result) {",
                 process_id);
        add_to_output(current_output_position, output_length, output, buf);

        add_to_output(current_output_position, output_length, output, "*result=");
        add_to_output(current_output_position, output_length, output, fn);
        add_to_output(current_output_position, output_length, output, "(");
        for (int j = 0; j < argc; j++) {
          emit_expression(call->body.function_call.arguments[j], output,
                          output_length, current_output_position);
          if (j < argc - 1)
            add_to_output(current_output_position, output_length, output, ", ");
        }
        add_to_output(current_output_position, output_length, output, ");");
        add_to_output(current_output_position, output_length, output,
                      "exit(0);}\n");
        process_id++;
      }
    }
  }

  add_to_output(current_output_position, output_length, output,
                "int main() {\n");

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.is_shared) {
      char buf[150];
      snprintf(buf, sizeof(buf), "pthread_mutex_init(&lock_%s, NULL);\n",
               stmt->body.var_declaration.variable_name);
      add_to_output(current_output_position, output_length, output, buf);
    }
  }

  threaded_worker_counter = 1;

  /* Emit shared memory for process result variables */
  {
    int process_id = 1;
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.variable_parallel_type ==
              PARALLEL_TYPE_PROCESS) {
        const char *name = stmt->body.var_declaration.variable_name;
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "%s_ptr = mmap(NULL, sizeof(int), "
                 "PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); "
                 "*%s_ptr = 0; pid_t _process_%s = fork(); "
                 "if (_process_%s == 0) { process_call_%d(%s_ptr); } ",
                 name, name, name, name, process_id, name);
        add_to_output(current_output_position, output_length, output, buf);
        /* Dummy thread handle so await can call pthread_join safely */
        char dummy[64];
        snprintf(dummy, sizeof(dummy), "pthread_t _thread_%s;", name);
        add_to_output(current_output_position, output_length, output, dummy);
        process_id++;
      }
    }
  }

  /* Emit thread call inlines (pthread_create calls) */
  emit_all_thread_call_inlines(node, output, output_length,
                               current_output_position);

  /* Join unnamed threads before other statements */
  {
    int unnamed_count = 0;
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if ((stmt->type == NODE_FUNCTION_CALL &&
           stmt->body.function_call.type == PARALLEL_TYPE_THREAD) ||
          stmt->type == NODE_THREAD) {
        unnamed_count++;
      }
    }
    for (int i = 1; i <= unnamed_count; i++) {
      char buf[128];
      snprintf(buf, sizeof(buf), "pthread_join(_thread__t%d, NULL);", i);
      add_to_output(current_output_position, output_length, output, buf);
    }
  }

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION || stmt->type == NODE_VAR_DECLARATION ||
        (stmt->type == NODE_FUNCTION_CALL &&
         (stmt->body.function_call.type == PARALLEL_TYPE_THREAD ||
          stmt->body.function_call.type == PARALLEL_TYPE_PROCESS)) ||
        stmt->type == NODE_THREAD) {
      continue;
    }
    emit_statement(stmt, output, output_length, current_output_position, node);
  }

  add_to_output(current_output_position, output_length, output,
                "\n  return 0;\n}\n");
}

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Could not open file '%s'\n", path);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)length + 1);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    fclose(f);
    return NULL;
  }
  fread(buf, 1, (size_t)length, f);
  buf[length] = '\0';
  fclose(f);
  return buf;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <source.flucs>\n", argv[0]);
    return 1;
  }

  char *source = read_file(argv[1]);
  if (!source)
    return 1;

  int output_length = 64;
  int current_output_position = 0;
  char *output = malloc((size_t)output_length);

  Lexer lexer = new_lexer(source);
  Node *root = parse(&lexer);
  semantic_analyze(root);
  emit_program(root, &output, &output_length, &current_output_position);

  printf("\nResult:\n%s\n", output);

  FILE *f = fopen("temp.c", "w");
  if (!f) {
    perror("fopen");
    free(source);
    free(output);
    return 1;
  }
  fputs(output, f);
  fclose(f);
  free(source);
  free(output);
  free_node(root);

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
