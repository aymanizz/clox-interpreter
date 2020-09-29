#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
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
  uint8_t index;
  bool is_local;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;

  Upvalue upvalues[UINT8_COUNT];
  Local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
} Compiler;

// precedence from lowest to highest
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! - +
  PREC_CALL,        // . () []
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

Parser parser;
Compiler *current = NULL;

static void initCompiler(Compiler *c, FunctionType type) {
  c->enclosing = current;
  c->function = newFunction();
  c->type = type;
  c->local_count = 0;
  c->scope_depth = 0;
  current = c;

  // reserved for the current function object value being interpreted
  Local *local = &current->locals[current->local_count++];
  local->depth = 0;
  local->name.type = TOKEN_IDENTIFIER;
  local->name.start = "";
  local->name.length = 0;
  local->name.line = 0;
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

static void error(const char *message) { errorAt(&parser.previous, message); }

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

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static void synchronize() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      return;
    }

    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_LET:
      case TOKEN_LOOP:
      case TOKEN_IF:
      case TOKEN_RETURN:
        return;
      default:
        advance();
    }
  }
}

static Chunk *currentChunk() { return &current->function->chunk; }

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
  if (jump > UINT8_MAX) error("too much code to jump over");

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitLoop(int start) {
  emitByte(OP_LOOP);

  int jump = currentChunk()->size - start + 2;
  if (jump > UINT8_MAX) error("loop body too large");

  emitByte((jump >> 8) & 0xff);
  emitByte(jump & 0xff);
}

static void emitReturn() {
  emitByte(OP_NIL);
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

static ObjFunction *endCompiler() {
  Chunk *chunk = currentChunk();
  if (chunk->code[chunk->size - 1] != OP_RETURN) emitReturn();

  ObjFunction *function = current->function;

#ifdef CLOX_DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  current = current->enclosing;
  return function;
}

static void beginScope() { ++current->scope_depth; }

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
static void varDeclaration();

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

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void expressionStatement() {
  expression();
  emitByte(OP_POP);
  consume(TOKEN_SEMICOLON, "expected ';' after expression");
}

static void block() {
  beginScope();
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "expected '}' after block");
  endScope();
}

static void ifStatement() {
  expression();
  int then_jump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  consume(TOKEN_LEFT_BRACE, "expected a block after condition");
  block();

  int else_jump = match(TOKEN_ELSE) ? emitJump(OP_JUMP) : -1;
  patchJump(then_jump);
  emitByte(OP_POP);

  if (else_jump != -1) {
    if (match(TOKEN_IF)) {
      ifStatement();
    } else {
      consume(TOKEN_LEFT_BRACE, "expected a block after 'else'");
      block();
    }
    patchJump(else_jump);
  }
}

static void loopStatement() {
  // loop_statement := loop (
  //    <var declaration>? <condition expression>(';' <increment expression>)?
  //  )? { <body> }
  int start = currentChunk()->size;
  bool forever = check(TOKEN_LEFT_BRACE);

  bool has_declaration = !forever && match(TOKEN_LET);
  if (has_declaration) {
    beginScope();
    varDeclaration();
    start = currentChunk()->size;
  }

  int exit_jump = -1;
  if (!forever) {
    expression();
    exit_jump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  if (!forever && match(TOKEN_SEMICOLON)) {
    int jump = emitJump(OP_JUMP);
    int increment_start = currentChunk()->size;

    expression();
    emitByte(OP_POP);

    emitLoop(start);
    start = increment_start;
    patchJump(jump);
  }

  consume(TOKEN_LEFT_BRACE, "expected a '{' after clauses");
  block();
  emitLoop(start);

  if (exit_jump != -1) {
    patchJump(exit_jump);
    emitByte(OP_POP);
  }

  if (has_declaration) endScope();
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("cannot return from outside a function");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after expression");
    emitByte(OP_RETURN);
  }
}

static void statement() {
  if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_LOOP)) {
    loopStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    block();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
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
  if (current->scope_depth == 0) return;

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
  if (current->scope_depth > 0) return 0;
  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scope_depth == 0) return;
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

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  current->function->name =
      copyString(parser.previous.start, parser.previous.length);

  consume(TOKEN_LEFT_PAREN, "expected '(' after function name");
  beginScope();

  while (!check(TOKEN_RIGHT_PAREN)) {
    if (++current->function->arity >= 255) {
      errorAtCurrent("cannot have more than 255 parameters");
    }

    uint8_t param_const = parseVariable("expected a parameter name");
    defineVariable(param_const);
    if (!match(TOKEN_COMMA)) break;
  }

  consume(TOKEN_RIGHT_PAREN, "expected ')' after parameters");
  if (match(TOKEN_LEFT_BRACE)) {
    block();
  } else if (match(TOKEN_EQUAL)) {
    expression();
    consume(TOKEN_SEMICOLON, "expected ';' after expression");
    emitByte(OP_RETURN);
  } else {
    errorAtCurrent("expected '=' or '{' after parameters list");
  }

  ObjFunction *f = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(f)));

  for (int i = 0; i < f->upvalue_count; ++i) {
    emitByte(compiler.upvalues[i].is_local ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void funDeclaration() {
  uint8_t global = parseVariable("expected function name");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void declaration() {
  if (match(TOKEN_LET)) {
    varDeclaration();
  } else if (match(TOKEN_FUN)) {
    funDeclaration();
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
    case TOKEN_NIL:
      emitByte(OP_NIL);
      break;
    case TOKEN_TRUE:
      emitByte(OP_TRUE);
      break;
    case TOKEN_FALSE:
      emitByte(OP_FALSE);
      break;
    default:
      return;  // unreachable
  }
}

static void number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
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

static int addUpValue(Compiler *compiler, uint8_t index, bool is_local) {
  int count = compiler->function->upvalue_count;
  Upvalue *upvalues = compiler->upvalues;

  for (int i = 0; i < count; ++i) {
    Upvalue *upvalue = &upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) return i;
  }

  if (count == UINT8_COUNT) {
    error("too many closure variables in function");
    return 0;
  }

  upvalues[count].index = index;
  upvalues[count].is_local = is_local;
  return compiler->function->upvalue_count++;
}

static int resolveUpValue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    return addUpValue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpValue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpValue(compiler, (uint8_t)upvalue, false);
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
  } else if ((arg = resolveUpValue(current, name)) != -1) {
    get_op = OP_GET_UPVALUE;
    set_op = OP_SET_UPVALUE;
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
  Token operator= parser.previous;

  ParseRule *rule = getRule(operator.type);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operator.type) {
    case TOKEN_PLUS:
      emitByteWithLine(OP_ADD, operator.line);
      break;
    case TOKEN_MINUS:
      emitByteWithLine(OP_SUB, operator.line);
      break;
    case TOKEN_STAR:
      emitByteWithLine(OP_MUL, operator.line);
      break;
    case TOKEN_SLASH:
      emitByteWithLine(OP_DIV, operator.line);
      break;
    case TOKEN_BANG_EQUAL:
      emitBytesWithLine(OP_EQUAL, OP_NOT, operator.line);
      break;
    case TOKEN_EQUAL_EQUAL:
      emitByteWithLine(OP_EQUAL, operator.line);
      break;
    case TOKEN_GREATER:
      emitByteWithLine(OP_GREATER, operator.line);
      break;
    case TOKEN_GREATER_EQUAL:
      emitBytesWithLine(OP_LESS, OP_NOT, operator.line);
      break;
    case TOKEN_LESS:
      emitByteWithLine(OP_LESS, operator.line);
      break;
    case TOKEN_LESS_EQUAL:
      emitBytesWithLine(OP_GREATER, OP_NOT, operator.line);
      break;
    default:
      return;  // unreachable.
  }
}

static void unary(bool can_assign) {
  Token operator= parser.previous;

  parsePrecedence(PREC_UNARY);

  switch (operator.type) {
    case TOKEN_BANG:
      emitByteWithLine(OP_NOT, operator.line);
      break;
    case TOKEN_MINUS:
      emitByteWithLine(OP_NEGATE, operator.line);
      break;
    default:
      return;  // unreachable.
  }
}

static void grouping(bool can_assign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "expected ')' after expression");
}

static uint8_t argumentList() {
  uint8_t arg_count = 0;
  while (!check(TOKEN_RIGHT_PAREN)) {
    expression();
    if (++arg_count >= 255) error("cannot have more than 255 arguments");
    if (!match(TOKEN_COMMA)) break;
  }

  consume(TOKEN_RIGHT_PAREN, "expected ')' after argument list");
  return arg_count;
}

static void call(bool can_assign) {
  uint8_t arg_count = argumentList();
  emitBytes(OP_CALL, arg_count);
}

// has to be in the same order as TokenType enum
ParseRule rules[] = {
    {grouping, call, PREC_CALL},      // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},          // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},          // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},          // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},          // TOKEN_COMMA
    {NULL, NULL, PREC_CALL},          // TOKEN_DOT
    {unary, binary, PREC_TERM},       // TOKEN_MINUS
    {NULL, binary, PREC_TERM},        // TOKEN_PLUS
    {NULL, NULL, PREC_NONE},          // TOKEN_SEMICOLON
    {NULL, binary, PREC_FACTOR},      // TOKEN_SLASH
    {NULL, binary, PREC_FACTOR},      // TOKEN_STAR
    {unary, NULL, PREC_NONE},         // TOKEN_BANG
    {NULL, binary, PREC_EQUALITY},    // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},          // TOKEN_EQUAL
    {NULL, binary, PREC_EQUALITY},    // TOKEN_EQUAL_EQUAL
    {NULL, binary, PREC_COMPARISON},  // TOKEN_GREATER
    {NULL, binary, PREC_COMPARISON},  // TOKEN_GREATER_EQUAL
    {NULL, binary, PREC_COMPARISON},  // TOKEN_LESS
    {NULL, binary, PREC_COMPARISON},  // TOKEN_LESS_EQUAL
    {variable, NULL, PREC_NONE},      // TOKEN_IDENTIFIER
    {string, NULL, PREC_NONE},        // TOKEN_STRING
    {number, NULL, PREC_NONE},        // TOKEN_NUMBER
    {NULL, and_, PREC_AND},           // TOKEN_AND
    {NULL, NULL, PREC_NONE},          // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},          // TOKEN_ELSE
    {literal, NULL, PREC_NONE},       // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},          // TOKEN_FUN
    {NULL, NULL, PREC_NONE},          // TOKEN_IF
    {NULL, NULL, PREC_NONE},          // TOKEN_LET
    {NULL, NULL, PREC_NONE},          // TOKEN_LOOP
    {literal, NULL, PREC_NONE},       // TOKEN_NIL
    {NULL, or_, PREC_OR},             // TOKEN_OR
    {NULL, NULL, PREC_NONE},          // TOKEN_RETURN
    {NULL, NULL, PREC_NONE},          // TOKEN_SUPER
    {NULL, NULL, PREC_NONE},          // TOKEN_THIS
    {literal, NULL, PREC_NONE},       // TOKEN_TRUE
    {NULL, NULL, PREC_NONE},          // TOKEN_ERROR
    {NULL, NULL, PREC_NONE},          // TOKEN_EOF
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

ObjFunction *compile(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  parser.had_error = false;
  parser.panic_mode = false;

  // put the parser in initial valid state.
  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction *function = endCompiler();
  return parser.had_error ? NULL : function;
}