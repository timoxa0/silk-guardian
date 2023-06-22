#!/bin/bash
set -eu

make clean
make all install
rmmod silk || true
modprobe silk

echo "done"
