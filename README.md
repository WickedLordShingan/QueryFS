INFO :
I randomly came across fuse and developed this urge to write a custom filesystem. So here I am, QueryFS is a query based filesystem as the name suggests. 
Using a pretty minimal grammar (checkout [THE AST][./Parser/ast.h] and [THE GRAMMAR][./Parser/grammar.md] it is able to group the files that satisfy the
user's query and present it as a directory or a file (in cases where only a file satisfies the query)

DEMO :
The folder tests is used in the following images for testing the filesystem
