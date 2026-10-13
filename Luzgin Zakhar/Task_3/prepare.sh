#!/bin/sh
# Создать демонстрационный файл с доступом только для владельца.
set -eu
umask 077
if [ ! -e private_data.txt ]; then
    printf 'Test file for Task 3\n' > private_data.txt
fi
chmod 600 private_data.txt
ls -l private_data.txt
