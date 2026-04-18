#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "lexer.h"
#include "parser.h"
#include "uthash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SCOPE 100
#define MAX_NAME_LEN 32
#define MAX_THREAD_IDS 10

typedef struct {
  char name[MAX_NAME_LEN];
  TokenType type;
  int thread_ids[MAX_THREAD_IDS];
  int thread_count;
  int is_shared;
  UT_hash_handle hh;

  Node *variable_declaration_node;
} VariableEntry;

VariableEntry *lookup_variable(const char *name);
void semantic_analyze(Node *root);

#endif
