#ifndef CLOX_COMMON_H
#define CLOX_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef DEBUG
	// #define CLOX_DEBUG_PRINT_CODE
	#define CLOX_DEBUG_TRACE_EXECUTION
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif