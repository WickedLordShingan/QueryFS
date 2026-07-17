#include "../Helpers/minunit.h"
#include "../Helpers/stb_ds.h"
#include "../Scanner/scanner.h"
#include "ast.h"
#include "parser.h"
#include <stdio.h>

Query *parse_string(const char *input) {
  Scanner s = scanner_init(input);
  scan_tokens(&s);

  Parser p = parser_init(s.tokens);
  return parse(&p);
}

Token *just_tokens(const char *input) {
  Scanner s = scanner_init(input);
  scan_tokens(&s);
  return s.tokens;
}

static void test_single_path(void) {
  Query *q = parse_string("\"foo.txt\"");

  mu_assert("top level is a plain group (no and/or)", q->kind == QUERY_GROUP);
  mu_assert("not negated", q->as.leaf.negated == false);

  Group *g = q->as.leaf.group;
  mu_assert("group is a name group", g->kind == GROUP_NAME);
  mu_assert("name group is a path list", g->as.name->kind == NAME_PATHS);
  mu_assert("exactly one path", arrlen(g->as.name->as.path_list.paths) == 1);
  mu_assert("path text matches",
            strcmp(g->as.name->as.path_list.paths[0], "foo.txt") == 0);
}

static void test_comma_separated_paths(void) {
  Query *q = parse_string("\"a.txt\",\"b.txt\",\"c.txt\"");
  Group *g = q->as.leaf.group;

  mu_assert("three paths collected",
            arrlen(g->as.name->as.path_list.paths) == 3);
  mu_assert("first path",
            strcmp(g->as.name->as.path_list.paths[0], "a.txt") == 0);
  mu_assert("second path",
            strcmp(g->as.name->as.path_list.paths[1], "b.txt") == 0);
  mu_assert("third path",
            strcmp(g->as.name->as.path_list.paths[2], "c.txt") == 0);
}

static void test_name_regex(void) {
  Query *q = parse_string("regex(a.*b)");
  Group *g = q->as.leaf.group;

  mu_assert("name group is a regex", g->as.name->kind == NAME_REGEX);
  mu_assert("regex pattern extracted correctly",
            strcmp(g->as.name->as.regex_pattern, "a.*b") == 0);
}

// ==================================================================
// Level 2: characteristic groups (time / size / content)
// ==================================================================

static void test_newest_with_no_number(void) {
  Query *q = parse_string("newest");
  CharacteristicGroup *cg = q->as.leaf.group->as.characteristic;

  mu_assert("is a time group", cg->kind == CHAR_TIME);
  mu_assert("is the newer/newest side", cg->as.time->kind == TIME_NEWER);
  mu_assert("is the superlative form",
            cg->as.time->compare.kind == TIME_CMP_SUPERLATIVE);
  mu_assert("no count given", !cg->as.time->compare.as.superlative.has_count);
}

static void test_newest_with_number(void) {
  Query *q = parse_string("newest 5");
  TimeCompare cmp = q->as.leaf.group->as.characteristic->as.time->compare;

  mu_assert("count was given", cmp.as.superlative.has_count);
  mu_assert("count value is 5", cmp.as.superlative.count == 5);
}

static void test_bigger_than_number(void) {
  Query *q = parse_string("bigger_than 1024");
  SizeGroup *sg = q->as.leaf.group->as.characteristic->as.size;

  mu_assert("bigger side", sg->kind == SIZE_BIGGER);
  mu_assert("compared against a raw number",
            sg->compare.kind == SIZE_CMP_THAN_NUMBER);
  mu_assert("byte count is 1024", sg->compare.as.byte_count == 1024);
}

static void test_bigger_than_group(void) {
  Query *q = parse_string("bigger_than \"foo.txt\"");
  SizeGroup *sg = q->as.leaf.group->as.characteristic->as.size;

  mu_assert("compared against a reference group",
            sg->compare.kind == SIZE_CMP_THAN_GROUP);
  mu_assert("reference group is a name group",
            sg->compare.as.reference_group->kind == GROUP_NAME);
}

static void test_contains_regex(void) {
  Query *q = parse_string("contains regex(TODO)");
  ContentGroup *cg = q->as.leaf.group->as.characteristic->as.content;

  mu_assert("is a contains group", cg->kind == CONTENT_CONTAINS);
  mu_assert("pattern extracted", strcmp(cg->regex_pattern, "TODO") == 0);
}

// ==================================================================
// Level 2: "not" negation
// ==================================================================

static void test_not_negates_the_leaf(void) {
  Query *q = parse_string("not \"foo.txt\"");

  mu_assert("still just one leaf (not doesn't wrap in and/or)",
            q->kind == QUERY_GROUP);
  mu_assert("negated flag is set", q->as.leaf.negated == true);
  q = parse_string("not smaller_than \"foo.txt\"");

  mu_assert("still just one leaf (not doesn't wrap in and/or)",
            q->kind == QUERY_GROUP);
  mu_assert("negated flag is set", q->as.leaf.negated == true);
  mu_assert("negated flag is set",
            q->as.leaf.group->kind == GROUP_CHARACTERISTIC);
  mu_assert("negated flag is set",
            q->as.leaf.group->as.characteristic->kind == CHAR_SIZE);
  mu_assert("negated flag is set",
            q->as.leaf.group->as.characteristic->as.size->kind == SIZE_SMALLER);
  mu_assert("negated flag is set",
            q->as.leaf.group->as.characteristic->as.size->compare.as
                    .reference_group->kind == GROUP_NAME);
  mu_assert("negated flag is set",
            strcmp("foo.txt",
                   q->as.leaf.group->as.characteristic->as.size->compare.as
                       .reference_group->as.name->as.path_list.paths[0]) == 0);
}

// ==================================================================
// Level 2: and / or combination + precedence
// ==================================================================

static void test_simple_and(void) {
  Query *q = parse_string("\"a.txt\" and \"b.txt\"");

  mu_assert("top level is AND", q->kind == QUERY_AND);
  mu_assert("left side is a leaf", q->as.binop.left->kind == QUERY_GROUP);
  mu_assert("right side is a leaf", q->as.binop.right->kind == QUERY_GROUP);
}

static void test_simple_or(void) {
  Query *q = parse_string("\"a.txt\" or \"b.txt\"");
  mu_assert("top level is OR", q->kind == QUERY_OR);
}

static void test_and_binds_tighter_than_or(void) {
  // "a and b or not c" should parse as (a and b) or (not c),
  // i.e. the TOP node is OR, whose left child is the AND subtree.
  Query *q = parse_string("\"a.txt\" and \"b.txt\" or not \"c.txt\"");

  mu_assert("top level is OR (or is the loosest binding)", q->kind == QUERY_OR);

  Query *left = q->as.binop.left;
  mu_assert("left side of OR is an AND subtree", left->kind == QUERY_AND);

  Query *right = q->as.binop.right;
  mu_assert("right side of OR is the negated leaf",
            right->kind == QUERY_GROUP && right->as.leaf.negated == true);
}

static void test_and_chain_of_three(void) {
  // a and b and c -- make sure three terms don't just silently
  // drop the third one (this was the exact bug in an earlier design)
  Query *q = parse_string("\"a.txt\" and \"b.txt\" and \"c.txt\"");

  mu_assert("top level is AND", q->kind == QUERY_AND);
  mu_assert("right side is the last leaf (c.txt)",
            q->as.binop.right->kind == QUERY_GROUP);

  Query *left = q->as.binop.left;
  mu_assert("left side is itself an AND (a and b)", left->kind == QUERY_AND);
}

// ==================================================================
// Level 3: one realistic full query, end to end
// ==================================================================

static void test_realistic_full_query(void) {
  Query *q = parse_string("newer_than regex(draft.*) and bigger_than 2048 or "
                          "contains regex(FIXME)");

  // top level should be OR, left side AND, right side a leaf
  mu_assert("top is OR", q->kind == QUERY_OR);
  mu_assert("left of OR is AND", q->as.binop.left->kind == QUERY_AND);
  mu_assert("right of OR is a plain leaf",
            q->as.binop.right->kind == QUERY_GROUP);
  mu_assert("right of OR is a contains",
            q->as.binop.right->as.leaf.group->kind == GROUP_CHARACTERISTIC);
  mu_assert("something", strcmp(q->as.binop.right->as.leaf.group->as
                                    .characteristic->as.content->regex_pattern,
                                "FIXME") == 0);

  // q = parse_string("not smaller_than \"2\" and contains regex(baalabaala)");
  // mu_assert("top is AND", q->kind == QUERY_AND);
  // mu_assert("left is a negated group", q->as.binop.left->as.leaf.negated ==
  // 1); mu_assert("left is a characteristic group",
  //           q->as.binop.left->as.leaf.group->kind == GROUP_CHARACTERISTIC);
  // mu_assert("left is a characteristic group",
  //           q->as.binop.left->as.leaf.group->as.characteristic->kind ==
  //               CHAR_SIZE);
}

int main(void) {
  mu_run_test(test_single_path);
  mu_run_test(test_comma_separated_paths);
  mu_run_test(test_name_regex);

  mu_run_test(test_newest_with_no_number);
  mu_run_test(test_newest_with_number);
  mu_run_test(test_bigger_than_number);
  mu_run_test(test_bigger_than_group);
  mu_run_test(test_contains_regex);

  mu_run_test(test_not_negates_the_leaf);

  mu_run_test(test_simple_and);
  mu_run_test(test_simple_or);
  mu_run_test(test_and_binds_tighter_than_or);
  mu_run_test(test_and_chain_of_three);

  mu_run_test(test_realistic_full_query);

  printf("\n%d tests run, %d failed\n", tests_run, tests_failed);
  return tests_failed != 0;
}
