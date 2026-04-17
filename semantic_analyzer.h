#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "lexer.h"
#include "parser.h"
#include "uthash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SCOPE 100

typedef struct {
  char name[32];
  TokenType type;
  int thread_ids[10];
  int thread_count;
  int is_shared;
  UT_hash_handle hh;

  // To keep in sync:
  Node *variable_declaration_node;
  // Node *variable_update;
} VariableEntry;

VariableEntry *lookup_variable(const char *name);
void propagate_shared_flags(Node *node);
void semantic_analyze(Node *node);

#endif
