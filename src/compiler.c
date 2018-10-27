#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
	Token current;
	Token previous;
	bool had_error;
	bool panic_mode;
} Parser;

// precedence from lowest to highest
typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,    // =
	PREC_OR,            // or
	PREC_AND,           // and
	PREC_EQUALITY,      // == !=
	PREC_COMPARISON,    // < > <= >=
	PREC_TERM,          // + -
	PREC_FACTOR,        // * /
	PREC_UNARY,         // ! - +
	PREC_CALL,          // . () []
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;

static void errorAt(Token *token, const char *message) {
	if (parser.panic_mode) return;
	parser.panic_mode = true;

	fprintf(stderr, "error: line %d", token->line);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, ", at end");
	} else if (token->type == TOKEN_ERROR) {
		// do nothing.
	} else {
		fprintf(stderr, ", at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s.\n", message);
	parser.had_error = true;
}

static void error(const char *message) {
	errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message) {
	errorAt(&parser.current, message);
}

static void advance() {
	parser.previous = parser.current;

	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

static void consume(TokenType type, const char *message) {
	if (parser.current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

Chunk *compiling_chunk;

static Chunk *currentChunk() {
	return compiling_chunk;
}

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitByteWithLine(uint8_t byte, int line) {
	writeChunk(currentChunk(), byte, line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn() {
	emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error("too many constants in one chunk");
		return 0;
	}

	return (uint8_t)constant;
}

static void emitConstant(Value value) {
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
	emitReturn();
	#ifdef CLOX_DEBUG_PRINT_CODE
		if (!parser.had_error) {
			disassembleChunk(currentChunk(), "code");
		}
	#endif
}

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
	if (!prefix_rule) {
		error("expected an expression");
		return;
	}

	prefix_rule();

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infix_rule = getRule(parser.previous.type)->infix;
		infix_rule();
	}
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void number() {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void binary() {
	TokenType operator_type = parser.previous.type;
	int operator_line = parser.previous.line;

	ParseRule *rule = getRule(operator_type);
	parsePrecedence((Precedence)(rule->precedence + 1));

	switch (operator_type) {
		case TOKEN_PLUS:  emitByteWithLine(OP_ADD, operator_line); break;
		case TOKEN_MINUS: emitByteWithLine(OP_SUB, operator_line); break;
		case TOKEN_STAR:  emitByteWithLine(OP_MUL, operator_line); break;
		case TOKEN_SLASH: emitByteWithLine(OP_DIV, operator_line); break;
		default:
			return; // unreachable.
	}
}

static void unary() {
	Token operator = parser.previous;

	parsePrecedence(PREC_UNARY);

	switch (operator.type) {
		case TOKEN_MINUS: emitByteWithLine(OP_NEGATE, operator.line); break;
		default:
			return; // unreachable.
	}
}

static void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

// has to be in the same order as TokenType enum
ParseRule rules[] = {
	{ grouping, NULL,    PREC_CALL },       // TOKEN_LEFT_PAREN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
	{ NULL,     NULL,    PREC_CALL },       // TOKEN_DOT
	{ unary,    binary,  PREC_TERM },       // TOKEN_MINUS
	{ NULL,     binary,  PREC_TERM },       // TOKEN_PLUS
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON
	{ NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH
	{ NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_BANG
	{ NULL,     NULL,    PREC_EQUALITY },   // TOKEN_BANG_EQUAL
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
	{ NULL,     NULL,    PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
	{ NULL,     NULL,    PREC_COMPARISON }, // TOKEN_GREATER
	{ NULL,     NULL,    PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
	{ NULL,     NULL,    PREC_COMPARISON }, // TOKEN_LESS
	{ NULL,     NULL,    PREC_COMPARISON }, // TOKEN_LESS_EQUAL
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_STRING
	{ number,   NULL,    PREC_NONE },       // TOKEN_NUMBER
	{ NULL,     NULL,    PREC_AND },        // TOKEN_AND
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FALSE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_IF
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_NIL
	{ NULL,     NULL,    PREC_OR },         // TOKEN_OR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_THIS
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_TRUE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_VAR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EOF
};

static ParseRule *getRule(TokenType type) {
	return &rules[type];
}

bool compile(const char *source, Chunk *chunk) {
	initScanner(source);

	compiling_chunk = chunk;
	parser.had_error = false;
	parser.panic_mode = false;

	advance();
	expression();
	consume(TOKEN_EOF, "expected end of expression");
	endCompiler();
	return !parser.had_error;
}