#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  int capacity;
  int length;
} OutputBuffer;

static void output_init(OutputBuffer *output, int initial_capacity) {
  output->data = malloc((size_t)initial_capacity);
  if (!output->data) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
  }
  output->data[0] = '\0';
  output->capacity = initial_capacity;
  output->length = 0;
}

static void output_append(OutputBuffer *output, const char *str) {
  int needed = (int)strlen(str) + output->length;
  while (needed >= output->capacity) {
    output->capacity *= 2;
  }
  output->data = realloc(output->data, (size_t)(output->capacity + 1));
  strcpy(output->data + output->length, str);
  output->length += (int)strlen(str);
}

static void output_appendf(OutputBuffer *output, const char *fmt, ...) {
  char buf[4096];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output_append(output, buf);
}

static int threaded_worker_counter = 1;

#define MAX_CAPTURED 20
static char captured_names[MAX_CAPTURED][32];
static int captured_count = 0;
static int saved_captured_count = 0;

static int is_captured_var(const char *name) {
  for (int i = 0; i < captured_count; i++) {
    if (strcmp(captured_names[i], name) == 0)
      return 1;
  }
  return 0;
}

/* Forward declarations */
static void emit_statement(Node *node, OutputBuffer *output, Node *program_node);
static void emit_expression(Node *node, OutputBuffer *output);
static void emit_function(Node *node, OutputBuffer *output);
static void emit_block(Node *node, OutputBuffer *output, Node *program_node);
static void emit_operand(Node *node, OutputBuffer *output);
static void emit_thread_call_wrapper(Node *node, const char *result_var,
                                     int wrapper_id, OutputBuffer *output,
                                     Node *program_node);
static void emit_thread_call_inline(Node *node, const char *result_var,
                                    int wrapper_id, OutputBuffer *output);
static void emit_all_thread_call_wrappers(Node *node, OutputBuffer *output);
static void emit_threaded_for_loop_worker(Node *node, OutputBuffer *output);

static const char *escape_string(const char *str, char *buf, int buf_size) {
  int j = 0;
  for (int i = 0; str[i] && j < buf_size - 2; i++) {
    if (str[i] == '\\' && str[i + 1]) {
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

static void emit_operand(Node *node, OutputBuffer *output) {
  switch (node->type) {
  case NODE_BINARY_OPERATION:
    output_append(output, "(");
    emit_operand(node->body.binary_operation.left_operand, output);
    output_append(output,
              operator_to_string(node->body.binary_operation.operator_type));
    emit_operand(node->body.binary_operation.right_operand, output);
    output_append(output, ")");
    break;
  case NODE_INT_VALUE:
    output_appendf(output, "%d", node->body.int_value.value);
    break;
  case NODE_STRING_VALUE:
    output_append(output, node->body.string_value.value);
    break;
  case NODE_IDENTIFIER: {
    const char *name = node->body.identifier.name;
    if (is_captured_var(name)) {
      output_appendf(output, "(*_%s)", name);
    } else {
      output_append(output, name);
    }
    break;
  }
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

static int find_function_return_type(Node *program_node, const char *function_name) {
  if (program_node->type != NODE_PROGRAM)
    return TOKEN_VOID;
  for (int i = 0; i < program_node->body.program.statement_count; i++) {
    Node *stmt = program_node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION &&
        strcmp(stmt->body.function.name, function_name) == 0) {
      return stmt->body.function.return_type;
    }
  }
  return TOKEN_VOID;
}

static void emit_thread_call_wrapper(Node *node, const char *result_var,
                                     int wrapper_id, OutputBuffer *output,
                                     Node *program_node) {
  const char *function_name = node->body.function_call.name;
  int argc = node->body.function_call.argument_count;

  output_appendf(output, "void* thread_call_%d(void* arg) {", wrapper_id);

  if (argc > 0) {
    output_append(output, "intptr_t* args=(intptr_t*)arg;");
  }

  if (result_var) {
    int ret_type = find_function_return_type(program_node, function_name);
    if (ret_type != TOKEN_VOID) {
      output_append(output, result_var);
      output_append(output, "=");
    }
  }

  output_append(output, function_name);
  output_append(output, "(");
  for (int i = 0; i < argc; i++) {
    output_appendf(output, "(int)args[%d]", i);
    if (i < argc - 1)
      output_append(output, ", ");
  }
  output_append(output, ");");
  output_append(output, "return NULL;}\n");
}

static void emit_thread_call_inline(Node *node, const char *result_var,
                                    int wrapper_id, OutputBuffer *output) {
  int argc = node->body.function_call.argument_count;

  output_appendf(output, "pthread_t _thread_%s; pid_t _process_%s = -1;", result_var,
             result_var);

  if (argc > 0) {
    char arr_name[64];
    snprintf(arr_name, sizeof(arr_name), "_args_%s", result_var);
    output_appendf(output, "intptr_t %s[%d]={", arr_name, argc);
    for (int i = 0; i < argc; i++) {
      emit_expression(node->body.function_call.arguments[i], output);
      if (i < argc - 1)
        output_append(output, ", ");
    }
    output_append(output, "};");
  }

  output_appendf(output, "pthread_create(&_thread_%s,NULL,thread_call_%d,", result_var,
             wrapper_id);

  if (argc > 0) {
    char arr_name[64];
    snprintf(arr_name, sizeof(arr_name), "_args_%s", result_var);
    output_appendf(output, "%s);", arr_name);
  } else {
    output_append(output, "NULL);");
  }
}

static void emit_all_thread_call_wrappers(Node *node, OutputBuffer *output) {
  if (node->type != NODE_PROGRAM)
    return;
  int wrapper_id = 1;
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION_CALL &&
        stmt->body.function_call.type == PARALLEL_TYPE_THREAD) {
      emit_thread_call_wrapper(stmt, NULL, wrapper_id, output, node);
      wrapper_id++;
    }
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD) {
      const char *result_var = stmt->body.var_declaration.variable_name;
      emit_thread_call_wrapper(stmt->body.var_declaration.variable_value,
                               result_var, wrapper_id, output, node);
      wrapper_id++;
    }
    if (stmt->type == NODE_THREAD) {
      output_appendf(output, "void* thread_call_%d(void* arg) {", wrapper_id);
      for (int j = 0; j < stmt->body.thread.statement_count; j++) {
        emit_statement(stmt->body.thread.statements[j], output, NULL);
      }
      output_append(output, "return NULL;}\n");
      wrapper_id++;
    }
  }
}

static void emit_all_thread_call_inlines(Node *node, OutputBuffer *output) {
  if (node->type != NODE_PROGRAM)
    return;
  int wrapper_id = 1;
  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION_CALL &&
        stmt->body.function_call.type == PARALLEL_TYPE_THREAD) {
      char handle[32];
      snprintf(handle, sizeof(handle), "_t%d", wrapper_id);
      emit_thread_call_inline(stmt, handle, wrapper_id, output);
      wrapper_id++;
    }
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD) {
      const char *result_var = stmt->body.var_declaration.variable_name;
      emit_thread_call_inline(stmt->body.var_declaration.variable_value,
                              result_var, wrapper_id, output);
      wrapper_id++;
    }
    if (stmt->type == NODE_THREAD) {
      output_appendf(output,
                 "pthread_t _thread__t%d;pthread_create(&_thread__t%d,NULL,"
                 "thread_call_%d,NULL);",
                 wrapper_id, wrapper_id, wrapper_id);
      wrapper_id++;
    }
  }
}

static void emit_function_call(Node *node, OutputBuffer *output) {
  const char *function_name = node->body.function_call.name;

  if (node->body.function_call.type == PARALLEL_TYPE_THREAD) {
    return;
  }

  if (node->body.function_call.type == PARALLEL_TYPE_PROCESS) {
    return;
  }

  output_append(output, function_name);
  output_append(output, "(");

  for (int i = 0; i < node->body.function_call.argument_count; i++) {
    emit_expression(node->body.function_call.arguments[i], output);
    if (i < node->body.function_call.argument_count - 1)
      output_append(output, ", ");
  }

  output_append(output, ")");
}

static void emit_expression(Node *node, OutputBuffer *output) {
  if (!node) {
    fprintf(stderr, "ERROR: NULL expression\n");
    exit(1);
  }

  switch (node->type) {
  case NODE_BINARY_OPERATION:
    emit_operand(node, output);
    break;
  case NODE_INT_VALUE:
    output_appendf(output, "%d", node->body.int_value.value);
    break;
  case NODE_STRING_VALUE: {
    char escaped[512];
    escape_string(node->body.string_value.value, escaped, sizeof(escaped));
    output_appendf(output, "\"%s\"", escaped);
    break;
  }
  case NODE_IDENTIFIER: {
    const char *name = node->body.identifier.name;
    if (is_captured_var(name)) {
      output_appendf(output, "(*_%s)", name);
    } else {
      output_append(output, name);
    }
    break;
  }
  case NODE_FUNCTION_CALL:
    emit_function_call(node, output);
    break;
  default:
    fprintf(stderr, "Unsupported expression type: %d\n", node->type);
    exit(1);
  }
}

static void emit_block(Node *node, OutputBuffer *output, Node *program_node) {
  for (int i = 0; i < node->body.block.statement_count; i++) {
    emit_statement(node->body.block.statements[i], output, program_node);
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

static void emit_function(Node *node, OutputBuffer *output) {
  output_append(output, c_type(node->body.function.return_type));
  output_append(output, " ");
  output_append(output, node->body.function.name);
  output_append(output, "(");

  for (int i = 0; i < node->body.function.param_count; i++) {
    output_append(output, c_type(node->body.function.params[i].type));
    output_append(output, " ");
    output_append(output, node->body.function.params[i].name);
    if (i < node->body.function.param_count - 1)
      output_append(output, ", ");
  }

  output_append(output, ") {");

  for (int i = 0; i < node->body.function.statement_count; i++) {
    emit_statement(node->body.function.statements[i], output, NULL);
  }

  output_append(output, "}\n");
}

static void emit_threaded_for_loop_worker(Node *node, OutputBuffer *output) {
  if (!node)
    return;

  if (node->type == NODE_FOR_LOOP &&
      node->body.for_loop.type == PARALLEL_TYPE_THREAD) {
    Node *for_loop_node = node;
    Node *cond = for_loop_node->body.for_loop.condition;
    const char *var =
        cond->body.binary_operation.left_operand->body.identifier.name;

    char rhs[64];
    Node *rhs_node = cond->body.binary_operation.right_operand;
    if (rhs_node->type == NODE_IDENTIFIER) {
      snprintf(rhs, sizeof(rhs), "%s", rhs_node->body.identifier.name);
    } else {
      snprintf(rhs, sizeof(rhs), "%d", rhs_node->body.int_value.value);
    }

    int start = for_loop_node->body.for_loop.initializer->body.var_declaration
                    .variable_value->body.int_value.value;
    int thread_count = for_loop_node->body.for_loop.thread_amount;
    int captured_count_local = for_loop_node->body.for_loop.captured_count;

    for_loop_node->body.for_loop.worker_id = threaded_worker_counter;

    saved_captured_count = captured_count;
    captured_count = 0;
    for (int i = 0; i < captured_count_local; i++) {
      strcpy(captured_names[captured_count],
             for_loop_node->body.for_loop.captured_names[i]);
      captured_count++;
    }

    output_appendf(output,
               "void *for_loop_worker_%d(void *arg) { "
               "intptr_t* _args = (intptr_t*)arg; "
               "int start_index = (int)_args[0] + %d; ",
               threaded_worker_counter, start);

    for (int i = 0; i < captured_count_local; i++) {
      output_appendf(output, "%s* _%s = (%s*)_args[%d]; ",
                 c_type(for_loop_node->body.for_loop.captured_types[i]),
                 for_loop_node->body.for_loop.captured_names[i],
                 c_type(for_loop_node->body.for_loop.captured_types[i]), i + 1);

      output_appendf(output, "pthread_mutex_t* _lock_%s = (pthread_mutex_t*)_args[%d]; ",
                 for_loop_node->body.for_loop.captured_names[i], captured_count_local + i + 1);
    }

    output_appendf(output, "for (int %s = start_index; %s %s %s; %s = %s + %d) { ", var,
               var, operator_to_string(cond->body.binary_operation.operator_type),
               rhs, var, var, thread_count);

    for (int i = 0; i < captured_count_local; i++) {
      output_appendf(output, "pthread_mutex_lock(_lock_%s);",
                 for_loop_node->body.for_loop.captured_names[i]);
    }

    emit_block(for_loop_node->body.for_loop.body, output, NULL);

    for (int i = 0; i < captured_count_local; i++) {
      output_appendf(output, "pthread_mutex_unlock(_lock_%s);",
                 for_loop_node->body.for_loop.captured_names[i]);
    }

    output_append(output, "} return NULL; }");

    captured_count = saved_captured_count;
    threaded_worker_counter++;
  }

  switch (node->type) {
  case NODE_FOR_LOOP:
    emit_threaded_for_loop_worker(node->body.for_loop.body, output);
    break;
  case NODE_BLOCK:
    for (int i = 0; i < node->body.block.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.block.statements[i], output);
    break;
  case NODE_PROGRAM:
    for (int i = 0; i < node->body.program.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.program.statements[i], output);
    break;
  case NODE_FUNCTION:
    for (int i = 0; i < node->body.function.statement_count; i++)
      emit_threaded_for_loop_worker(node->body.function.statements[i], output);
    break;
  case NODE_IF_STATEMENT:
    emit_threaded_for_loop_worker(node->body.if_statement.then_branch, output);
    if (node->body.if_statement.else_branch)
      emit_threaded_for_loop_worker(node->body.if_statement.else_branch, output);
    break;
  default:
    break;
  }
}

static void emit_statement(Node *node, OutputBuffer *output, Node *program_node) {
  switch (node->type) {
  case NODE_IF_STATEMENT: {
    output_append(output, "if (");
    emit_expression(node->body.if_statement.condition, output);
    output_append(output, "){");
    emit_block(node->body.if_statement.then_branch, output, program_node);
    output_append(output, "}");
    if (node->body.if_statement.else_branch) {
      output_append(output, "else {");
      emit_block(node->body.if_statement.else_branch, output, program_node);
      output_append(output, "}");
    }
    break;
  }

  case NODE_PRINT: {
    Node *print_value = node->body.print.print_value;
    if (print_value->type == NODE_INT_VALUE) {
      output_appendf(output, "printf(\"%%d\",%d);", print_value->body.int_value.value);
    } else if (print_value->type == NODE_STRING_VALUE) {
      char escaped[512];
      escape_string(print_value->body.string_value.value, escaped, sizeof(escaped));
      output_appendf(output, "printf(\"%%s\",\"%s\");", escaped);
    } else if (print_value->type == NODE_IDENTIFIER) {
      const char *fmt = (print_value->body.identifier.type == TOKEN_STRING_TYPE)
                            ? "\"%s\""
                            : "\"%d\"";
      output_appendf(output, "printf(%s,%s);", fmt, print_value->body.identifier.name);
    }
    break;
  }

  case NODE_VAR_DECLARATION: {
    const char *var_name = node->body.var_declaration.variable_name;

    if (node->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_THREAD ||
        node->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_PROCESS) {
      /* Handled by emit_all_thread_call_inlines */
    } else {
      output_append(output, c_type(node->body.var_declaration.variable_type));
      output_append(output, " ");
      output_append(output, var_name);
      output_append(output, "=");
      emit_expression(node->body.var_declaration.variable_value, output);
      output_append(output, ";");
    }
    break;
  }

  case NODE_VAR_UPDATE: {
    const char *varname = node->body.var_update.variable_name;
    int captured = is_captured_var(varname);

    if (node->body.var_update.is_shared) {
      if (captured) {
        output_appendf(output, "pthread_mutex_lock(_lock_%s);", varname);
      } else {
        output_appendf(output, "pthread_mutex_lock(&lock_%s);", varname);
      }
    }

    if (captured) {
      output_appendf(output, "(*_%s)", varname);
    } else {
      output_append(output, varname);
    }

    switch (node->body.var_update.operator_type) {
    case TOKEN_EQUAL:
      output_append(output, "=");
      break;
    case TOKEN_PLUS_EQUAL:
      output_append(output, "+=");
      break;
    case TOKEN_MINUS_EQUAL:
      output_append(output, "-=");
      break;
    case TOKEN_PLUS_PLUS:
      output_append(output, "++");
      break;
    default:
      break;
    }

    if (node->body.var_update.value) {
      emit_expression(node->body.var_update.value, output);
    }
    output_append(output, ";");

    if (node->body.var_update.is_shared) {
      if (captured) {
        output_appendf(output, "pthread_mutex_unlock(_lock_%s);", varname);
      } else {
        output_appendf(output, "pthread_mutex_unlock(&lock_%s);", varname);
      }
    }
    break;
  }

  case NODE_FOR_LOOP: {
    if (node->body.for_loop.type == PARALLEL_TYPE_THREAD) {
      int thread_count = node->body.for_loop.thread_amount;
      int captured_count_local = node->body.for_loop.captured_count;
      int worker_id = node->body.for_loop.worker_id;

      saved_captured_count = captured_count;
      captured_count = 0;
      for (int i = 0; i < captured_count_local; i++) {
        strcpy(captured_names[captured_count],
               node->body.for_loop.captured_names[i]);
        captured_count++;
      }

      if (captured_count_local == 0) {
        output_appendf(
            output,
            "pthread_t threads[%d]; int starts[%d]; "
            "for (int i = 0; i < %d; i++) { starts[i] = i; "
            "pthread_create(&threads[i], NULL, for_loop_worker_%d, "
            "&starts[i]); } "
            "for (int i = 0; i < %d; i++) { pthread_join(threads[i], NULL); }",
            thread_count, thread_count, thread_count, worker_id, thread_count);
      } else {
        int total_args = 1 + captured_count_local * 2;

        for (int i = 0; i < captured_count_local; i++) {
          output_appendf(output, "pthread_mutex_t _lock_%s;",
                     node->body.for_loop.captured_names[i]);
          output_appendf(output, "pthread_mutex_init(&_lock_%s, NULL);",
                     node->body.for_loop.captured_names[i]);
        }

        output_appendf(output,
                   "intptr_t _fargs[%d][%d]; "
                   "for (int _fi = 0; _fi < %d; _fi++) { "
                   "_fargs[_fi][0] = _fi; ",
                   thread_count, total_args, thread_count);

        for (int i = 0; i < captured_count_local; i++) {
          output_appendf(output,
                     "_fargs[_fi][%d] = (intptr_t)&%s; "
                     "_fargs[_fi][%d] = (intptr_t)&_lock_%s; ",
                     i + 1, node->body.for_loop.captured_names[i],
                     captured_count_local + i + 1, node->body.for_loop.captured_names[i]);
        }
        output_append(output, "} ");

        output_appendf(output,
                   "pthread_t threads[%d]; "
                   "for (int i = 0; i < %d; i++) { "
                   "pthread_create(&threads[i], NULL, for_loop_worker_%d, "
                   "_fargs[i]); } "
                   "for (int i = 0; i < %d; i++) { pthread_join(threads[i], "
                   "NULL); }",
                   thread_count, thread_count, worker_id, thread_count);

        for (int i = 0; i < captured_count_local; i++) {
          output_appendf(output, "pthread_mutex_destroy(&_lock_%s);",
                     node->body.for_loop.captured_names[i]);
        }
      }

      captured_count = saved_captured_count;
      return;
    }

    output_append(output, "for (");
    emit_statement(node->body.for_loop.initializer, output, program_node);
    output_append(output, " ");
    emit_expression(node->body.for_loop.condition, output);
    output_append(output, "; ");

    Node *updater_node = node->body.for_loop.updater;
    output_append(output, updater_node->body.var_update.variable_name);
    switch (updater_node->body.var_update.operator_type) {
    case TOKEN_EQUAL:
      output_append(output, "=");
      break;
    case TOKEN_PLUS_EQUAL:
      output_append(output, "+=");
      break;
    case TOKEN_MINUS_EQUAL:
      output_append(output, "-=");
      break;
    case TOKEN_PLUS_PLUS:
      output_append(output, "++");
      break;
    default:
      break;
    }
    if (updater_node->body.var_update.value) {
      emit_expression(updater_node->body.var_update.value, output);
    }
    output_append(output, ") {");
    emit_block(node->body.for_loop.body, output, program_node);
    output_append(output, "}");
    break;
  }

  case NODE_RETURN_STATEMENT: {
    output_append(output, "return ");
    emit_expression(node->body.return_statement.expression, output);
    output_append(output, ";");
    break;
  }

  case NODE_FUNCTION_CALL: {
    emit_function_call(node, output);
    output_append(output, ";");
    break;
  }

  case NODE_AWAIT: {
    for (int i = 0; i < node->body.thread.statement_count; i++) {
      Node *id_node = (Node *)node->body.thread.statements[i];
      const char *name = id_node->body.identifier.name;
      if (is_process_variable(program_node, name)) {
        output_appendf(output, "waitpid(_process_%s, NULL, 0); %s = *%s_ptr;", name,
                   name, name);
      } else {
        output_appendf(output, "pthread_join(_thread_%s, NULL);", name);
      }
    }
    break;
  }

  default:
    break;
  }
}

static void emit_program(Node *node, OutputBuffer *output) {
  if (node->type != NODE_PROGRAM)
    return;

  output_append(output,
            "#include <stdlib.h>\n"
            "#include <stdio.h>\n"
            "#include <stdint.h>\n"
            "#include <pthread.h>\n"
            "#include <unistd.h>\n"
            "#include <sys/wait.h>\n"
            "#include <sys/mman.h>\n");

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        (stmt->body.var_declaration.variable_parallel_type ==
             PARALLEL_TYPE_THREAD ||
         stmt->body.var_declaration.variable_parallel_type ==
             PARALLEL_TYPE_PROCESS)) {
      const char *name = stmt->body.var_declaration.variable_name;
      output_appendf(output, "%s %s = 0;\n",
                 c_type(stmt->body.var_declaration.variable_type), name);
      if (stmt->body.var_declaration.variable_parallel_type ==
          PARALLEL_TYPE_PROCESS) {
        output_appendf(output, "int* %s_ptr;\n", name);
      }
    }
  }

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.variable_parallel_type ==
            PARALLEL_TYPE_REGULAR) {
      output_appendf(output, "%s %s = ", c_type(stmt->body.var_declaration.variable_type),
                 stmt->body.var_declaration.variable_name);
      emit_expression(stmt->body.var_declaration.variable_value, output);
      output_append(output, ";\n");
    }
  }

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.is_shared) {
      output_appendf(output, "pthread_mutex_t lock_%s;\n",
                 stmt->body.var_declaration.variable_name);
    }
  }

  threaded_worker_counter = 1;
  emit_threaded_for_loop_worker(node, output);

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_FUNCTION) {
      emit_function(stmt, output);
      output_append(output, "\n");
    }
  }

  emit_all_thread_call_wrappers(node, output);

  {
    int process_id = 1;
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.variable_parallel_type ==
              PARALLEL_TYPE_PROCESS) {
        Node *call = stmt->body.var_declaration.variable_value;
        const char *function_name = call->body.function_call.name;
        int argc = call->body.function_call.argument_count;

        output_appendf(output, "void process_call_%d(int* result) {", process_id);
        output_append(output, "*result=");
        output_append(output, function_name);
        output_append(output, "(");
        for (int j = 0; j < argc; j++) {
          emit_expression(call->body.function_call.arguments[j], output);
          if (j < argc - 1)
            output_append(output, ", ");
        }
        output_append(output, ");");
        output_append(output, "exit(0);}\n");
        process_id++;
      }
    }
  }

  output_append(output, "int main() {\n");

  for (int i = 0; i < node->body.program.statement_count; i++) {
    Node *stmt = node->body.program.statements[i];
    if (stmt->type == NODE_VAR_DECLARATION &&
        stmt->body.var_declaration.is_shared) {
      output_appendf(output, "pthread_mutex_init(&lock_%s, NULL);\n",
                 stmt->body.var_declaration.variable_name);
    }
  }

  threaded_worker_counter = 1;

  {
    int process_id = 1;
    for (int i = 0; i < node->body.program.statement_count; i++) {
      Node *stmt = node->body.program.statements[i];
      if (stmt->type == NODE_VAR_DECLARATION &&
          stmt->body.var_declaration.variable_parallel_type ==
              PARALLEL_TYPE_PROCESS) {
        const char *name = stmt->body.var_declaration.variable_name;
        output_appendf(
            output,
            "%s_ptr = mmap(NULL, sizeof(int), "
            "PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0); "
            "*%s_ptr = 0; pid_t _process_%s = fork(); "
            "if (_process_%s == 0) { process_call_%d(%s_ptr); } ",
            name, name, name, name, process_id, name);
        output_appendf(output, "pthread_t _thread_%s;", name);
        process_id++;
      }
    }
  }

  emit_all_thread_call_inlines(node, output);

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
      output_appendf(output, "pthread_join(_thread__t%d, NULL);", i);
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
    emit_statement(stmt, output, node);
  }

  output_append(output, "\n  return 0;\n}\n");
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

  OutputBuffer output;
  output_init(&output, 64);

  Lexer lexer = new_lexer(source);
  Node *root = parse(&lexer);
  semantic_analyze(root);
  emit_program(root, &output);

  printf("\nResult:\n%s\n", output.data);

  FILE *f = fopen("temp.c", "w");
  if (!f) {
    perror("fopen");
    free(source);
    free(output.data);
    return 1;
  }
  fputs(output.data, f);
  fclose(f);
  free(source);
  free(output.data);
  free_node(root);

#if defined(_WIN32)
  system("gcc temp.c -o temp.exe");
  system("temp.exe");
#elif defined(__linux__) || defined(__APPLE__)
  system("gcc temp.c -o temp");
  system("./temp");
#else
  fprintf(stderr, "Unsupported OS\n");
#endif

  return 0;
}
