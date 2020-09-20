#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VM vm;

static void clearStack() {
  vm.sp = vm.stack;
  vm.frame_count = 0;
}

static void initNativeFunctions();

void initVM() {
  clearStack();
  vm.objects = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);
  initNativeFunctions();
}

void freeVM() {
  freeObjects();
  freeTable(&vm.globals);
  freeTable(&vm.strings);
}

static Value peek(int distance) { return vm.sp[-1 - distance]; }

static void runtimeError(const char *format, ...) {
  for (int i = 0; i < vm.frame_count; ++i) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->function;

    uint8_t offset = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "line %d in ", function->chunk.lines[offset]);
    fprintf(stderr, "%s\n",
            function->name != NULL ? function->name->chars : "script");
  }

  va_list args;
  va_start(args, format);
  fputs("error: ", stderr);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  clearStack();
}

static bool isFalsy(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool call(ObjFunction *function, uint8_t arg_count) {
  if (function->arity != arg_count) {
    runtimeError("expected %i arguments, got %i", function->arity, arg_count);
    return false;
  } else if (vm.frame_count == CLOX_FRAMES_MAX) {
    runtimeError("stack overflow");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frame_count++];
  frame->function = function;
  frame->ip = frame->function->chunk.code;
  frame->slots = vm.sp - arg_count - 1;
  return true;
}

static bool callNative(ObjNativeFn *nativeFn, uint8_t arg_count) {
  Value *args = vm.sp - arg_count;
  Value result = nativeFn->function(arg_count, args);
  vm.sp = args - 1;
  push(result);
  return true;
}

static bool callValue(Value callee, uint8_t arg_count) {
  if (IS_OBJ(callee)) {
    switch (AS_OBJ(callee)->type) {
      case OBJ_FUNCTION: {
        return call(AS_FUNCTION(callee), arg_count);
      }
      case OBJ_NATIVE_FN: {
        return callNative(AS_NATIVE_FN(callee), arg_count);
      }
      default:
        break;
    }
  }

  runtimeError("can only call callable values");
  return false;
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  ((uint16_t)(frame->ip += 2, (frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type, op)                     \
  do {                                                \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtimeError("operands must be numbers");       \
      return INTERPRET_RUNTIME_ERROR;                 \
    }                                                 \
                                                      \
    double b = AS_NUMBER(pop());                      \
    double a = AS_NUMBER(pop());                      \
    push(value_type(a op b));                         \
  } while (0)

  for (;;) {
#ifdef CLOX_DEBUG_TRACE_EXECUTION
    if (vm.stack != vm.sp) {
      printf("        ");
      for (Value *slot = vm.stack; slot < vm.sp; ++slot) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
      }
      printf("\n");
    }
    disassembleOp(&frame->function->chunk,
                  (int)(frame->ip - frame->function->chunk.code));
#endif

    OpCode op = READ_BYTE();
    switch (op) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NEGATE: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("operand must be a number");
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      }
      case OP_NIL:
        push(NIL_VAL);
        break;
      case OP_TRUE:
        push(BOOL_VAL(true));
        break;
      case OP_FALSE:
        push(BOOL_VAL(false));
        break;
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_DEF_GLOBAL: {
        ObjString *name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          runtimeError("undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL: {
        Value a = pop();
        Value b = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OP_LESS:
        BINARY_OP(BOOL_VAL, <);
        break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          ObjString *b = AS_STRING(pop());
          ObjString *a = AS_STRING(pop());
          push(OBJ_VAL(stringConcat(a, b)));
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          BINARY_OP(NUMBER_VAL, +);
        } else {
          runtimeError("operands must be two numbers or two strings");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUB:
        BINARY_OP(NUMBER_VAL, -);
        break;
      case OP_MUL:
        BINARY_OP(NUMBER_VAL, *);
        break;
      case OP_DIV:
        BINARY_OP(NUMBER_VAL, /);
        break;
      case OP_NOT:
        push(BOOL_VAL(isFalsy(pop())));
        break;
      case OP_JUMP: {
        int offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsy(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        uint8_t arg_count = READ_BYTE();
        if (!callValue(peek(arg_count), arg_count))
          return INTERPRET_RUNTIME_ERROR;

        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_RETURN: {
        Value ret_value = pop();

        --vm.frame_count;
        if (vm.frame_count == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.sp = frame->slots;
        push(ret_value);
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

static void defineNativeFn(const char *name, NativeFn fn) {
  push(OBJ_VAL(copyString(name, strlen(name))));
  push(OBJ_VAL(newNativeFn(fn)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

static Value nativeClock(int arg_count, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value nativePrintln(int arg_count, Value *args) {
  for (int i = 0; i < arg_count; ++i) {
    Value value = args[i];
    switch (value.type) {
      case VAL_BOOL:
        printf("%s", AS_BOOL(value) ? "true" : "false");
        break;
      case VAL_NIL:
        printf("nil");
        break;
      case VAL_NUMBER:
        printf("%g", AS_NUMBER(value));
        break;
      case VAL_OBJ:
        printObject(value);
    }

    if (i != arg_count - 1) {
      putchar(' ');
    }
  }
  putchar('\n');

  return NIL_VAL;
}

static void initNativeFunctions() {
  defineNativeFn("clock", nativeClock);
  defineNativeFn("println", nativePrintln);
}

InterpretResult interpret(const char *source) {
  ObjFunction *function = compile(source);
  if (!function) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}

void push(Value value) {
  *vm.sp = value;
  ++vm.sp;
}

Value pop() {
  --vm.sp;
  return *vm.sp;
}