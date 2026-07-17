#ifndef PARSER_H
#define PARSER_H

#include "../Scanner/token.h"
#include "ast.h"

typedef struct {
  Token *tokens;
  int current;
} Parser;

Parser parser_init(Token *tokens);
Query *parse(Parser *self);
void my_free_query(Query *query);

#endif
