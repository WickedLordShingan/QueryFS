// if here you define the implementation macro then you need to comment out the
// implementation macro inside of scanner.c
#define STB_DS_IMPLEMENTATION
#define FUSE_USE_VERSION 31

#include "./Helpers/stb_ds.h"
#include "./Interpreter/interpreter.h"
#include "./Parser/parser.h"
#include "./Scanner/scanner.h"
#include "./Universe_and_related/universe_and_related.h"

#include <errno.h>
#include <fuse3/fuse.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// TODO: getattr, readdir, open, read, release, mkdir, plus
// opendir/releasedir
static char og_directory[PATH_MAX];
static UniverseCache cache;

static int fs_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
  (void)fi;
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  const char **universe = resolve_universe(&cache, path, og_directory);
  if (universe == NULL) {
    return -ENOENT;
  }

  // any query that results in single file is consider as a file (weird yes, but
  // it allows cool things like nvim newest)
  int result = 0;
  if (arrlen(universe) == 1) {
    if (stat(universe[0], stbuf) == 0) {
    } else {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    }
  } else {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }

  for (int i = 0; i < arrlen(universe); i++) {
    free((void *)universe[i]);
  }
  arrfree(universe);
  return result;
}

static int fs_opendir(const char *path, struct fuse_file_info *fi) {
  const char **universe = resolve_universe(&cache, path, og_directory);
  if (universe == NULL) {
    return -ENOENT;
  }
  return 0;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
  (void)buf;
  (void)fi;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  const char **universe = resolve_universe(&cache, path, og_directory);
  for (int i = 0; i < arrlen(universe); i++) {
    const char *base = strrchr(universe[i], '/');
    base = base ? base + 1 : universe[i];
    filler(buf, base, NULL, 0, 0);
  }

  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  const char **universe = resolve_universe(&cache, path, og_directory);
  if (universe == NULL || arrlen(universe) != 1) {
    if (universe != NULL) {
      for (int i = 0; i < arrlen(universe); i++) {
        free((void *)universe[i]);
      }
      arrfree(universe);
    }
    return -ENOENT;
  }

  int fd = open(universe[0], fi->flags);
  for (int i = 0; i < arrlen(universe); i++) {
    free((void *)universe[i]);
  }
  arrfree(universe);

  if (fd < 0) {
    return -errno;
  }

  fi->fh = (uint64_t)fd;
  return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  (void)path;
  int fd = (int)fi->fh;

  ssize_t bytes_read = pread(fd, buf, size, offset);
  if (bytes_read < 0) {
    return -errno;
  }
  return (int)bytes_read;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
  (void)path;
  close((int)fi->fh);
  return 0;
}

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  (void)path;
  int fd = (int)fi->fh;

  ssize_t bytes_written = pwrite(fd, buf, size, offset);
  if (bytes_written < 0) {
    return -errno;
  }
  return (int)bytes_written;
}

static int fs_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
  // might be called on a file that's already open
  if (fi != NULL) {
    if (ftruncate((int)fi->fh, size) != 0) {
      return -errno;
    }
    return 0;
  }

  const char **universe = resolve_universe(&cache, path, og_directory);
  int result = 0;
  if (universe == NULL || arrlen(universe) != 1) {
    if (universe != NULL) {
      for (int i = 0; i < arrlen(universe); i++) {
        free((void *)universe[i]);
      }
      arrfree(universe);
    }
    return -ENOENT;
  }
  if (truncate(universe[0], size) != 0) {
    result = -errno;
  }

  for (int i = 0; i < arrlen(universe); i++) {
    free((void *)universe[i]);
  }
  arrfree(universe);

  return result;
}

static int fs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
  (void)path;
  int res = datasync ? fdatasync((int)fi->fh) : fsync((int)fi->fh);
  if (res != 0) {
    return -errno;
  }
  return 0;
}

static int fs_mkdir(const char *path, mode_t mode) {
  (void)mode;
  printf("CALLED\n");

  const char **result = resolve_universe(&cache, path, og_directory);
  if (result == NULL) {
    return -EINVAL; // query didn't resolve/parse
  }

  char prefix[PATH_MAX];
  char query_text[PATH_MAX];
  split_last_component(path, prefix, query_text);

  // strip trailing slash
  char dir_label[PATH_MAX];
  strncpy(dir_label, prefix, sizeof(dir_label));
  size_t len = strlen(dir_label);
  if (len > 1 && dir_label[len - 1] == '/') {
    dir_label[len - 1] = '\0';
  }

  char new_dir_path[PATH_MAX];
  for (char *c = dir_label; *c; c++) {
    if (*c == '/')
      *c = '|';
  }
  snprintf(new_dir_path, sizeof(new_dir_path), "%s/%s from %s", og_directory,
           query_text, dir_label);

  printf("MADE\n");
  int mkdir_status = 0;
  if (mkdir(new_dir_path, 0755) != 0) {
    mkdir_status = -errno;
  }

  for (int i = 0; i < arrlen(result); i++) {
    // get the basename from absolute path
    const char *filename = strrchr(result[i], '/');
    filename = filename ? filename + 1 : result[i];

    char link_path[PATH_MAX];
    snprintf(link_path, sizeof(link_path), "%s/%s", new_dir_path, filename);
    symlink(result[i], link_path);
    printf("LINKED %s\n", result[i]);
  }

  for (int i = 0; i < arrlen(result); i++) {
    free((void *)result[i]);
  }
  arrfree(result);

  return mkdir_status;
}

static struct fuse_operations fs_ops = {
    .getattr = fs_getattr,
    .opendir = fs_opendir,
    .releasedir = fs_releasedir,
    .readdir = fs_readdir,

    .open = fs_open,
    .read = fs_read,
    .release = fs_release,

    .write = fs_write,
    .fsync = fs_fsync,
    .truncate = fs_truncate,

    .mkdir = fs_mkdir,
};

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <og_directory> <mountpoint> [fuse options]\n",
            argv[0]);
    return 1;
  }

  realpath(argv[1], og_directory);
  cache = universe_cache_init();

  for (int i = 1; i < argc - 1; i++) {
    argv[i] = argv[i + 1];
  }
  argc -= 1;

  return fuse_main(argc, argv, &fs_ops, NULL);
}
