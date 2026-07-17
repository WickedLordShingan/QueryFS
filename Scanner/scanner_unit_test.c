#include "../Helpers/minunit.h"
#include "../Helpers/stb_ds.h"
#include "scanner.h"
#include "token.h"
#include <stdio.h>
#include <string.h>

bool is_a_num(char current);
bool is_path_char(char c);
// ---- Level 1: smallest possible units, pure functions, no Scanner needed ----

static void test_is_a_num(void) {
  mu_assert("digit '5' should be a num", is_a_num('5'));
  mu_assert("'0' should be a num", is_a_num('0'));
  mu_assert("letter 'a' should NOT be a num", !is_a_num('a'));
  mu_assert("space should NOT be a num", !is_a_num(' '));
}

static void test_is_path_char(void) {
  mu_assert("letter is a path char", is_path_char('a'));
  mu_assert("digit is a path char", is_path_char('5'));
  mu_assert("dot is a path char", is_path_char('.'));
  mu_assert("| is a path char", is_path_char('|'));
  mu_assert("slash is not a path char", !is_path_char('/'));
  mu_assert("underscore is a path char", is_path_char('_'));
  mu_assert("open paren is a path char (for regex()", is_path_char('('));
  mu_assert("space is NOT a path char", !is_path_char(' '));
  mu_assert("comma is NOT a path char", !is_path_char(','));
  mu_assert("null byte is NOT a path char", !is_path_char('\0'));
}

// ---- Level 2: single-token behavior via scan_tokens on tiny inputs ----
// Helper: scan a string, return the array of tokens (caller must not free
// individually here -- test process exits anyway, so we don't bother
// cleaning up in these tests. In real code you WOULD free.)

static Token *tokens_from(const char *src) {
  Scanner s = scanner_init(src);
  scan_tokens(&s);
  return s.tokens;
}

static void test_single_number(void) {
  Token *toks = tokens_from("42");
  mu_assert("expect 2 tokens (NUMBER + EOF)", arrlen(toks) == 2);
  mu_assert("token 0 is NUMBER", toks[0].token_type == NUMBER);
  mu_assert("token 0 value is 42", toks[0].number_value == 42);
  mu_assert("last token is EOF",
            toks[arrlen(toks) - 1].token_type == TOKEN_EOF);
}

static void test_single_path(void) {
  Token *toks = tokens_from("\"1\"");
  mu_assert("expect 2 tokens (PATH + EOF)", arrlen(toks) == 2);
  mu_assert("token 0 is PATH", toks[0].token_type == PATH);
  mu_assert("lexeme matches", strcmp(toks[0].lexeme, "1") == 0);
}

static void test_keyword_recognized(void) {
  Token *toks = tokens_from("newer_than");
  mu_assert("expect 2 tokens", arrlen(toks) == 2);
  mu_assert("recognized as NEWER_THAN keyword",
            toks[0].token_type == NEWER_THAN);
  mu_assert("keyword tokens carry no lexeme", toks[0].lexeme == NULL);
}

static void test_comma_separated_paths(void) {
  Token *toks = tokens_from("\"a.txt\",\"b.txt\"");
  mu_assert("expect 4 tokens (PATH, COMMA, PATH, EOF)", arrlen(toks) == 4);
  mu_assert("token 0 PATH", toks[0].token_type == PATH);
  mu_assert("token 1 COMMA", toks[1].token_type == COMMA);
  mu_assert("token 2 PATH", toks[2].token_type == PATH);
}

static void test_regex_body_extracted(void) {
  Token *toks = tokens_from("regex(a.*b)");
  mu_assert("expect 2 tokens", arrlen(toks) == 2);
  mu_assert("token 0 is REGEX", toks[0].token_type == REGEX);
  mu_assert("regex body correctly extracted (no 'regex(' prefix, no ')')",
            strcmp(toks[0].lexeme, "a.*b") == 0);
}

static void test_multiple_spaces_between_tokens(void) {
  // this is the whitespace bug we fixed -- guard against regression
  Token *toks = tokens_from("\"foo.txt\"     \"bar.txt\"");
  mu_assert("expect 3 tokens despite multiple spaces", arrlen(toks) == 3);
  mu_assert("token 0 PATH foo.txt", strcmp(toks[0].lexeme, "foo.txt") == 0);
  mu_assert("token 1 PATH bar.txt", strcmp(toks[1].lexeme, "bar.txt") == 0);
}

static void test_leading_and_trailing_whitespace(void) {
  Token *toks = tokens_from("   42   ");
  mu_assert("expect 2 tokens", arrlen(toks) == 2);
  mu_assert("token 0 is NUMBER 42", toks[0].number_value == 42);
}

static void test_empty_input(void) {
  Token *toks = tokens_from("");
  mu_assert("empty input should still produce just EOF", arrlen(toks) == 1);
  mu_assert("that one token is EOF", toks[0].token_type == TOKEN_EOF);
}

static void test_whitespace_only_input(void) {
  Token *toks = tokens_from("    ");
  mu_assert("whitespace-only input should produce just EOF", arrlen(toks) == 1);
}

// ---- Level 3: a realistic full query, end to end ----

static void test_full_query(void) {
  Token *toks =
      tokens_from("\"foo.txt\", \"bar.txt\" newer_than regex(a.*b) 42");
  TokenType expected[] = {PATH,  COMMA,  PATH,     NEWER_THAN,
                          REGEX, NUMBER, TOKEN_EOF};
  int n = sizeof(expected) / sizeof(expected[0]);
  mu_assert("token count matches expected sequence", arrlen(toks) == n);
  for (int i = 0; i < n && i < arrlen(toks); i++) {
    mu_assert("token type matches expected sequence at this position",
              toks[i].token_type == expected[i]);
  }
}

int main(void) {
  mu_run_test(test_is_a_num);
  mu_run_test(test_is_path_char);
  mu_run_test(test_single_number);
  mu_run_test(test_single_path);
  mu_run_test(test_keyword_recognized);
  mu_run_test(test_comma_separated_paths);
  mu_run_test(test_regex_body_extracted);
  mu_run_test(test_multiple_spaces_between_tokens);
  mu_run_test(test_leading_and_trailing_whitespace);
  mu_run_test(test_empty_input);
  mu_run_test(test_whitespace_only_input);
  mu_run_test(test_full_query);

  printf("\n%d tests run, %d failed\n", tests_run, tests_failed);
  return tests_failed != 0;
}
