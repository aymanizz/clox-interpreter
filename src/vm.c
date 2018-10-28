#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "memory.h"
#include "value.h"
#include "object.h"
#include "compiler.h"
#include "debug.h"

VM vm;

static void clearStack() {
	vm.sp = vm.stack;
}

void initVM() {
	clearStack();
	vm.objects = NULL;
}

void freeVM() {
	freeObjects();
}

static Value peek(int distance) {
	return vm.sp[-1 - distance];
}

static void runtimeError(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm.ip - vm.chunk->code;
	fprintf(stderr, "[line %d] in script\n", vm.chunk->lines[instruction]);

	clearStack();
}

static bool isFalsy(Value value) {
	return IS_NIL(value)
		|| (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	ObjString *b = AS_STRING(pop());
	ObjString *a = AS_STRING(pop());

	int length = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString *result = takeString(chars, length);
	push(OBJ_VAL(result));
}

static InterpretResult run() {
	#define READ_BYTE() (*vm.ip++)
	#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	#define BINARY_OP(value_type, op) do { \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtimeError("operands must be numbers"); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		\
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(value_type(a op b));    \
	} while (0)

	for (;;) {
		#ifdef CLOX_DEBUG_TRACE_EXECUTION
			printf("	");
			for (Value* slot = vm.stack; slot < vm.sp; ++slot) {
				printf("[ ");
				printValue(*slot);
				printf(" ]");
			}
			printf("\n");
			disassembleOp(vm.chunk, (int)(vm.ip - vm.chunk->code));
		#endif

		uint8_t op;
		switch (op = READ_BYTE()) {
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
			case OP_NIL: push(NIL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;
			case OP_EQUAL: {
				Value a = pop();
				Value b = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER:BINARY_OP(BOOL_VAL, >); break;
			case OP_LESS:   BINARY_OP(BOOL_VAL, <); break;
			case OP_ADD: {
				if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
					BINARY_OP(NUMBER_VAL, +); break;
				} else {
					runtimeError("operands must be two numbers or two strings");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUB:    BINARY_OP(NUMBER_VAL, -); break;
			case OP_MUL:    BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIV:    BINARY_OP(NUMBER_VAL, /); break;
			case OP_NOT: push(BOOL_VAL(isFalsy(pop()))); break;
			case OP_RETURN: {
				printValue(pop());
				printf("\n");
				return INTERPRET_OK;
			}
		}
	}

	#undef READ_BYTE
	#undef READ_CONSTANT
	#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
	Chunk chunk;
	initChunk(&chunk);

	if (!compile(source, &chunk)) {
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	InterpretResult result = run();

	freeChunk(&chunk);
	return result;
}

void push(Value value) {
	*vm.sp = value;
	++vm.sp;
}

Value pop() {
	--vm.sp;
	return *vm.sp;
}