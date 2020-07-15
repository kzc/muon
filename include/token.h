#ifndef BOSON_TOKEN_H
#define BOSON_TOKEN_H

#include <stddef.h>

enum token_type {
	TOKEN_EOF,
	TOKEN_EOL,

	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACK,
	TOKEN_RBRACK,
	TOKEN_LCURL,
	TOKEN_RCURL,
	TOKEN_DOT,
	TOKEN_COMMA,
	TOKEN_COLON,

	TOKEN_ASSIGN,

	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_STAR,
	TOKEN_SLASH,
	TOKEN_MODULO,

	TOKEN_PLUSEQ,
	TOKEN_MINUSEQ,
	TOKEN_STAREQ,
	TOKEN_SLASHEQ,
	TOKEN_MODULOEQ,

	TOKEN_EQ,
	TOKEN_NEQ,
	TOKEN_GT,
	TOKEN_GEQ,
	TOKEN_LT,
	TOKEN_LEQ,

	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_ELIF,
	TOKEN_ENDIF,
	TOKEN_AND,
	TOKEN_OR,
	TOKEN_NOT,
	TOKEN_QM,
	TOKEN_FOREACH,
	TOKEN_ENDFOREACH,
	TOKEN_IN,
	TOKEN_CONTINUE,
	TOKEN_BREAK,

	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_NUMBER,
};

struct token {
	enum token_type type;
	size_t n;
	char *data;
};

void token_destroy(struct token *);

const char *token_type_to_string(enum token_type);
const char *token_to_string(struct token *);

#endif // BOSON_TOKEN_H
