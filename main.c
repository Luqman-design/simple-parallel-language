#include <stdio.h>
#include <ctype.h>

typedef enum {
    INT, IDENTIFIER, EQUAL, SEMICOLON, INT_VALUE
} TokenType;

typedef enum {
    START, IN_INTEGER, IN_IDENTIFIER, ACCEPT
} State;

typedef struct {
    TokenType type;
    union {
        char *string_value;
        int int_value;
    } value;
} Token;

typedef struct {
    char *input;
    int position;
} Lexer;

Lexer new_lexer(char* input) {
    Lexer lexer;
    lexer.input = input;
    lexer.position = 0;
    return lexer;
}

char peek(Lexer *lexer) {
	return lexer -> input[lexer -> position];
}

Token next_token(Lexer lexer) {
	State state = START;
	char current_char = peek(&lexer);
	lexer.position++;
	char temp_token[64];
	int temp_token_tracker = 0;

	while(state != ACCEPT) {
		if (state == START && current_char == '=') {
			Token token;
			token.type = EQUAL;
			lexer.position++;
			return token;
		} else if (state == START && current_char == ';') {
			Token token;
			token.type = SEMICOLON;
			lexer.position++;
			return token;
		} else if (state == START && (current_char == '_' || isalpha(current_char))) {
			temp_token[temp_token_tracker] = current_char;
			temp_token_tracker++;
			lexer.position++;
			state = IN_IDENTIFIER;
		} else if (state == IN_IDENTIFIER && (current_char == '_' || isalpha(current_char))) {
			temp_token[temp_token_tracker] = current_char;
			temp_token_tracker++;
			lexer.position++;
		} else if (state == IN_IDENTIFIER) {
			temp_token[temp_token_tracker] = current_char;
			temp_token_tracker++;
			lexer.position++;
			
			Token token;
			token.type = IDENTIFIER;
			token.value.string_value = temp_token;
			return token;
		}
	}
}

/*
 * struct:
 *    type,
 *    værdi (enten string eller int) (hint: brug union)
 *
 * struct:
 *    input (vores kode string),
 *    position (hvilket char er vi ved)
 * 
 * Lexer new_lexer(char* input)
 *    Lav ny lexer of retuner den
 *
 * Token next_token(Lexer lexer)
 *    loop (stop loopet ved ACCEPT state)
 *
 *        if state is START and char == '='
 *            return Token(TokenType.EQUAL)
 *        if state is START and char == ';'
 *            return ...
 *
 *
 *        if state is START and if a-z | A-Z | _
 *            set state IN_IDENTIFIER
 *
 *        if state is IN_IDENTIFIER and if a-z | A-Z | _
 *            set state IN_IDENTIFIER
 *
 *        if state is IN_IDENTIFIER
 *            set state ACCEPT
 *
 *
 *        if state is START and if 0-9
 *            set state IN_INTEGER
 *
 *        if state is IN_INTEGER and if 0-9
 *            set state IN_INTEGER
 *
 *        if state is IN_INTEGER
 *            set state ACCEPT
 *
 *    switch på Identifier'en
 *        hvis den matcher et keyword (som 'int'),
 *              så lav den til et keyword i stedet.
 */

int main() {
    printf("Hello, World!\n");
    char *str = "int x = 10;";
    printf("%s", str);

    return 0;
}
