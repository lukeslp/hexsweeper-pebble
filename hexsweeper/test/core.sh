#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/test
node tools/geometry.cjs --check
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c \
  test/core.c src/c/game.c src/c/sphere.c src/c/controller.c -lm -o build/test/core
build/test/core

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c \
  test/chord.c src/c/game.c -o build/test/chord
build/test/chord

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c \
  test/infinite.c src/c/infinite.c -o build/test/infinite
build/test/infinite

cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c \
 test/world_save.c src/c/world_save.c src/c/infinite.c -o build/test/world-save
build/test/world-save

for suite in core chord; do
  cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Isrc/c test/classic_$suite.c src/c/classic.c -o build/test/classic-$suite
  build/test/classic-$suite
done
