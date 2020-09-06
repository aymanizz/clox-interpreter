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