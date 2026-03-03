#include "lexer.h"
#include "parser.h"
#include <stdio.h>

int main() {
  char *str = "int x = 10;\n";
  printf("Input: %s", str);
  Lexer lexer = new_lexer(str);

  Lexer lexer_copy = lexer;
  Token token;
  while (lexer_copy.position < lexer_copy.length) {
    token = next_token(&lexer_copy);
    printf("Token: %d\n", token.type);
  }

  Node *root = parse(&lexer);

  return 0;
}
