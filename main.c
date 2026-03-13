#include "lexer.h"
#include "parser.h"
#include <stdio.h>

int main() {
  char *str = "if(a < b){print(x);}\n";
  Lexer lexer = new_lexer(str);

  Node *root = parse(&lexer);

  return 0;
}
