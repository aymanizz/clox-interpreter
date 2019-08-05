#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

typedef struct {
	Token name;
	int depth;
} Local;

typedef struct {
	Local locals[UINT8_COUNT];
	int local_count;
	int scope_depth;
} Compiler;

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

typedef void (*ParseFn)(bool can_assign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;
Compiler* current = NULL;

static void initCompiler(Compiler* c) {
	c->local_count = 0;
	c->scope_depth = 0;
	current = c;
}

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

static bool check(TokenType type) {
	return parser.current.type == type;
}

static bool match(TokenType type) {
	if (!check(type)) return false;
	advance();
	return true;
}

static void synchronize() {
	parser.panic_mode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON){
			return;
		}

		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUN:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_WHILE:
			case TOKEN_IF:
			case TOKEN_RETURN:
				return;
			default:
				; // pass.
		}

		advance();
	}
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

static void emitBytesWithLine(uint8_t byte1, uint8_t byte2, int line) {
	emitByteWithLine(byte1, line);
	emitByteWithLine(byte2, line);
}

static int emitJump(uint8_t jump) {
	emitByte(jump);
	// 2 bytes jump target for later patching
	emitByte(0xff);
	emitByte(0xff);
	return currentChunk()->size - 2;
}

static void patchJump(int offset) {
	int jump = currentChunk()->size - offset - 2;
	if (jump > UINT8_MAX)
		error("too much code to jump over");

	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitLoop(int start) {
	emitByte(OP_LOOP);

	int jump = currentChunk()->size - start + 2;
	if (jump > UINT8_MAX)
		error("loop body too large");

	emitByte((jump >> 8) & 0xff);
	emitByte(jump & 0xff);
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
	// TODO: remove when OP_RETURN is correctly implemented
	emitReturn();

	#ifdef CLOX_DEBUG_PRINT_CODE
		if (!parser.had_error) {
			disassembleChunk(currentChunk(), "code");
		}
	#endif
}

static void beginScope() {
	++current->scope_depth;
}

static bool isTopLocalOutOfScope() {
	if (current->local_count == 0) return false;
	int current_depth = current->locals[current->local_count - 1].depth;
	return current_depth > current->scope_depth;
}

static void endScope() {
	--current->scope_depth;

	while (isTopLocalOutOfScope()) {
		emitByte(OP_POP);
		--current->local_count;
	}
}

static ParseRule *getRule(TokenType type);
static void declaration();
static void statement();
static void expression();

static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
	if (!prefix_rule) {
		error("expected an expression");
		return;
	}

	bool can_assign = precedence <= PREC_ASSIGNMENT;
	prefix_rule(can_assign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infix_rule = getRule(parser.previous.type)->infix;
		infix_rule(can_assign);
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		error("invalid assignment target");
		expression();
	}
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void expressionStatement() {
	expression();
	emitByte(OP_POP);
	consume(TOKEN_SEMICOLON, "expected ';' after expression");
}

static void ifStatement() {
	consume(TOKEN_LEFT_PAREN, "expected '(' after 'if'");
	expression();
	consume(TOKEN_RIGHT_PAREN, "expected ')' after condition");

	int then_jump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	int else_jump = emitJump(OP_JUMP);
	patchJump(then_jump);
	emitByte(OP_POP);
	if (match(TOKEN_ELSE))
		statement();

	patchJump(else_jump);
}

static void whileStatement() {
	int start = currentChunk()->size;

	consume(TOKEN_LEFT_PAREN, "expected a '(' after 'while'");
	expression();
	consume(TOKEN_RIGHT_PAREN, "expected a ')' after condition");

	int jump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(start);
	patchJump(jump);
	emitByte(OP_POP);
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "expected '}' after block");
}

static void statement() {
	if (match(TOKEN_IF)) {
		ifStatement();
	} else if (match(TOKEN_WHILE)) {
		whileStatement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	} else {
		expressionStatement();
	}
}

static uint8_t identifierConstant(Token *name) {
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b) {
	assert(a->type == TOKEN_IDENTIFIER && a->type == b->type);
	return a->length == b->length && !memcmp(a->start, b->start, a->length);
}

static void addLocal(Token name) {
	if (current->local_count == UINT8_COUNT) {
		error("too many local variables in function");
		return;
	}

	Local *local = &current->locals[current->local_count++];
	local->name = name;
	local->depth = -1;
}

static void declareVariable() {
	if (current->scope_depth == 0)
		return;

	Token *name = &parser.previous;
	for (int i = current->local_count - 1; i >= 0; --i) {
		Local *local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scope_depth)
			break;
		else if (identifiersEqual(name, &local->name))
			error("Variable with this name already declared in this scope");
	}

	addLocal(*name);
}

static uint8_t parseVariable(const char *error_msg) {
	consume(TOKEN_IDENTIFIER, error_msg);
	declareVariable();
	if (current->scope_depth > 0)
		return 0;
	return identifierConstant(&parser.previous);
}

static void markInitialized() {
	if (current->scope_depth == 0)
		return;
	current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void defineVariable(uint8_t global) {
	if (current->scope_depth > 0) {
		markInitialized();
		return;
	}
	emitBytes(OP_DEF_GLOBAL, global);
}

static void varDeclaration() {
	uint8_t global = parseVariable("expected variable name");

	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "expected ';' after variable declaration");

	defineVariable(global);
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	} else {
		statement();
	}

	if (parser.panic_mode) synchronize();
}

static void and_(bool can_assign) {
	int jump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);
	patchJump(jump);
}

static void or_(bool can_assign) {
	int jump = emitJump(OP_JUMP_IF_FALSE);
	int end_jump = emitJump(OP_JUMP);
	patchJump(jump);
	emitByte(OP_POP);
	parsePrecedence(PREC_OR);
	patchJump(end_jump);
}

static void literal(bool can_assign) {
	switch (parser.previous.type) {
		case TOKEN_NIL: emitByte(OP_NIL); break;
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		default:
			return; // unreachable
	}
}

static void number(bool can_assign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
	emitConstant(OBJ_VAL(copyString(
		parser.previous.start + 1, parser.previous.length - 2)));
}

static int resolveLocal(Compiler *compiler, Token *name) {
	for (int i = compiler->local_count - 1; i >= 0; --i) {
		Local *local = &compiler->locals[i];
		if (identifiersEqual(&local->name, name)) {
			if (local->depth == -1)
				error("local variable referenced before assignement");
			return i;
		}
	}
	return -1;
}

static void variable(bool can_assign) {
	Token *name = &parser.previous;
	int arg = resolveLocal(current, name);
	uint8_t get_op, set_op;
	if (arg != -1) {
		get_op = OP_GET_LOCAL;
		set_op = OP_SET_LOCAL;
	} else {
		arg = identifierConstant(name);
		get_op = OP_GET_GLOBAL;
		set_op = OP_SET_GLOBAL;
	}

	if (can_assign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(set_op, (uint8_t)arg);
	} else {
		emitBytes(get_op, (uint8_t)arg);
	}
}

static void binary(bool can_assign) {
	Token operator = parser.previous;

	ParseRule *rule = getRule(operator.type);
	parsePrecedence((Precedence)(rule->precedence + 1));

	switch (operator.type) {
		case TOKEN_PLUS:  emitByteWithLine(OP_ADD, operator.line); break;
		case TOKEN_MINUS: emitByteWithLine(OP_SUB, operator.line); break;
		case TOKEN_STAR:  emitByteWithLine(OP_MUL, operator.line); break;
		case TOKEN_SLASH: emitByteWithLine(OP_DIV, operator.line); break;
		case TOKEN_BANG_EQUAL:
			emitBytesWithLine(OP_EQUAL, OP_NOT, operator.line); break;
		case TOKEN_EQUAL_EQUAL:
			emitByteWithLine(OP_EQUAL, operator.line); break;
		case TOKEN_GREATER:
			emitByteWithLine(OP_GREATER, operator.line); break;
		case TOKEN_GREATER_EQUAL:
			emitBytesWithLine(OP_LESS, OP_NOT, operator.line); break;
		case TOKEN_LESS:
			emitByteWithLine(OP_LESS, operator.line); break;
		case TOKEN_LESS_EQUAL:
			emitBytesWithLine(OP_GREATER, OP_NOT, operator.line); break;
		default:
			return; // unreachable.
	}
}

static void unary(bool can_assign) {
	Token operator = parser.previous;

	parsePrecedence(PREC_UNARY);

	switch (operator.type) {
		case TOKEN_BANG: emitByteWithLine(OP_NOT, operator.line); break;
		case TOKEN_MINUS: emitByteWithLine(OP_NEGATE, operator.line); break;
		default:
			return; // unreachable.
	}
}

static void grouping(bool can_assign) {
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
	{ unary,    NULL,    PREC_NONE },       // TOKEN_BANG
	{ NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
	{ NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
	{ NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER
	{ NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
	{ NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS
	{ NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL
	{ variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
	{ string,   NULL,    PREC_NONE },       // TOKEN_STRING
	{ number,   NULL,    PREC_NONE },       // TOKEN_NUMBER
	{ NULL,     and_,    PREC_AND },        // TOKEN_AND
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
	{ literal,  NULL,    PREC_NONE },       // TOKEN_FALSE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_IF
	{ literal,  NULL,    PREC_NONE },       // TOKEN_NIL
	{ NULL,     or_,    PREC_OR },         // TOKEN_OR
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_THIS
	{ literal,  NULL,    PREC_NONE },       // TOKEN_TRUE
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
	Compiler compiler;
	initCompiler(&compiler);
	compiling_chunk = chunk;
	parser.had_error = false;
	parser.panic_mode = false;

	// put the parser in initial valid state.
	advance();
	while (!match(TOKEN_EOF)) {
		declaration();
	}

	endCompiler();
	return !parser.had_error;
}