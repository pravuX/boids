#!/bin/bash
set -e

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <source_file.c>"
    exit 1
fi

mkdir -p obj

source_file="$1"
object_file="./obj/$(basename "$source_file" .c).o"
executable="./$(basename "$source_file" .c).out"

# for generating debug symbols
# gcc -ggdb -c "$source_file" -o "$object_file"
gcc -c "$source_file" -o "$object_file"
gcc -o "$executable" "$object_file" -Wall -lraylib -lm -lpthread -ldl -lrt -lX11

# echo "Build successful. Running $executable:"
# ./"$executable"
