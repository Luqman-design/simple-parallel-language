#include "lexer.h"
#include "parser.h"
#include <stdio.h>

int main() {
  char *str = "print(x);\n";
  printf("Input: %s", str);
  Lexer lexer = new_lexer(str);

  Lexer lexer_copy = lexer;
  Token token;
  while (lexer_copy.position < lexer_copy.length - 1) {
    token = next_token(&lexer_copy);
    printf("Token: %d\n", token.type);
  }

  Node *root = parse(&lexer);

  return 0;
}
