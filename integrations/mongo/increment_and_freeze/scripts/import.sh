#!/bin/bash
# This script re-vendors the Increment-and-Freeze cache simulator.
#
# Unlike the other libraries under src/third_party, this one is pulled straight from the
# research upstream rather than from a mongodb-forks fork -- it exists for local cache-analysis
# builds only. See ../README.md.

set -o verbose
set -o errexit
set -euo pipefail
IFS=$'\n\t'

set -vx

NAME=increment_and_freeze
UPSTREAM="git@github.com:etwest/Increment-and-Freeze.git"
BRANCH="sampling"
REVISION="9cb484c7ab82fd62c68667e6bb993e5f18903cc9"
VERSION="0.0.0-$BRANCH-${REVISION:0:12}"

# Translation units, matching IAF_SRCS in the upstream CMakeLists.txt.
SRCS=(
    bounded_iaf.cc
    iaf_api.cc
    increment_and_freeze.cc
    projection.cc
)

# Header closure of the above. The remaining upstream headers pull in the simulator drivers
# and the container/ subtree, which are not vendored.
HDRS=(
    bounded_iaf.h
    cache_sim.h
    iaf_api.h
    iaf_params.h
    increment_and_freeze.h
    op.h
    partition.h
    projection.h
)

DEST_DIR=$(git rev-parse --show-toplevel)/src/third_party/$NAME
if [[ -d $DEST_DIR/dist ]]; then
    echo "You must remove '$DEST_DIR/dist' before running $0" >&2
    exit 1
fi

SRC_DIR=$(mktemp -d)
trap 'rm -rf "$SRC_DIR"' EXIT

git clone --branch "$BRANCH" "$UPSTREAM" "$SRC_DIR/upstream"
git -C "$SRC_DIR/upstream" checkout "$REVISION"

mkdir -p "$DEST_DIR/dist/src" "$DEST_DIR/dist/includes"
for f in "${SRCS[@]}"; do
    cp "$SRC_DIR/upstream/src/$f" "$DEST_DIR/dist/src/$f"
done
for f in "${HDRS[@]}"; do
    cp "$SRC_DIR/upstream/includes/$f" "$DEST_DIR/dist/includes/$f"
done
cp "$SRC_DIR/upstream/LICENSE.txt" "$DEST_DIR/dist/LICENSE.txt"
