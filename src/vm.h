#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

#define CLOX_VM_STACK_MAX 256

typedef struct {
	Chunk *chunk;
	uint8_t *ip;
	Value stack[CLOX_VM_STACK_MAX];
	Value *sp;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

#endif