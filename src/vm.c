#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "value.h"
#include "compiler.h"
#include "debug.h"

VM vm;

static void clearStack() {
	vm.sp = vm.stack;
}

void initVM() {
	clearStack();
}

void freeVM() {

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
			case OP_ADD:    BINARY_OP(NUMBER_VAL, +); break;
			case OP_SUB:    BINARY_OP(NUMBER_VAL, -); break;
			case OP_MUL:    BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIV:    BINARY_OP(NUMBER_VAL, /); break;
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