# Embed an arbitrary file into a generated C source as a byte string literal.
#
# Invoked at build time:
#   cmake -DBIN2C_SRC=<input> -DBIN2C_DST=<output.c> -DBIN2C_SYM=<symbol> \
#         -P bin2c.cmake
#
# Produces:
#   const char   <symbol>[]     = "\xNN\xNN...";   (trailing NUL added)
#   const unsigned long <symbol>_len = <byte count>;
#
# A string literal compiles far faster than a million-element array
# initializer. Each byte is emitted as its own \xNN escape, so adjacent
# escapes never merge.

file(READ "${BIN2C_SRC}" hex HEX)
string(LENGTH "${hex}" hexlen)
math(EXPR len "${hexlen} / 2")
string(REGEX REPLACE "(..)" "\\\\x\\1" body "${hex}")

file(WRITE "${BIN2C_DST}"
    "/* Generated from ${BIN2C_SRC} by bin2c.cmake — do not edit. */\n"
    "const char ${BIN2C_SYM}[] = \"${body}\";\n"
    "const unsigned long ${BIN2C_SYM}_len = ${len}UL;\n")
