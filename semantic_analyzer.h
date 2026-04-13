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
  int threads[10];
  int thread_count;
  int is_shared;
  UT_hash_handle hh;
} VariableEntry;

VariableEntry *lookup_variable(const char *name);
void register_variable_usage(const char *name);
void semantic_analyze(Node *node);


#endif
