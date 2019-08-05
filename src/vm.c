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
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM() {
	freeObjects();
	freeTable(&vm.globals);
	freeTable(&vm.strings);
}

static Value peek(int distance) {
	return vm.sp[-1 - distance];
}

static void runtimeError(const char *format, ...) {
	size_t instruction = vm.ip - vm.chunk->code;
	fprintf(stderr, "error: line %d, in script: ", vm.chunk->lines[instruction]);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fputs("\n", stderr);
	va_end(args);

	clearStack();
}

static bool isFalsy(Value value) {
	return IS_NIL(value)
		|| (IS_BOOL(value) && !AS_BOOL(value));
}

static InterpretResult run() {
	#define READ_BYTE() (*vm.ip++)
	#define READ_SHORT() ((uint16_t)(vm.ip += 2, (vm.ip[-2] << 8) | vm.ip[-1]))
	#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	#define READ_STRING() AS_STRING(READ_CONSTANT())
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
			if (vm.stack != vm.sp) {
				printf("        ");
				for (Value* slot = vm.stack; slot < vm.sp; ++slot) {
					printf("[ ");
					printValue(*slot);
					printf(" ]");
				}
				printf("\n");
			}
			disassembleOp(vm.chunk, (int)(vm.ip - vm.chunk->code));
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
			case OP_NIL: push(NIL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;
			case OP_POP: pop(); break;
			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(vm.stack[slot]);
				break;
			}
			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				vm.stack[slot] = peek(0);
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
			case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
			case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
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
			case OP_SUB: BINARY_OP(NUMBER_VAL, -); break;
			case OP_MUL: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIV: BINARY_OP(NUMBER_VAL, /); break;
			case OP_NOT: push(BOOL_VAL(isFalsy(pop()))); break;
			case OP_JUMP: {
				int offset = READ_SHORT();
				vm.ip += offset;
				break;
			}
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsy(peek(0)))
					vm.ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				vm.ip -= offset;
				break;
			}
			case OP_RETURN: {
				while (vm.sp != vm.stack) {
					printValue(pop());
					printf("\n");
				}
				return INTERPRET_OK;
			}
		}
	}

	#undef READ_BYTE
	#undef READ_SHORT
	#undef READ_CONSTANT
	#undef READ_STRING
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