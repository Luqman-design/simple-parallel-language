#include "lexer.h"
#include "parser.h"
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
  int a,
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

typedef struct {
  TokenType type;
  char[32] name;
} variable;

typedef struct {
  char key[32];
  variable *variable;
  UT_hash_handle hash_handle;
} declaration;

void semantic_analysis(Node *root) {
  int n = 2;
  declaration **map = calloc(n, sizeof(declaration));

  for (int i = 0; i < node->body.program.statement_count; i++) {
  }

  free(variables);

  return;
}

/* One struct = one hashmap entry /
typedef struct {
    char key[32];
    int  value;
    UT_hash_handle hh;  // makes this struct hashable
} Item;

int main(void) {
    / Dynamic array of hashmap pointers /
    int n = 2;
    Item **maps = calloc(n, sizeof(Item));  // [{}, {}]

    * Fill map[0] /
    Iteme = malloc(sizeof(Item));
    strcpy(e->key, "score");  e->value = 42;
    HASH_ADD_STR(maps[0], key, e);

    * Fill map[1] /
    e = malloc(sizeof(Item));
    strcpy(e->key, "score");  e->value = 99;
    HASH_ADD_STR(maps[1], key, e);

    / Read back /
    for (int i = 0; i < n; i++) {
        Itemfound;
  HASH_FIND_STR(maps[i], "score", found);
        if (found) printf("map[%d] score = %d\n", i, found->value);
    }

    return 0;
}
