#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk *chunk, const char *name);
int disassembleOp(Chunk *chunk, int i);

#endif