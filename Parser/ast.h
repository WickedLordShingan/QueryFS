#ifndef AST_H
#define AST_H

#include <stdbool.h>

// ============================================================
// query -> or_group
// or_group -> and_group ("or" and_group)*
// and_group -> unary ("and" unary)*
// unary -> "not"? group
// ============================================================

typedef enum {
  QUERY_GROUP,
  QUERY_AND,
  QUERY_OR,
} QueryKind;

typedef struct query Query;

typedef struct {
  Query *left;
  Query *right;
} QueryBinop;

typedef struct {
  bool negated;
  struct group *group;
} QueryLeaf;

typedef union {
  QueryLeaf leaf;   // valid when kind == QUERY_GROUP
  QueryBinop binop; // valid when kind == QUERY_AND or QUERY_OR
} QueryData;

struct query {
  QueryKind kind;
  QueryData as;
};

// ============================================================
// group -> name_group | characteristic_group
// ============================================================

typedef enum {
  GROUP_NAME,
  GROUP_CHARACTERISTIC,
} GroupKind;

typedef struct group {
  GroupKind kind;
  union {
    struct name_group *name;
    struct characteristic_group *characteristic;
  } as;
} Group;

// ============================================================
// name_group -> PATH ("," PATH)* | regex (on name)
// ============================================================

typedef enum {
  NAME_PATHS, // comma-separated list of paths
  NAME_REGEX, // regex matched against filename
} NameGroupKind;

typedef struct name_group {
  NameGroupKind kind;
  union {
    struct {
      const char **paths; // list of paths
    } path_list;
    char *regex_pattern; // owned string, just the inner pattern text
  } as;
} NameGroup;

// ============================================================
// characteristic_group -> time_group | size_group | content_group
// ============================================================

typedef enum {
  CHAR_TIME,
  CHAR_SIZE,
  CHAR_CONTENT,
} CharacteristicGroupKind;

typedef struct characteristic_group {
  CharacteristicGroupKind kind;
  union {
    struct time_group *time;
    struct size_group *size;
    struct content_group *content;
  } as;
} CharacteristicGroup;

// ============================================================
// time_group -> newer_group | older_group
// newer_group -> "newer than" group | newest (NUMBER)?
// older_group -> "older than" group | oldest (NUMBER)?
// ============================================================

typedef enum {
  TIME_NEWER,
  TIME_OLDER,
} TimeGroupKind;

typedef enum {
  TIME_CMP_THAN_GROUP,  // "newer than"/"older than" <group>
  TIME_CMP_SUPERLATIVE, // "newest"/"oldest" [NUMBER]
} TimeCompareKind;

typedef struct {
  TimeCompareKind kind;
  union {
    Group *reference_group;
    struct {
      bool has_count; // if has_count is false then return the oldest / newest
      long count;
    } superlative; // valid when kind == TIME_CMP_SUPERLATIVE
  } as;
} TimeCompare;

typedef struct time_group {
  TimeGroupKind kind; // TIME_NEWER or TIME_OLDER
  TimeCompare compare;
} TimeGroup;

// ============================================================
// size_group -> bigger_group | smaller_group
// bigger_group -> "bigger than" (group | NUMBER) | "biggest" (NUMBER)?
// smaller_group -> "smaller than" (group | NUMBER) | "smallest" (NUMBER)?
// ============================================================

typedef enum {
  SIZE_BIGGER,
  SIZE_SMALLER,
} SizeGroupKind;

typedef enum {
  SIZE_CMP_THAN_GROUP,  // "bigger than"/"smaller than" <group>
  SIZE_CMP_THAN_NUMBER, // "bigger than"/"smaller than" <NUMBER bytes>
  SIZE_CMP_SUPERLATIVE, // "biggest"/"smallest" [NUMBER]
} SizeCompareKind;

typedef struct {
  SizeCompareKind kind;
  union {
    Group *reference_group; // valid when kind == SIZE_CMP_THAN_GROUP
    long byte_count;        // valid when kind == SIZE_CMP_THAN_NUMBER
    struct {
      bool has_count;
      long count;  // valid only when has_count is true
    } superlative; // valid when kind == SIZE_CMP_SUPERLATIVE
  } as;
} SizeCompare;

typedef struct size_group {
  SizeGroupKind kind; // SIZE_BIGGER or SIZE_SMALLER
  SizeCompare compare;
} SizeGroup;

// ============================================================
// content_group -> starts_with_group | contains_group
// starts_with_group -> "starts_with" regex
// contains_group -> "contains" regex
// regex -> "regex(" REGEX? ")"
// ============================================================

typedef enum {
  CONTENT_STARTS_WITH,
  CONTENT_CONTAINS,
} ContentGroupKind;

typedef struct content_group {
  ContentGroupKind kind;
  char *regex_pattern; // owned string, just the inner pattern text
                       // (empty string "" if "regex()" was given)
} ContentGroup;

#endif
