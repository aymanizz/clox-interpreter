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

static ObjString *allocateStringObj(char *chars, int length) {
	ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;

	return string;
}

ObjString *copyString(const char *chars, int length) {
	char *string = ALLOCATE(char, length + 1);
	memcpy(string, chars, length);
	string[length] = '\0';
	
	return allocateStringObj(string, length);
}

ObjString *takeString(char *chars, int length) {
	return allocateStringObj(chars, length);
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