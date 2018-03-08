#!/bin/sh

BASE_DIR=`pwd`

echo "Building infos..."

make -C $BASE_DIR/infos || exit 1

echo "Building infos user-space..."

make -C $BASE_DIR/infos-user || exit 1

echo "Creating infos user-space hard-disk image..."

make -C $BASE_DIR/infos-user fs || exit 1
