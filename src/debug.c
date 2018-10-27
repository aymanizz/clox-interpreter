#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simpleOp(const char *name, int offset);
static int constantOp(const char*name, Chunk *chunk, int offset);

void disassembleChunk(Chunk *chunk, const char *name) {
	printf("[==================] %s [===================]\n", name);

	for (int i = 0; i < chunk->size;) {
		i = disassembleOp(chunk, i);
	}
}

int disassembleOp(Chunk *chunk, int offset) {
	printf("%04d ", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	} else {
		printf("%04d ", chunk->lines[offset]);
	}

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
		default:
			printf("Unkown opcode %d\n", op);
			return offset + 1;
	}
}

static int constantOp(const char*name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%s %4d (", name, constant);
	printValue(chunk->constants.values[constant]);
	puts(")");
	return offset + 2;
}

static int simpleOp(const char *name, int offset) {
	printf("%-16s\n", name);
	return offset + 1;
}