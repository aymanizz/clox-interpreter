#include <string.h>
#include <ctype.h>

#include "common.h"
#include "scanner.h"

typedef struct {
	const char *start;
	const char *current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
	scanner.start = scanner.current = source;
	scanner.line = 1;
}

static Token makeToken(TokenType type) {
	return (Token) {
		.type = type,
		.start = scanner.start,
		.length = (int)(scanner.current - scanner.start),
		.line = scanner.line
	};
}

static Token errorToken(const char *error) {
	return (Token) {
		.type = TOKEN_ERROR,
		.start = error,
		.length = strlen(error),
		.line = scanner.line
	};
}

static bool isAtEnd() {
	return *scanner.current == '\0';
}

static char advance() {
	return *scanner.current++;
}

static char peek() {
	return *scanner.current;
}

static char peekNext() {
	if (isAtEnd()) return '\0';
	return scanner.current[1];
}

static bool match(const char expected) {
	if (isAtEnd() || peek() != expected)
		return false;

	advance();
	return false;
}

static bool isDigit(const char c) {
	return c >= '0' && c <= '9';
}

static bool isAlpha(const char c) {
	return (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z');
}

static bool isAlphanum(const char c) {
	return isAlpha(c) || isDigit(c);
}

static void skipWhitespace() {
	for (;;) {
		switch(peek()) {
			case '\n':
				++scanner.line;
			case ' ':
			case '\t':
			case '\r':
				advance();
				break;
			case '/':
				if (peekNext() == '/') {
					while (peek() != '\n' && !isAtEnd()) advance();
					advance();
					break;
				}
			default:
				return;
		}
	}
}

static TokenType checkKeyword(int start, int length,
		const char *rest, TokenType type) {
	if (scanner.current - scanner.start == start + length
		&& !memcmp(scanner.start + start, rest, length)) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	/* hardcoded trie */
	switch(scanner.start[0]) {
		case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'f': {
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
					case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
				}
			}
		}
		case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
		case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 't': {
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'h': return checkKeyword(2, 2, "is", TOKEN_FALSE);
					case 'r': return checkKeyword(2, 2, "ue", TOKEN_FOR);
				}
			}
		}
		case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
		case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
	}

	return TOKEN_IDENTIFIER;
}

static Token identifierToken() {
	while (isAlphanum(peek())) advance();

	return makeToken(identifierType());
}

static Token numberToken() {
	while (isDigit(peek())) advance();

	if (peek() == '.' && isDigit(peekNext())) {
		advance();
		while (isDigit(peek())) advance();
	}

	return makeToken(TOKEN_NUMBER);
}

static Token stringToken() {
	while (!isAtEnd() && peek() != '"') {
		if (peek() == '\n') ++scanner.line;
		advance();
	}

	if (isAtEnd()) return errorToken("unterminated string literal");

	advance();
	return makeToken(TOKEN_STRING);
}

Token scanToken() {
	skipWhitespace();
	scanner.start = scanner.current;

	if (isAtEnd()) return makeToken(TOKEN_EOF);

	const char c = advance();

	if (isAlpha(c)) return identifierToken();
	if (isDigit(c)) return numberToken();
	
	switch (c) {
		case '(': return makeToken(TOKEN_LEFT_PAREN);
		case ')': return makeToken(TOKEN_RIGHT_PAREN);
		case '{': return makeToken(TOKEN_LEFT_BRACE);
		case '}': return makeToken(TOKEN_RIGHT_BRACE);
		case ',': return makeToken(TOKEN_COMMA);
		case '.': return makeToken(TOKEN_DOT);
		case '-': return makeToken(TOKEN_MINUS);
		case '+': return makeToken(TOKEN_PLUS);
		case ';': return makeToken(TOKEN_SEMICOLON);
		case '/': return makeToken(TOKEN_SLASH);
		case '*': return makeToken(TOKEN_STAR);
		case '!': return makeToken(
			match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
		case '=': return makeToken(
			match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '>': return makeToken(
			match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
		case '<': return makeToken(
			match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '"': return stringToken();
		default: {

		}
	}

	return errorToken("unexpected character");
}