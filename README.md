# QueryFS

# INFO
I randomly came across fuse and developed this urge to write a custom filesystem. So here I am, QueryFS is a query based filesystem as the name suggests. 
Using a pretty minimal grammar (checkout [THE AST](./Parser/ast.h) and [THE GRAMMAR](./Parser/grammar.md)) it is able to group the files that satisfy the user's query and present it as a directory or a file (in cases where only a file satisfies the query)

# DEMO
The folder tests is used in the following images for testing the filesystem

![cd_ls demo](./Demo/cd_ls.png)
![bigger and smaller](./Demo/bigger_and_smaller.png)
![older and newer](./Demo/older_and_newer.png)
![regex on content and path](./Demo/regex_on_content_and_path.png)

# REQUIREMENTS

## System Dependencies

- **GCC Compiler** - The project is compiled using GCC
- **FUSE 3 Library** - Required for the filesystem implementation (libfuse3 development headers)
- **pkg-config** - Used to locate and configure FUSE3
- **Make** - Build automation tool

## Installation

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install libfuse3-dev build-essential pkg-config
```

### Fedora/RHEL

```bash
sudo dnf install fuse3-devel gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S fuse3 gcc make pkg-config
```

### macOS

```bash
brew install osxfuse gcc make pkg-config
```

# BUILDING

Once all requirements are installed, build the project using:

```bash
make all
```

This will compile the filesystem binary and run all unit tests.

### Build targets:
- `make all` - Build the main filesystem binary and run tests
- `make test` - Run unit tests only (scanner, parser, interpreter tests)
- `make baalafs` - Build only the main filesystem binary
- `make clean` - Remove compiled binaries

The compiled filesystem binary will be created at `./binaries/baalafs`

# RUNNING

### Mount the filesystem

```bash
./binaries/baalafs <source_directory> <mount_point>
```

Example:
```bash
./binaries/baalafs ~/my_files ~/my_files_mount
```

### Query the filesystem

Once mounted, navigate to the mount point and query files using the minimal grammar:

```bash
cd ~/my_files_mount
ls                    # List all files
cd 'query_expression' # Navigate to files matching the query (ex : cd 'newer_than "a.txt")
cat 'query_expression_that_evaluates_to_a_single_file' (ex : cat 'newest')

```

### Unmount the filesystem

```bash
fusermount -u <mount_point>
```

Example:
```bash
fusermount -u ~/my_files_mount
```

### SOME LIMITATIONS
 - Paths should always be surrounded by double quotes
 - Paths should be typed with | in place / . The scanner will later substitute / in place of |
 - regex patterns should be surrounded in regex()
