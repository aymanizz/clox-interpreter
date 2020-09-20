#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_NATIVE_FN,
} ObjType;

typedef struct Obj {
  ObjType type;
  struct Obj *next;
} Obj;

typedef struct {
  Obj obj;
  int length;
  uint32_t hash;
  char chars[];
} ObjString;

typedef struct {
  Obj obj;
  int arity;
  Chunk chunk;
  ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int args_count, Value *args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNativeFn;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE_FN(value) isObjType(value, OBJ_NATIVE_FN)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE_FN(value) ((ObjNativeFn *)AS_OBJ(value))

ObjString *newString(const int length);
ObjString *copyString(const char *chars, int length);
uint32_t hashString(const char *key, const int length);
ObjString *stringConcat(ObjString *a, ObjString *b);

ObjFunction *newFunction();
ObjNativeFn *newNativeFn(NativeFn function);

void printObject(Value value);
bool objectsEqual(Value a, Value b);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif