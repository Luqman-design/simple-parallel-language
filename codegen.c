

/*

function emit_statement(Nodem output) {
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
