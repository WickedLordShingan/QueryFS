#include "universe_and_related.h"
#include "../Helpers/stb_ds.h"
#include "../Interpreter/interpreter.h"
#include "../Parser/ast.h"
#include "../Parser/parser.h"
#include "../Scanner/scanner.h"
#include "../Scanner/token.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// helpers

static void free_universe(char **universe) {
  if (universe == NULL) {
    return;
  }
  for (int i = 0; i < arrlen(universe); i++) {
    free(universe[i]);
  }
  arrfree(universe);
}

static char **root_universe(const char *og_directory) {
  char **result = NULL;
  DIR *dir = opendir(og_directory);
  if (dir == NULL) {
    return result;
  }
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    arrput(result, strdup(entry->d_name));
  }
  closedir(dir);
  return result;
}

static Query *compile_query_segment(const char *segment_text) {
  Scanner scanner = scanner_init(segment_text);
  scan_tokens(&scanner);

  if (arrlen(scanner.tokens) > 0 &&
      scanner.tokens[arrlen(scanner.tokens) - 1].token_type == TOKEN_ERROR) {
    free_tokens(scanner.tokens);
    shfree(scanner.keyword_map);
    return NULL;
  }

  Parser parser = parser_init(scanner.tokens);
  Query *result = parse(&parser);

  free_tokens(scanner.tokens);
  shfree(scanner.keyword_map);

  return result;
}

UniverseCache universe_cache_init(void) {
  UniverseCache cache;
  cache.map = NULL; // stb_ds convention: NULL is a valid empty hashmap
  return cache;
}

void split_last_component(const char *path, char *prefix, char *last_segment) {
  size_t len = strlen(path);

  // strip a single trailing slash, if present (e.g. "/foo/bar/" -> "/foo/bar")
  if (len > 1 && path[len - 1] == '/') {
    len -= 1;
  }

  // find the rightmost slash
  int last_slash = -1;
  for (int i = (int)len - 1; i >= 0; i--) {
    if (path[i] == '/') {
      last_slash = i;
      break;
    }
  }

  if (last_slash == -1) {
    prefix[0] = '\0';
    memcpy(last_segment, path, len);
    last_segment[len] = '\0';
    return;
  }

  if (last_slash == 0) {
    // e.g. "/foo" -> prefix is root "/"
    prefix[0] = '/';
    prefix[1] = '\0';
  } else {
    memcpy(prefix, path, last_slash);
    prefix[last_slash] = '\0';
  }

  size_t seg_len = len - (last_slash + 1);
  memcpy(last_segment, path + last_slash + 1, seg_len);
  last_segment[seg_len] = '\0';
}

const char **resolve_universe(UniverseCache *cache, const char *path,
                              const char *og_directory) {
  if (strcmp(path, "/") == 0) {
    return (const char **)root_universe(og_directory);
  }

  char prefix[PATH_MAX];
  char last_segment[PATH_MAX];
  split_last_component(path, prefix, last_segment);

  const char **universe = get_cached_or_compute(cache, prefix, og_directory);

  // literal passthrough: is last_segment just an existing filename?
  for (int i = 0; i < arrlen(universe); i++) {
    const char *base = strrchr(universe[i], '/');
    base = base ? base + 1 : universe[i];
    if (strcmp(base, last_segment) == 0) {
      char abs_path[PATH_MAX];
      if (universe[i][0] == '/') {
        strncpy(abs_path, universe[i], sizeof(abs_path) - 1);
        abs_path[sizeof(abs_path) - 1] = '\0';
      } else {
        snprintf(abs_path, sizeof(abs_path), "%s/%s", og_directory,
                 universe[i]);
      }
      char **literal = NULL;
      arrput(literal, strdup(abs_path));
      return (const char **)literal;
    }
  }

  Query *query = compile_query_segment(last_segment);
  if (query == NULL) {
    return NULL;
  }
  const char **result = evaluate(query, og_directory, universe);
  my_free_query(query);
  return result;
}

const char **get_cached_or_compute(UniverseCache *cache, const char *prefix,
                                   const char *og_directory) {
  CacheMapEntry *found = shgetp_null(cache->map, prefix);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  bool valid = found != NULL && found->value.universe != NULL &&
               (now.tv_sec - found->value.computed_at.tv_sec) < TTL;

  if (valid) {
    return (const char **)found->value.universe;
  }

  // old cache
  if (found != NULL && found->value.universe != NULL) {
    free_universe(found->value.universe);
  }

  char **fresh = (char **)resolve_universe(cache, prefix, og_directory);

  CacheEntry new_entry;
  new_entry.universe = fresh;
  new_entry.computed_at = now;
  shput(cache->map, prefix, new_entry);

  return (const char **)fresh;
}
