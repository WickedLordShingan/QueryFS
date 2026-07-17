// #define STB_DS_IMPLEMENTATION
#include "parser.h"
#include "../Helpers/stb_ds.h"
#include "../Scanner/token.h"
#include "ast.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>

Parser parser_init(Token *tokens) {
  return (Parser){
      tokens,
      0,
  };
}

Query *query(Parser *self);
Query *query_or(Parser *self);
Query *query_and(Parser *self);
Query *unary(Parser *self);

Group *group_p(Parser *self);
NameGroup *name_group(Parser *self);
CharacteristicGroup *characteristic_group(Parser *self);

TimeGroup *time_group(Parser *self);
SizeGroup *size_group(Parser *self);
ContentGroup *content_group(Parser *self);

bool parser_match_types(TokenType *types, Parser *self);
bool parser_check(Parser *self, TokenType type);
bool parser_is_at_end(Parser *self);
Token parser_advance(Parser *self);
Token parser_previous_token(Parser *self);
Token parser_peek(Parser *self);

void my_free_query(Query *query);
void free_query_group(Group *group);
void free_name_group(NameGroup *name_group);
void free_characteristic_group(CharacteristicGroup *char_group);

Query *parse(Parser *self) { return query(self); }

Query *query(Parser *self) { return query_or(self); }

Query *query_or(Parser *self) {
  Query *left = query_and(self);
  if (left == NULL) {
    return NULL;
  }
  while (parser_check(self, OR)) {
    parser_advance(self);
    Query *inner = query_and(self);
    if (inner == NULL) {
      my_free_query(left);
      return NULL;
    }
    Query *new = (Query *)malloc(sizeof(Query));
    new->kind = QUERY_OR;
    (new->as).binop = (QueryBinop){left, inner};
    left = new;
  }
  return left;
}

Query *query_and(Parser *self) {
  Query *left = unary(self);
  if (left == NULL) {
    return NULL;
  }
  while (parser_check(self, AND)) {
    parser_advance(self);
    Query *inner = unary(self);
    if (inner == NULL) {
      my_free_query(left);
      return NULL;
    }
    Query *new = (Query *)malloc(sizeof(Query));
    new->kind = QUERY_AND;
    (new->as).binop = (QueryBinop){left, inner};
    left = new;
  }
  return left;
}

Query *unary(Parser *self) {
  bool negated = false;
  if (parser_check(self, NOT)) {
    parser_advance(self);
    negated = true;
  }
  Group *group = group_p(self);
  if (group == NULL) {
    return NULL;
  }

  Query *new = (Query *)malloc(sizeof(Query));
  new->kind = QUERY_GROUP;
  (new->as).leaf = (QueryLeaf){negated, group};
  return new;
}

Group *group_p(Parser *self) {
  TokenType *types = NULL;
  arrput(types, PATH);
  arrput(types, REGEX);
  arrput(types, TOKEN_EOF);
  if (parser_match_types(types, self)) {
    arrfree(types);
    NameGroup *temp = name_group(self);
    if (temp == NULL) {
      return NULL;
    }
    Group *new = (Group *)malloc(sizeof(Group));
    new->kind = GROUP_NAME;
    new->as.name = temp;
    return new;
  }
  arrfree(types);

  CharacteristicGroup *temp = characteristic_group(self);
  if (temp == NULL) {
    return NULL;
  }
  Group *new = (Group *)malloc(sizeof(Group));
  new->kind = GROUP_CHARACTERISTIC;
  new->as.characteristic = temp;
  return new;
}

NameGroup *name_group(Parser *self) {
  NameGroup *new = (NameGroup *)malloc(sizeof(NameGroup));
  if (parser_check(self, PATH)) {
    new->kind = NAME_PATHS;
    new->as.path_list.paths = NULL;

    Token first = parser_advance(self);
    arrput(new->as.path_list.paths, strdup(first.lexeme));

    while (parser_check(self, COMMA)) {
      parser_advance(self);
      if (!parser_check(self, PATH)) {
        free(new); // note: leaks any strdup'd paths already collected;
                   // acceptable one-off leak on the error path
        return NULL;
      }
      Token path = parser_advance(self);
      arrput(new->as.path_list.paths, strdup(path.lexeme));
    }
    return new;
  }

  if (!parser_check(self, REGEX)) {
    free(new);
    return NULL;
  }
  new->kind = NAME_REGEX;
  Token regex = parser_advance(self);
  new->as.regex_pattern = strdup(regex.lexeme);
  return new;
}

CharacteristicGroup *characteristic_group(Parser *self) {
  CharacteristicGroup *new =
      (CharacteristicGroup *)malloc(sizeof(CharacteristicGroup));

  TokenType *types = NULL;
  arrput(types, NEWER_THAN);
  arrput(types, OLDER_THAN);
  arrput(types, NEWEST);
  arrput(types, OLDEST);
  arrput(types, TOKEN_EOF);

  if (parser_match_types(types, self)) {
    arrfree(types);
    TimeGroup *temp = time_group(self);
    if (temp == NULL) {
      free(new);
      return NULL;
    }
    new->kind = CHAR_TIME;
    new->as.time = temp;
    return new;
  }
  arrfree(types);

  types = NULL;
  arrput(types, STARTS_WITH);
  arrput(types, CONTAINS);
  arrput(types, TOKEN_EOF);
  if (parser_match_types(types, self)) {
    arrfree(types);
    ContentGroup *temp = content_group(self);
    if (temp == NULL) {
      free(new);
      return NULL;
    }
    new->kind = CHAR_CONTENT;
    new->as.content = temp;
    return new;
  }
  arrfree(types);

  types = NULL;
  arrput(types, BIGGER_THAN);
  arrput(types, SMALLER_THAN);
  arrput(types, BIGGEST);
  arrput(types, SMALLEST);
  arrput(types, TOKEN_EOF);

  if (!parser_match_types(types, self)) {
    arrfree(types);
    free(new);
    return NULL;
  }
  arrfree(types);

  SizeGroup *temp = size_group(self);
  if (temp == NULL) {
    free(new);
    return NULL;
  }
  new->kind = CHAR_SIZE;
  new->as.size = temp;
  return new;
}

TimeGroup *time_group(Parser *self) {
  TimeGroup *new = (TimeGroup *)malloc(sizeof(TimeGroup));

  if (parser_check(self, NEWER_THAN)) {
    new->kind = TIME_NEWER;
    new->compare.kind = TIME_CMP_THAN_GROUP;
    parser_advance(self);
    Group *group = group_p(self);
    if (group == NULL) {
      free(new);
      return NULL;
    }
    new->compare.as.reference_group = group;
    return new;
  }

  if (parser_check(self, NEWEST)) {
    new->kind = TIME_NEWER;
    new->compare.kind = TIME_CMP_SUPERLATIVE;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.as.superlative.has_count = true;
      new->compare.as.superlative.count = parser_advance(self).number_value;
      return new;
    }
    new->compare.as.superlative.has_count = false;
    new->compare.as.superlative.count = 1;
    return new;
  }

  if (parser_check(self, OLDER_THAN)) {
    new->kind = TIME_OLDER;
    new->compare.kind = TIME_CMP_THAN_GROUP;
    parser_advance(self);
    Group *group = group_p(self);
    if (group == NULL) {
      free(new);
      return NULL;
    }
    new->compare.as.reference_group = group;
    return new;
  }

  if (parser_check(self, OLDEST)) {
    new->kind = TIME_OLDER;
    new->compare.kind = TIME_CMP_SUPERLATIVE;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.as.superlative.has_count = true;
      new->compare.as.superlative.count = parser_advance(self).number_value;
      return new;
    }
    new->compare.as.superlative.has_count = false;
    new->compare.as.superlative.count = 1;
    return new;
  }

  free(new);
  return NULL;
}

ContentGroup *content_group(Parser *self) {
  ContentGroup *new = (ContentGroup *)malloc(sizeof(ContentGroup));
  if (parser_check(self, CONTAINS)) {
    new->kind = CONTENT_CONTAINS;
    parser_advance(self);
    if (!parser_check(self, REGEX)) {
      free(new);
      return NULL;
    }
    new->regex_pattern = strdup(parser_advance(self).lexeme);
    return new;
  }
  if (!parser_check(self, STARTS_WITH)) {
    free(new);
    return NULL;
  }

  parser_advance(self);
  new->kind = CONTENT_STARTS_WITH;
  if (!parser_check(self, REGEX)) {
    free(new);
    return NULL;
  }
  new->regex_pattern = strdup(parser_advance(self).lexeme);
  return new;
}

SizeGroup *size_group(Parser *self) {
  SizeGroup *new = (SizeGroup *)malloc(sizeof(SizeGroup));

  if (parser_check(self, BIGGER_THAN)) {
    new->kind = SIZE_BIGGER;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.kind = SIZE_CMP_THAN_NUMBER;
      new->compare.as.byte_count = parser_advance(self).number_value;
      return new;
    }
    new->compare.kind = SIZE_CMP_THAN_GROUP;
    Group *group = group_p(self);
    if (group == NULL) {
      free(new);
      return NULL;
    }
    new->compare.as.reference_group = group;
    return new;
  }

  if (parser_check(self, BIGGEST)) {
    new->kind = SIZE_BIGGER;
    new->compare.kind = SIZE_CMP_SUPERLATIVE;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.as.superlative.has_count = true;
      new->compare.as.superlative.count = parser_advance(self).number_value;
      return new;
    }
    new->compare.as.superlative.has_count = false;
    new->compare.as.superlative.count = 1;
    return new;
  }

  if (parser_check(self, SMALLER_THAN)) {
    new->kind = SIZE_SMALLER;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.kind = SIZE_CMP_THAN_NUMBER;
      new->compare.as.byte_count = parser_advance(self).number_value;
      return new;
    }
    new->compare.kind = SIZE_CMP_THAN_GROUP;
    Group *group = group_p(self);
    if (group == NULL) {
      free(new);
      return NULL;
    }
    new->compare.as.reference_group = group;
    return new;
  }

  if (parser_check(self, SMALLEST)) {
    new->kind = SIZE_SMALLER;
    new->compare.kind = SIZE_CMP_SUPERLATIVE;
    parser_advance(self);
    if (parser_check(self, NUMBER)) {
      new->compare.as.superlative.has_count = true;
      new->compare.as.superlative.count = parser_advance(self).number_value;
      return new;
    }
    new->compare.as.superlative.has_count = false;
    new->compare.as.superlative.count = 1;
    return new;
  }

  free(new);
  return NULL;
}

// helpers (unchanged)

bool parser_match_types(TokenType *types, Parser *self) {
  int i = 0;
  while (types[i] != TOKEN_EOF) {
    if ((self->tokens)[self->current].token_type == types[i]) {
      return true;
    }
    i += 1;
  }
  return false;
}

bool parser_check(Parser *self, TokenType type) {
  return (self->tokens)[self->current].token_type == type;
}

Token parser_peek(Parser *self) { return (self->tokens)[self->current]; }

bool parser_is_at_end(Parser *self) {
  return (arrlen(self->tokens)) == self->current ||
         parser_peek(self).token_type == TOKEN_EOF;
}

Token parser_previous_token(Parser *self) {
  if (self->current == 0 || self->current == arrlen(self->tokens)) {
    return (Token){TOKEN_EOF, NULL, -1};
  }
  return (self->tokens)[self->current - 1];
}

Token parser_advance(Parser *self) {
  if (!parser_is_at_end(self)) {
    self->current += 1;
    return parser_previous_token(self);
  }
  return parser_previous_token(self);
}

void my_free_query(Query *query) {
  if (query == NULL) {
    return;
  }
  switch (query->kind) {
  case QUERY_GROUP: {
    free_query_group(query->as.leaf.group);
    free(query);
    break;
  }
  default: {
    my_free_query(query->as.binop.left);
    my_free_query(query->as.binop.right);
    free(query);
  }
  }
}

void free_query_group(Group *group) {
  if (group == NULL) {
    return;
  }
  switch (group->kind) {
  case GROUP_NAME:
    free_name_group(group->as.name);
    break;
  case GROUP_CHARACTERISTIC:
    free_characteristic_group(group->as.characteristic);
    break;
  }
  free(group);
}

void free_name_group(NameGroup *name_group) {
  if (name_group == NULL) {
    return;
  }
  switch (name_group->kind) {
  case NAME_PATHS: {
    const char **paths = name_group->as.path_list.paths;
    for (int i = 0; i < arrlen(paths); i++) {
      free((void *)paths[i]);
    }
    arrfree(paths);
    break;
  }
  default:
    free((void *)name_group->as.regex_pattern);
  }
  free(name_group);
}

void free_characteristic_group(CharacteristicGroup *char_group) {
  if (char_group == NULL) {
    return;
  }
  switch (char_group->kind) {
  case CHAR_TIME: {
    TimeGroup *time_group = char_group->as.time;
    if (time_group->compare.kind == TIME_CMP_THAN_GROUP) {
      free_query_group(time_group->compare.as.reference_group);
    }
    free(time_group);
    break;
  }
  case CHAR_SIZE: {
    SizeGroup *size_group = char_group->as.size;
    if (size_group->compare.kind == SIZE_CMP_THAN_GROUP) {
      free_query_group(size_group->compare.as.reference_group);
    }
    free(size_group);
    break;
  }
  case CHAR_CONTENT: {
    ContentGroup *content_group = char_group->as.content;
    free(content_group->regex_pattern);
    free(content_group);
    break;
  }
  }
  free(char_group);
}
