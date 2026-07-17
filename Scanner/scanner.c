// #define STB_DS_IMPLEMENTATION
#include "scanner.h"
#include "../Helpers/stb_ds.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Token scan_token(Scanner *self);
bool is_a_num(char current);
bool is_path_char(char c);
bool starts_with(const char *pre, const char *str);
long number_reader(Scanner *self);
char *string_reader(Scanner *self);

bool is_at_end(Scanner *self);
char peek(Scanner *self);
char advance(Scanner *self);
bool match(Scanner *self, char match);

Scanner scanner_init(const char *source) {
  KeywordEntry *map = NULL;
  shput(map, "contains", CONTAINS);
  shput(map, "starts_with", STARTS_WITH);
  shput(map, "newer_than", NEWER_THAN);
  shput(map, "older_than", OLDER_THAN);
  shput(map, "oldest", OLDEST);
  shput(map, "newest", NEWEST);
  shput(map, "bigger_than", BIGGER_THAN);
  shput(map, "smaller_than", SMALLER_THAN);
  shput(map, "smallest", SMALLEST);
  shput(map, "biggest", BIGGEST);
  shput(map, "and", AND);
  shput(map, "or", OR);
  shput(map, "not", NOT);
  shdefault(map, MISSING);
  return (Scanner){
      .source = source,
      .tokens = NULL,
      .start = 0,
      .current = 0,
      .keyword_map = map,
  };
}

void scan_tokens(Scanner *self) {
  while (!is_at_end(self)) {
    // skip ALL consecutive whitespace
    while (!is_at_end(self) &&
           (peek(self) == ' ' || peek(self) == '\t' || peek(self) == '\n')) {
      advance(self);
    }
    if (is_at_end(self)) {
      break;
    }
    self->start = self->current;
    Token t = scan_token(self);
    if (t.token_type == TOKEN_ERROR) {
      arrput(self->tokens, t);
      break;
    }
    arrput(self->tokens, t);
    self->start = self->current;
  }
  Token eof = (Token){
      TOKEN_EOF,
      NULL,
      -1,
  };
  arrput(self->tokens, eof);
}

Token scan_token(Scanner *self) {
  if (match(self, '"')) {
    // advance(self);
    char *path_name = (char *)malloc(513);
    int i = 0;
    while (!is_at_end(self) && i <= 512 && peek(self) != '"') {
      char temp = advance(self);
      if (temp == '/') {
        printf("READ THE DOCS : '/' IS FORBIDDEN\n");
        return (Token){TOKEN_ERROR, NULL, -1};
      }
      if (temp == '|') {
        temp = '/';
      }
      path_name[i++] = temp;
    }
    if (i == 513) {
      printf("TOO MANY CHARACTERS BRUH : position {%d}\n", self->current);
      free(path_name);
      return (Token){TOKEN_ERROR, NULL, -1};
    }
    if (is_at_end(self)) {
      printf("UNTERMINATED PATH : position {%d}\n", self->current);
      free(path_name);
      return (Token){TOKEN_ERROR, NULL, -1};
    }
    advance(self);
    path_name[i] = '\0';
    return (Token){
        PATH,
        path_name,
        -1,
    };
  }
  if (match(self, ',')) {
    return (Token){
        COMMA,
        NULL,
        (long)-1,
    };
  } else {
    if (is_a_num(peek(self))) {
      long num = number_reader(self);
      return (Token){
          NUMBER,
          NULL,
          num,
      };
    }

    if (is_path_char(peek(self))) {
      char *potentital_keyword = string_reader(self);
      TokenType value = shget(self->keyword_map, potentital_keyword);
      if (value != MISSING) {
        free(potentital_keyword); // keyword tokens don't need the lexeme
        return (Token){
            value,
            NULL,
            -1,
        };
      }
      if (starts_with("regex(", potentital_keyword)) {
        char *cparen = strstr(potentital_keyword, ")");
        if (cparen == NULL) {
          printf("WHERE IS THE CLOSING PAREN : approximate position {%d}\n",
                 self->current);

          free(potentital_keyword);
          return (Token){TOKEN_ERROR, NULL, -1};
        }
        potentital_keyword[cparen - potentital_keyword] = '\0';
        // copy out the inner pattern so we can free the original buffer
        char *pattern = strdup(potentital_keyword + 6);
        free(potentital_keyword);
        return (Token){
            REGEX,
            pattern,
            -1,
        };
      }
      // if (potentital_keyword[0] == '"') {
      //   char *closing_quotes = strstr(potentital_keyword, "\"");
      //   if (closing_quotes == NULL) {
      //     printf("UNTERMINATED PATH NAME : position {%d}\n", self->current);
      //     exit(-1);
      //   }
      //   potentital_keyword[closing_quotes - potentital_keyword] = '\0';
      //   char *path = strdup(potentital_keyword + 1);
      //
      //   free(potentital_keyword);
      //   return (Token){
      //       PATH,
      //       path,
      //       -1,
      //   };
      // }
    }
    printf("WHAT THE HELL IS THIS ?\n");
    printf("                - lexer\n");
    printf("LOOK position : {%d}\n", self->current);
    return (Token){TOKEN_ERROR, NULL, -1};
  }
}

// helper functions

long number_reader(Scanner *self) {
  char number[33];
  int i = 0;
  while (is_a_num(peek(self)) && i <= 32) {
    number[i] = advance(self);
    i += 1;
  }
  if (i == 33) {
    printf("TOO MANY DIGITS BRUH : position {%d}\n", self->current);
    return 1;
  }
  number[i] = '\0';
  return (long)atoi(number);
}

char *string_reader(Scanner *self) {
  char *potentital_keyword = (char *)malloc(sizeof(char) * 513);
  int i = 0;
  while (is_path_char(peek(self)) && i <= 512) {
    potentital_keyword[i] = advance(self);
    i += 1;
  }
  if (i == 513) {
    printf("TOO MANY CHARACTERS BRUH : position {%d}\n", self->current);
    exit(-1);
  }
  potentital_keyword[i] = '\0';
  return potentital_keyword;
}

bool is_a_num(char current) { return current >= '0' && current <= '9'; }

// custom char_set
// so "regex(...)" bodies aren't truncated mid-scan)
bool is_path_char(char c) {
  if (c == '\0')
    return false;
  return isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-' ||
         c == '~' || c == '(' || c == ')' || c == '*' || c == '\\' ||
         c == '+' || c == '?' || c == '[' || c == ']' || c == '^' || c == '$' ||
         c == '|';
}

bool is_at_end(Scanner *self) {
  if (self->current >= strlen(self->source)) {
    return true;
  }
  return false;
}

char advance(Scanner *self) {
  self->current += 1;
  return (self->source)[self->current - 1];
}

bool match(Scanner *self, char match) {
  if (is_at_end(self)) {
    return false;
  }
  if ((self->source)[self->current] == match) {
    self->current += 1;
    return true;
  }
  return false;
}

char peek(Scanner *self) { return (self->source)[self->current]; }

bool starts_with(const char *pre, const char *str) {
  return strncmp(pre, str, strlen(pre)) == 0;
}

void free_tokens(Token *tokens) {
  for (int i = 0; i < arrlen(tokens); i++) {
    if (tokens[i].lexeme != NULL) {
      free(tokens[i].lexeme);
    }
  }
  arrfree(tokens);
}
