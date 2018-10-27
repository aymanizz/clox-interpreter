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

static InterpretResult run() {
	#define READ_BYTE() (*vm.ip++)
	#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	#define BINARY_OP(op) do { \
		Value b = pop(); \
		Value a = pop(); \
		push(a op b);    \
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
			case OP_NEGATE: push(-pop()); break;
			case OP_ADD:    BINARY_OP(+); break;
			case OP_SUB:    BINARY_OP(-); break;
			case OP_MUL:    BINARY_OP(*); break;
			case OP_DIV:    BINARY_OP(/); break;
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