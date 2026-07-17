query -> or_group

or_group -> and_group ("or" and_group)*

and_group -> unary ("and" unary)*

unary -> "not"? group

group -> (name_group | characteristic_group)

name_group -> PATH ("," PATH)* | regex (on name)

characteristic_group -> time_group | size_group | content_group

time_group -> newer_group | older_group

newer_group -> "newer than" group | newest (NUMBER)?

older_group -> "older than" group | oldest (NUMBER)?

size_group -> bigger_group | smaller_group

bigger_group -> "bigger than" (group | NUMBER) | "biggest" (NUMBER)?

smaller_group -> "smaller than" (group | NUMBER) | "smallest" (NUMBER)?

content_group -> starts_with_group | contains_group

starts_with_group -> "starts_with" regex

contains_group -> "contains" regex

regex -> "regex(" REGEX? ")"
