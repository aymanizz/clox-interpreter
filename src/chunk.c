#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk) {
	chunk->size = 0;
	chunk->capacity = 0;
	chunk->lines = NULL;
	chunk->code = NULL;
	initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
	if (chunk->capacity < chunk->size + 1) {
		int old_capacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(old_capacity);
		chunk->code = GROW_ARRAY(chunk->code, uint8_t,
			old_capacity, chunk->capacity);
		chunk->lines = GROW_ARRAY(chunk->lines, int,
			old_capacity, chunk->capacity);
	}

	chunk->code[chunk->size] = byte;
	chunk->lines[chunk->size] = line;
	++chunk->size;
}

void freeChunk(Chunk *chunk) {
	FREE_ARRAY(chunk->code, uint8_t, chunk->capacity);
	FREE_ARRAY(chunk->lines, int, chunk->capacity);
	initChunk(chunk);
}

int addConstant(Chunk *chunk, Value value) {
	writeValueArray(&chunk->constants, value);
	return chunk->constants.size - 1;
}