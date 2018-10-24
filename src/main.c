#include "common.h"
#include "vm.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
	initVM();

	Chunk chunk;
	initChunk(&chunk);

	int constant;

	constant = addConstant(&chunk, 1.2);
	writeChunk(&chunk, OP_CONSTANT, 1);
	writeChunk(&chunk, constant, 1);

	constant = addConstant(&chunk, 3.4);
	writeChunk(&chunk, OP_CONSTANT, 1);
	writeChunk(&chunk, constant, 1);

	writeChunk(&chunk, OP_ADD, 1);

	constant = addConstant(&chunk, 5.6);
	writeChunk(&chunk, OP_CONSTANT, 1);
	writeChunk(&chunk, constant, 1);

	writeChunk(&chunk, OP_DIV, 1);

	writeChunk(&chunk, OP_NEGATE, 1);

	writeChunk(&chunk, OP_RETURN, 1);

	interpret(&chunk);

	disassembleChunk(&chunk, "test");
	
	freeVM();
	freeChunk(&chunk);
	return 0;
}