#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, object_type) \
	(type*)allocateObj(sizeof(type), object_type)

static Obj *allocateObj(size_t size, ObjType type) {
	Obj *object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	return object;
}

ObjString *newString(const int length) {
	ObjString *string = (ObjString*)allocateObj(
		sizeof(*string) + length + 1, OBJ_STRING);
	string->length = length;

	return string;
}

uint32_t hashString(const char *key, const int length) {
	// FNV-1a hashing algorithm.
	uint32_t hash = 2166136261u;

	for (int i = 0; i < length; ++i) {
		hash ^= key[i];
		hash *= 16777619;
	}

	return hash;
}

ObjString *copyString(const char *chars, int length) {
	uint32_t hash = hashString(chars, length);
	ObjString *string = tableFindString(&vm.strings, chars, length, hash);
	if (string) return string;

	string = newString(length);
	memcpy(string->chars, chars, length);
	string->chars[length] = '\0';
	string->hash = hash;

	tableSet(&vm.strings, string, NIL_VAL);

	return string;
}

ObjString *stringConcat(ObjString *a, ObjString *b) {
	int length = a->length + b->length;

	ObjString *string = newString(length);
	memcpy(string->chars, a->chars, a->length);
	memcpy(string->chars + a->length, b->chars, b->length);
	string->chars[length] = '\0';

	uint32_t hash = hashString(string->chars, length);
	ObjString *internal = tableFindString(
		&vm.strings, string->chars, length, hash);
	if (internal) {
		FREE(string, ObjString);
		return internal;
	}

	string->length = length;
	string->hash = hash;
	tableSet(&vm.strings, string, NIL_VAL);

	return string;
}

void printObject(Value value) {
	switch(OBJ_TYPE(value)) {
		case OBJ_STRING:
			printf("%s", AS_CSTRING(value));
			break;
	}
}

bool objectsEqual(Value a, Value b) {
	if (OBJ_TYPE(a) != OBJ_TYPE(b))
		return false;

	switch(OBJ_TYPE(a)) {
		case OBJ_STRING: {
			// string interning guarantees string objects with the same
			// address are equal.
			return AS_OBJ(a) == AS_OBJ(b);
		}
	}

	return false; // unreachable.
}