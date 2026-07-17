#ifndef UNIVERSE_H
#define UNIVERSE_H

#include <time.h>

#define TTL 5

// for universe resolution
typedef struct {
  char **universe;
  struct timespec computed_at;
} CacheEntry;

#define MISSING_ENTRY                                                          \
  (CacheEntry) { NULL, {0} }

typedef struct {
  char *key;
  CacheEntry value;
} CacheMapEntry;

typedef struct {
  CacheMapEntry *map;
} UniverseCache;

void split_last_component(const char *path, char *prefix, char *last_segment);

UniverseCache universe_cache_init(void);
const char **get_cached_or_compute(UniverseCache *cache, const char *prefix,
                                   const char *og_directory);
const char **resolve_universe(UniverseCache *cache, const char *path,
                              const char *og_directory);

#endif
