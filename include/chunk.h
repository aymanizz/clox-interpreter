#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_DEF_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_NOT,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_NEGATE,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CLOSURE,
  OP_CALL,
  OP_RETURN,
} OpCode;

typedef struct {
  int size;
  int capacity;
  int *lines;
  uint8_t *code;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
void freeChunk(Chunk *chunk);
int addConstant(Chunk *chunk, Value value);

#endif