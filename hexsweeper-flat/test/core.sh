#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/test
for suite in core chord; do
  cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c \
    test/$suite.c src/c/game.c -o build/test/$suite
  build/test/$suite
done
