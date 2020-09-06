#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
} ObjType;

struct sObj {
  ObjType type;
  struct sObj *next;
};

struct sObjString {
  Obj obj;
  int length;
  uint32_t hash;
  char chars[];
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

ObjString *newString(const int length);
ObjString *copyString(const char *chars, int length);
uint32_t hashString(const char *key, const int length);
ObjString *stringConcat(ObjString *a, ObjString *b);
void printObject(Value value);
bool objectsEqual(Value a, Value b);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif