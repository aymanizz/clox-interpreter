#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
  (type *)allocateObj(sizeof(type), object_type)

static Obj *allocateObj(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  return object;
}

ObjString *newString(const int length) {
  ObjString *string =
      (ObjString *)allocateObj(sizeof(*string) + length + 1, OBJ_STRING);
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
  ObjString *internal =
      tableFindString(&vm.strings, string->chars, length, hash);
  if (internal) {
    FREE(string, ObjString);
    return internal;
  }

  string->length = length;
  string->hash = hash;
  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  initChunk(&function->chunk);
  function->arity = 0;
  function->upvalue_count = 0;
  function->name = NULL;
  return function;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  return closure;
}

ObjNativeFn *newNativeFn(NativeFn function) {
  ObjNativeFn *nativeFn = ALLOCATE_OBJ(ObjNativeFn, OBJ_NATIVE_FN);
  nativeFn->function = function;
  return nativeFn;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  return upvalue;
}

void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_NATIVE_FN:
      printf("<native fn>");
    case OBJ_UPVALUE:
      printf("upvalue");
  }
}

bool objectsEqual(Value a, Value b) {
  if (OBJ_TYPE(a) != OBJ_TYPE(b)) return false;

  switch (OBJ_TYPE(a)) {
    case OBJ_STRING: {
      // string interning guarantees string objects with the same
      // address are equal.
      return AS_OBJ(a) == AS_OBJ(b);
    }
    default:
      return false;
  }
}