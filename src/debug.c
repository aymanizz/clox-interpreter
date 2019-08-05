#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simpleOp(const char *name, int offset);
static int constantOp(const char *name, Chunk *chunk, int offset);
static int byteOp(const char *name, Chunk *chunk, int offset);
static int jumpOp(const char *name, Chunk *chunk, int offset);

void disassembleChunk(Chunk *chunk, const char *name) {
	printf("[==================] %s [===================]\n", name);

	for (int i = 0; i < chunk->size;) {
		i = disassembleOp(chunk, i);
	}
}

int disassembleOp(Chunk *chunk, int offset) {
	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("        ");
	} else {
		if (offset > 0) printf("\n");
		printf(" %04d > ", chunk->lines[offset]);
	}

	printf("%04d ", offset);

	uint8_t op = chunk->code[offset];
	switch (op) {
		case OP_CONSTANT:
			return constantOp("OP_CONSTANT", chunk, offset);
		case OP_NIL:
			return simpleOp("OP_NIL", offset);
		case OP_TRUE:
			return simpleOp("OP_TRUE", offset);
		case OP_FALSE:
			return simpleOp("OP_FALSE", offset);
		case OP_POP:
			return simpleOp("OP_POP", offset);
		case OP_GET_LOCAL:
			return byteOp("OP_GET_LOCAL", chunk, offset);
		case OP_SET_LOCAL:
			return byteOp("OP_SET_LOCAL", chunk, offset);
		case OP_DEF_GLOBAL:
			return constantOp("OP_DEF_GLOBAL", chunk, offset);
		case OP_GET_GLOBAL:
			return constantOp("OP_GET_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			return constantOp("OP_SET_GLOBAL", chunk, offset);
		case OP_NEGATE:
			return simpleOp("OP_NEGATE", offset);
		case OP_RETURN:
			return simpleOp("OP_RETURN", offset);
		case OP_ADD:
			return simpleOp("OP_ADD", offset);
		case OP_SUB:
			return simpleOp("OP_SUB", offset);
		case OP_MUL:
			return simpleOp("OP_MUL", offset);
		case OP_DIV:
			return simpleOp("OP_DIV", offset);
		case OP_NOT:
			return simpleOp("OP_NOT", offset);
		case OP_EQUAL:
			return simpleOp("OP_EQUAL", offset);
		case OP_GREATER:
			return simpleOp("OP_GREATER", offset);
		case OP_LESS:
			return simpleOp("OP_LESS", offset);
		case OP_JUMP:
			return jumpOp("OP_JUMP", chunk, offset);
		case OP_JUMP_IF_FALSE:
			return jumpOp("OP_JUMP_IF_FALSE", chunk, offset);
		default:
			printf("Unkown opcode %d\n", op);
			return offset + 1;
	}
}

static int byteOp(const char *name, Chunk *chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

static int constantOp(const char *name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%s %4d (", name, constant);
	printValue(chunk->constants.values[constant]);
	printf(")\n");
	return offset + 2;
}

static int simpleOp(const char *name, int offset) {
	printf("%-16s\n", name);
	return offset + 1;
}

static int jumpOp(const char *name, Chunk *chunk, int offset) {
	int jump = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
	printf("%-16s %4d\n", name, offset + 3 + jump);
	return offset + 3;
}