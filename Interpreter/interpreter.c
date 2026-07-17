#include "interpreter.h"
#include "../Helpers/stb_ds.h"
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// Output_Group *evaluate_query_and(Query *self, const char *pwd,
//                                  const char **universe);
// Output_Group *evaluate_query_or(Query *self, const char *pwd,
//                                 const char **universe);
// NOTE :universe here is NOT absolute (relative to the PWD), so that regex
// is trivial to the user instead of having them worry about paths

const char **evaluate_group(Group *self, const char *pwd,
                            const char **universe);
const char **evaluate_group_name(NameGroup *self, const char *pwd,
                                 const char **universe);
const char **evaluate_group_char(CharacteristicGroup *self, const char *pwd,
                                 const char **universe);
const char **evaluate_char_time(TimeGroup *self, const char *pwd,
                                const char **universe);
const char **evaluate_char_size(SizeGroup *self, const char *pwd,
                                const char **universe);
const char **evaluate_char_content(ContentGroup *self, const char *pwd,
                                   const char **universe);

// helper to normalize paths to absolute paths. Hashmaps require paths to be
// canonical to avoid ./foo.txt and foo.txt being different things
char *resolve_path(const char *path, const char *pwd);

// helper to take the union of two sets
void union_finder(const char **first, const char **second,
                  const char ***result);
// helper to take the set intersection
void intersection_finder(const char **first, const char **second,
                         const char ***result);
// helper to subtract sets
void difference_finder(const char **first, const char **second,
                       const char ***result, const char *pwd);
// comparators for sorting the files
int cmp_filestat(const void *a, const void *b);
int also_cmp_filestat(const void *a, const void *b);

const char **evaluate(Query *self, const char *pwd, const char **universe) {
  switch (self->kind) {
  case QUERY_GROUP: {
    const char **result = evaluate_group(self->as.leaf.group, pwd, universe);
    if (self->as.leaf.negated) {
      const char **remaining_files = NULL;
      difference_finder(universe, result, &remaining_files, pwd);
      for (int i = 0; i < arrlen(result); i++)
        free((void *)result[i]);
      arrfree(result);
      result = remaining_files;
      remaining_files = NULL;
      return result;
    }
    return result;
  }
  case QUERY_AND: {
    const char **left = evaluate(self->as.binop.left, pwd, universe);
    const char **right = evaluate(self->as.binop.right, pwd, universe);
    const char **result = NULL;
    intersection_finder(left, right, &result);

    for (int i = 0; i < arrlen(left); i++)
      free((void *)left[i]);
    arrfree(left);
    for (int i = 0; i < arrlen(right); i++)
      free((void *)right[i]);
    arrfree(right);
    return result;
  }
  case QUERY_OR: {
    const char **left = evaluate(self->as.binop.left, pwd, universe);
    const char **right = evaluate(self->as.binop.right, pwd, universe);
    const char **result = NULL;
    union_finder(left, right, &result);
    for (int i = 0; i < arrlen(left); i++)
      free((void *)left[i]);
    arrfree(left);
    for (int i = 0; i < arrlen(right); i++)
      free((void *)right[i]);
    arrfree(right);
    return result;
  }
  }
}

const char **evaluate_group(Group *self, const char *pwd,
                            const char **universe) {
  switch (self->kind) {
  case GROUP_NAME:
    return evaluate_group_name(self->as.name, pwd, universe);
  case GROUP_CHARACTERISTIC:
    return evaluate_group_char(self->as.characteristic, pwd, universe);
  }
}

const char **evaluate_group_name(NameGroup *self, const char *pwd,
                                 const char **universe) {
  switch (self->kind) {
  case NAME_PATHS: {
    const char **result = NULL;
    for (int i = 0; i < arrlen(self->as.path_list.paths); i++) {
      // all leaf interpreting functions return absolute paths
      const char *abs_path = resolve_path(self->as.path_list.paths[i], pwd);
      if (abs_path == NULL) {
        continue;
      }
      struct stat st;
      int dir_len = strlen(abs_path);
      // if given path is a path to a directory then add all the files in the
      // directory to the result
      if (stat(abs_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        for (int i = 0; i < arrlen(universe); i++) {
          char *candidate = resolve_path(universe[i], pwd);
          if (candidate == NULL) {
            continue;
          }
          if (strncmp(candidate, abs_path, dir_len) == 0 &&
              (candidate[dir_len] == '/' || candidate[dir_len] == '\0')) {
            arrput(result, candidate);
          }
          free(candidate);
        }
        free(abs_path);
      } else {
        arrput(result, abs_path);
      }
    }
    return result;
  }
  case NAME_REGEX: {
    regex_t regex;
    int compile_status = regcomp(&regex, self->as.regex_pattern, REG_EXTENDED);
    if (compile_status != 0) {
      char error_message[100];
      regerror(compile_status, &regex, error_message, sizeof(error_message));
      printf("Regex compilation failed: %s\n", error_message);
      exit(-1);
    }
    const char **result = NULL;
    for (int i = 0; i < arrlen(universe); i++) {
      int match_status = regexec(&regex, universe[i], 0, NULL, 0);
      if (match_status == 0) {
        char *abs_path = resolve_path(universe[i], pwd);
        if (abs_path != NULL) {
          arrput(result, abs_path);
        }
      } else if (match_status != REG_NOMATCH) {
        char error_message[100];
        regerror(match_status, &regex, error_message, sizeof(error_message));
        printf("ENCOUNTERED ERROR WHILE TRYING TO MATCH %s\n", universe[i]);
        arrfree(result);
        exit(-1);
      }
    }
    return result;
  }
  }
}

const char **evaluate_group_char(CharacteristicGroup *self, const char *pwd,
                                 const char **universe) {
  switch (self->kind) {
  case CHAR_SIZE: {
    return evaluate_char_size(self->as.size, pwd, universe);
  }
  case CHAR_TIME: {
    return evaluate_char_time(self->as.time, pwd, universe);
  }
  case CHAR_CONTENT:
    return evaluate_char_content(self->as.content, pwd, universe);
  }
}

const char **evaluate_char_time(TimeGroup *self, const char *pwd,
                                const char **universe) {
  const char **result = NULL;
  FileStat *files_with_time = NULL;
  struct {
    const char *key; // absolute path
    time_t value;    // atime
  } *map = NULL;
  sh_new_strdup(map);

  for (int i = 0; i < arrlen(universe); i++) {
    char *abs_path = resolve_path(universe[i], pwd);
    if (abs_path == NULL) {
      continue;
    }
    struct stat st;
    if (stat(abs_path, &st) == 0) {
      shput(map, abs_path, st.st_atime);
      FileStat new;
      new.atime = st.st_atime;
      new.path = abs_path;
      arrput(files_with_time, new);
    } else {
      printf("SORRY THE FILE %s DOES NOT EXIST\n", abs_path);
      free(abs_path);
      goto cleanup;
    }
  }

  qsort(files_with_time, arrlen(files_with_time), sizeof(FileStat),
        cmp_filestat); // ascending by atime, oldest first

  if (self->compare.kind == TIME_CMP_SUPERLATIVE) {
    int n = arrlen(files_with_time);
    int slice_end = self->compare.as.superlative.has_count
                        ? (int)self->compare.as.superlative.count
                        : 1;
    if (slice_end > n)
      slice_end = n;

    if (self->kind == TIME_OLDER) {
      for (int i = 0; i < slice_end; i++) {
        arrput(result, strdup(files_with_time[i].path));
      }
    } else { // TIME_NEWER
      for (int i = n - slice_end; i < n; i++) {
        arrput(result, strdup(files_with_time[i].path));
      }
    }
    goto cleanup;

  } else if (self->compare.kind == TIME_CMP_THAN_GROUP) {
    // reference_group entries are already absolute , so they can be looked up
    // in `map` directly
    const char **reference_group =
        evaluate_group(self->compare.as.reference_group, pwd, universe);

    if (arrlen(reference_group) == 0) {
      arrfree(reference_group);
      goto cleanup;
    }

    time_t pivot;
    if (self->kind == TIME_NEWER) {
      pivot = 0;
      for (int i = 0; i < arrlen(reference_group); i++) {
        time_t v = shget(map, reference_group[i]);
        if (v > pivot)
          pivot = v; // max of reference group
      }
    } else {
      pivot = LONG_MAX;
      for (int i = 0; i < arrlen(reference_group); i++) {
        time_t v = shget(map, reference_group[i]);
        if (v < pivot)
          pivot = v; // min of reference group
      }
    }
    arrfree(reference_group);

    for (int i = 0; i < arrlen(files_with_time); i++) {
      time_t v = files_with_time[i].atime;
      if (self->kind == TIME_NEWER ? (v > pivot) : (v < pivot)) {
        arrput(result, strdup(files_with_time[i].path));
      }
    }
    goto cleanup;
  }

cleanup:
  for (int i = 0; i < arrlen(files_with_time); i++) {
    free((void *)files_with_time[i].path);
  }
  arrfree(files_with_time);
  shfree(map);
  return result;
}

const char **evaluate_char_size(SizeGroup *self, const char *pwd,
                                const char **universe) {
  const char **result = NULL;
  AlsoFileStat *files_with_size = NULL;
  struct {
    const char *key; // absolute path
    off_t value;     // size
  } *map = NULL;
  sh_new_strdup(map);

  for (int i = 0; i < arrlen(universe); i++) {
    char *abs_path = resolve_path(universe[i], pwd);
    if (abs_path == NULL) {
      continue;
    }
    struct stat st;
    if (stat(abs_path, &st) == 0) {
      shput(map, abs_path, st.st_size);
      AlsoFileStat new;
      new.size = st.st_size;
      new.path = abs_path;
      arrput(files_with_size, new);
    } else {
      printf("SORRY THE FILE %s DOES NOT EXIST\n", abs_path);
      free(abs_path);
      goto cleanup;
    }
  }

  qsort(files_with_size, arrlen(files_with_size), sizeof(AlsoFileStat),
        also_cmp_filestat); // ascending by size

  if (self->compare.kind == SIZE_CMP_SUPERLATIVE) {
    int n = arrlen(files_with_size);
    int slice_end = self->compare.as.superlative.has_count
                        ? (int)self->compare.as.superlative.count
                        : 1;
    if (slice_end > n)
      slice_end = n;

    if (self->kind == SIZE_SMALLER) {
      for (int i = 0; i < slice_end; i++) {
        arrput(result, strdup(files_with_size[i].path));
      }
    } else { // SIZE_BIGGER
      for (int i = n - slice_end; i < n; i++) {
        arrput(result, strdup(files_with_size[i].path));
      }
    }
    goto cleanup;
  } else if (self->compare.kind == SIZE_CMP_THAN_NUMBER) {
    long size_pivot = self->compare.as.byte_count;
    for (int i = 0; i < arrlen(files_with_size); i++) {
      off_t current_size = files_with_size[i].size;
      if (self->kind == SIZE_BIGGER ? (current_size > size_pivot)
                                    : (current_size < size_pivot)) {
        arrput(result, strdup(files_with_size[i].path));
      }
    }
    goto cleanup;
  } else if (self->compare.kind == SIZE_CMP_THAN_GROUP) {
    const char **reference_group =
        evaluate_group(self->compare.as.reference_group, pwd, universe);
    if (arrlen(reference_group) == 0) {
      arrfree(reference_group);
      goto cleanup;
    }

    off_t pivot;
    if (self->kind == SIZE_BIGGER) {
      pivot = 0;
      for (int i = 0; i < arrlen(reference_group); i++) {
        off_t v = shget(map, reference_group[i]);
        if (v > pivot)
          pivot = v; // max of reference group
      }
    } else {
      pivot = LONG_MAX;
      for (int i = 0; i < arrlen(reference_group); i++) {
        off_t v = shget(map, reference_group[i]);
        if (v < pivot)
          pivot = v; // min of reference group
      }
    }
    arrfree(reference_group);

    for (int i = 0; i < arrlen(files_with_size); i++) {
      off_t v = files_with_size[i].size;
      if (self->kind == SIZE_BIGGER ? (v > pivot) : (v < pivot)) {
        arrput(result, strdup(files_with_size[i].path));
      }
    }
    goto cleanup;
  }

cleanup:
  for (int i = 0; i < arrlen(files_with_size); i++) {
    free((void *)files_with_size[i].path);
  }
  arrfree(files_with_size);
  shfree(map);
  return result;
}

const char **evaluate_char_content(ContentGroup *self, const char *pwd,
                                   const char **universe) {
  char *regex = self->regex_pattern;
  bool owns_regex = false;
  if (self->kind == CONTENT_STARTS_WITH) {
    const char *prefix = "^(";
    const char *suffix = ")";
    size_t total_len = strlen(prefix) + strlen(regex) + strlen(suffix) + 1;
    char *new_regex = malloc(total_len);
    if (new_regex == NULL) {
      perror("Malloc failed");
      return NULL;
    }
    snprintf(new_regex, total_len, "%s%s%s", prefix, regex, suffix);
    regex = new_regex;
    owns_regex = true;
  }

  const char **result = NULL;
  regex_t regex_obj;
  int compile_status = regcomp(&regex_obj, regex, REG_EXTENDED);
  if (owns_regex) {
    free(regex);
  }
  if (compile_status != 0) {
    char error_message[100];
    regerror(compile_status, &regex_obj, error_message, sizeof(error_message));
    printf("Regex compilation failed: %s\n", error_message);
    return NULL; // was exit(-1) — same class of bug as the parser, fix this too
  }

  for (int i = 0; i < arrlen(universe); i++) {
    char *abs_path = resolve_path(universe[i], pwd);
    if (abs_path == NULL) {
      continue;
    }
    FILE *f = fopen(abs_path, "rb");
    if (f == NULL) {
      free(abs_path);
      continue;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    int match_status = regexec(&regex_obj, buf, 0, NULL, 0);
    if (match_status == 0) {
      arrput(result, abs_path);
    } else {
      free(abs_path);
    }
    free(buf);
  }

  regfree(&regex_obj);
  return result;
}

// Helpers
void union_finder(const char **first, const char **second,
                  const char ***result) {
  struct {
    char *key;
    int value;
  } *map = NULL;
  sh_new_strdup(map);

  for (int i = 0; i < arrlen(first); i++) {
    arrput(*result, first[i]);
    shput(map, first[i], 1);
  }

  for (int i = 0; i < arrlen(second); i++) {
    if (shgeti(map, second[i]) == -1) {
      arrput(*result, second[i]);
    }
  }

  shfree(map);
}

void intersection_finder(const char **first, const char **second,
                         const char ***result) {
  struct {
    char *key;
    int value;
  } *map = NULL;
  sh_new_strdup(map);

  for (int i = 0; i < arrlen(first); i++) {
    shput(map, first[i], 1);
  }

  for (int i = 0; i < arrlen(second); i++) {
    if (shgeti(map, second[i]) >= 0) {
      arrput(*result, second[i]);
    }
  }

  shfree(map);
}

void difference_finder(const char **first, const char **second,
                       const char ***result, const char *pwd) {
  struct {
    char *key;
    int value;
  } *map = NULL;
  sh_new_strdup(map);

  // NOTE:
  //  the only value we initialize first with is the universe and for the sake
  //  of easy usage we decided that universe is going to be all relative paths
  //  converting universe to absolute paths so that the hashmap lookup still
  //  works
  const char **abs_paths = NULL;
  for (int i = 0; i < arrlen(first); i++) {
    arrput(abs_paths, resolve_path(first[i], pwd));
  }

  const char **intersection = NULL;
  intersection_finder(abs_paths, second, &intersection);
  for (int i = 0; i < arrlen(intersection); i++) {
    shput(map, intersection[i], 1);
  }

  for (int i = 0; i < arrlen(abs_paths); i++) {
    if (shgeti(map, abs_paths[i]) == -1) {
      arrput(*result, abs_paths[i]);
    }
  }

  shfree(map);
  arrfree(intersection);
}

char *resolve_path(const char *path, const char *pwd) {
  char full[PATH_MAX];
  if (path[0] == '/') {
    strcpy(full, path); // already absolute, use as-is
  } else {
    snprintf(full, sizeof(full), "%s/%s", pwd, path);
  }
  char *resolved = realpath(full, NULL);
  return resolved;
}

int cmp_filestat(const void *a, const void *b) {
  const FileStat *fa = a;
  const FileStat *fb = b;

  if (fa->atime > fb->atime) {
    return 1;
  } else if (fa->atime < fb->atime) {
    return -1;
  }
  return 0;
}

int also_cmp_filestat(const void *a, const void *b) {
  const AlsoFileStat *fa = a;
  const AlsoFileStat *fb = b;

  if (fa->size > fb->size) {
    return 1;
  } else if (fa->size < fb->size) {
    return -1;
  }
  return 0;
}
