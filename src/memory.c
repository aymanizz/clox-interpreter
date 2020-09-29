#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void *reallocate(void *previous, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    free(previous);
    return NULL;
  }

  return realloc(previous, new_size);
}

static void freeObject(Obj *object) {
  switch (object->type) {
    case OBJ_STRING: {
      ObjString *string = (ObjString *)object;
      FREE(string, ObjString);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction *function = (ObjFunction *)object;
      freeChunk(&function->chunk);
      FREE(function, ObjFunction);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure *closure = (ObjClosure *)object;
      FREE(closure, ObjClosure);
      break;
    }
    case OBJ_NATIVE_FN: {
      FREE(object, ObjNativeFn);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(object, ObjUpvalue);
      break;
    }
  }
}

void freeObjects() {
  Obj *object = vm.objects;
  while (object) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}