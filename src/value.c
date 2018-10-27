#include <stdio.h>

#include "value.h"
#include "memory.h"

void initValueArray(ValueArray *array) {
	array->capacity = 0;
	array->size = 0;
	array->values = NULL;
}

void writeValueArray(ValueArray *array, Value value) {
	if (array->capacity < array->size + 1) {
		int old_capacity = array->capacity;
		array->capacity = GROW_CAPACITY(old_capacity);
		array->values = GROW_ARRAY(array->values, Value,
			old_capacity, array->capacity);
	}

	array->values[array->size++] = value;
}

void freeArray(ValueArray *array) {
	FREE_ARRAY(array->values, Value, array->capacity);
	initValueArray(array);
}

void printValue(Value value) {
	switch (value.type) {
		case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
		case VAL_NIL: printf("nil"); break;
		case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
	}
}