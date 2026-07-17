#include "token.h"
#include <stdbool.h>

typedef struct {
  char *key;
  TokenType value;
} KeywordEntry;

typedef struct Scanner {
  const char *source;
  Token *tokens;
  int current;
  int start;
  KeywordEntry *keyword_map;
} Scanner;

Scanner scanner_init(const char *source);
void scan_tokens(Scanner *self);
void free_tokens(Token *tokens);
