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
		case OP_RETURN:
			return simpleOp("OP_RETURN", offset);
		default:
			printf("Unkown opcode %d\n", op);
			return offset + 1;
	}
}

static int constantOp(const char*name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%+16s %4d (", name, constant);
	printValue(chunk->constants.values[constant]);
	puts(")");
	return offset + 2;
}

static int simpleOp(const char *name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}