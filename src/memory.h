#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, old_size, size) \
	(type*) reallocate((previous), sizeof(type) * (old_size), \
		sizeof(type) * (size))

#define FREE_ARRAY(pointer, type, size) \
	reallocate(pointer, sizeof(type) * (size), 0)

void* reallocate(void *previous, size_t old_size, size_t new_size);

#endif