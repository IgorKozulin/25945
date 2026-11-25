#!/bin/sh
set -eu
cd "$(dirname "$0")"
for n in 6 7 9 10 17 18 21 25 30 31 32; do
  echo "=== Task_$n ==="
  (cd "Task_$n" && make)
done
