#ifndef MINUNIT_H
#define MINUNIT_H
#include <stdio.h>

static int tests_run = 0;
static int tests_failed = 0;

#define mu_assert(message, test)                                               \
  do {                                                                         \
    tests_run++;                                                               \
    if (!(test)) {                                                             \
      tests_failed++;                                                          \
      printf("  FAIL: %s (%s:%d)\n", message, __FILE__, __LINE__);             \
    }                                                                          \
  } while (0)

#define mu_run_test(test_fn)                                                   \
  do {                                                                         \
    printf("Running %s...\n", #test_fn);                                       \
    test_fn();                                                                 \
  } while (0)

#endif
