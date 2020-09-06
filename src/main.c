#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

static void repl();
static void runFile(const char *path);

int main(int argc, const char *argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    printf("%s: usage: %s [path]\n", argv[0], argv[0]);
  }

  freeVM();
}

static void repl() {
  char line[1024];
  for (;;) {
    printf(">>> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "error: could not open file \"%s\".\n", path);
    exit(10);
  }

  fseek(file, 0L, SEEK_END);
  size_t filesize = ftell(file);
  rewind(file);

  char *source = malloc(filesize + 1);
  if (!source) {
    fprintf(stderr, "error: not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  const size_t objects_num = filesize / sizeof(*source);
  size_t bytes_read = fread(source, sizeof(*source), objects_num, file);
  if (bytes_read < objects_num) {
    fprintf(stderr, "error: could not read file \"%s\".\n", path);
    exit(74);
  }

  source[bytes_read] = '\0';

  fclose(file);

  return source;
}

static void runFile(const char *path) {
  char *source = readFile(path);

  InterpretResult result = interpret(source);

  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}