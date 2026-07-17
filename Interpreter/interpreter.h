#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../Parser/ast.h"
#include "../Parser/parser.h"
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#define PATH_MAX 512

const char **evaluate(Query *self, const char *pwd, const char **universe);

typedef struct {
  const char *path;
  time_t atime;
} FileStat;

typedef struct {
  const char *path;
  off_t size;
} AlsoFileStat;

#endif
