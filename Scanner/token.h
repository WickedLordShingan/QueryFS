#ifndef TOKEN_H
#define TOKEN_H

typedef enum TOKEN_TYPE {
  // missing
  MISSING,
  // logical_kewords
  OR,
  AND,
  NOT,

  // terminals
  PATH,
  NUMBER,
  REGEX,

  // kewords
  CONTAINS,
  STARTS_WITH,
  NEWER_THAN,
  OLDER_THAN,
  OLDEST,
  NEWEST,
  BIGGER_THAN,
  SMALLER_THAN,
  SMALLEST,
  BIGGEST,

  // helpers
  COMMA,
  TOKEN_EOF,
  TOKEN_ERROR,
} TokenType;

typedef struct Token {
  TokenType token_type;
  char *lexeme;      // NULL for keyword-only tokens
  long number_value; // valid only when token_type == NUMBER
} Token;

#endif
