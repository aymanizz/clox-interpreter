#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"

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

ObjString *copyString(const char *chars, const int length) {
	ObjString *string = newString(length);
	memcpy(string->chars, chars, length);
	string->chars[length] = '\0';
	string->hash = hashString(chars, length);

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
			ObjString *a_string = AS_STRING(a);
			ObjString *b_string = AS_STRING(b);
			return a_string->length == b_string->length
				&& !strcmp(a_string->chars, b_string->chars);
		}
	}

	return false; // unreachable.
}