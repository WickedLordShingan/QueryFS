#include "../Helpers/minunit.h"
#include "../Helpers/stb_ds.h"
#include "../Parser/parser.h"
#include "../Scanner/scanner.h"
#include "interpreter.h"
#include <stdio.h>
#include <stdlib.h>

const char **interpret(const char *input) {
  Scanner s = scanner_init(input);
  scan_tokens(&s);

  Parser p = parser_init(s.tokens);
  Query *result = parse(&p);

  if (result == NULL) {
    printf("reason for segfault\n");
  }
  const char **universe = NULL;
  for (int i = 1; i <= 4; i++) {
    char *file_name = malloc(5);
    sprintf(file_name, "%d", i);
    arrput(universe, file_name);
  }
  return evaluate(
      result, "/home/baala/interests/file_systems/semantic_fs/tests", universe);
}

static void test_single_file(void) {
  const char **result = interpret("\"1.txt\"");
  mu_assert("only a single file", arrlen(result) == 1);
  mu_assert(
      "file matches",
      strcmp(result[0],
             "/home/baala/interests/file_systems/semantic_fs/tests/1.txt") ==
          0);
}

static void regex_on_name(void) {
  const char **result = interpret("regex(^[0-9]$)");
  mu_assert("all the files", arrlen(result) == 4);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/1") ==
                0);
  mu_assert("file matches",
            strcmp(result[1],
                   "/home/baala/interests/file_systems/semantic_fs/tests/2") ==
                0);
}

static void size_test_superlative_single(void) {
  const char **result = interpret("smallest");
  mu_assert("only a single file", arrlen(result) == 1);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/3") ==
                0);
}

static void size_test_superlative(void) {
  const char **result = interpret("smallest 3");
  mu_assert("three files", arrlen(result) == 3);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/3") ==
                0);
  mu_assert("file matches",
            strcmp(result[1],
                   "/home/baala/interests/file_systems/semantic_fs/tests/4") ==
                0);
  mu_assert("file matches",
            strcmp(result[2],
                   "/home/baala/interests/file_systems/semantic_fs/tests/2") ==
                0);
}

static void size_test_group(void) {
  const char **result = interpret("smaller_than \"1\"");
  mu_assert("3 files", arrlen(result) == 3);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/3") ==
                0);
  mu_assert("file matches",
            strcmp(result[1],
                   "/home/baala/interests/file_systems/semantic_fs/tests/4") ==
                0);

  const char **result_too = interpret("bigger_than \"2\"");
  mu_assert("1 file", arrlen(result_too) == 1);
  mu_assert("file matches",
            strcmp(result_too[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/1") ==
                0);
}

static void size_test_group_with_numbers(void) {
  const char **result = interpret("smaller_than 100");
  mu_assert("3 files", arrlen(result) == 2);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/3") ==
                0);
  mu_assert("file matches",
            strcmp(result[1],
                   "/home/baala/interests/file_systems/semantic_fs/tests/4") ==
                0);
}

static void regex_contains(void) {
  const char **result = interpret("contains regex(baalabaala)");
  mu_assert("all the files", arrlen(result) == 1);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/2") ==
                0);
}

static void and_test_nonempty(void) {
  // smaller_than "1" -> {3,4,2}; smallest 2 -> {3,4}
  // intersection -> {3,4}
  const char **result = interpret("smaller_than \"1\" and smallest 3");
  mu_assert("2 files", arrlen(result) == 3);
}

static void or_test_nonempty(void) {
  // smaller_than "1" -> {3,4,2}; biggest 1 -> {1}
  // union -> {1,2,3,4}
  const char **result = interpret("smaller_than \"1\" or biggest 1");
  mu_assert("4 files", arrlen(result) == 4);
}

static void combined_and_not_test(void) {
  // not (smaller_than "2") and contains regex(baalabaala)
  // not smaller_than "2" -> {1, 2}; contains(...) -> {2}; intersection -> {2}
  const char **result =
      interpret("not smaller_than \"2\" and contains regex(baalabaala)");
  mu_assert("1 file", arrlen(result) == 1);
  mu_assert("file matches",
            strcmp(result[0],
                   "/home/baala/interests/file_systems/semantic_fs/tests/2") ==
                0);
}

int main() {
  mu_run_test(test_single_file);
  mu_run_test(regex_on_name);
  mu_run_test(size_test_superlative_single);
  mu_run_test(size_test_superlative);
  mu_run_test(size_test_group);
  mu_run_test(size_test_group_with_numbers);
  mu_run_test(regex_contains);
  mu_run_test(and_test_nonempty);
  mu_run_test(or_test_nonempty);
  mu_run_test(combined_and_not_test);
}
