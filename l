#!/usr/bin/env bash

set -e

cd "$(dirname "$0")"
make linux

export LD_LIBRARY_PATH="$(pwd)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./demo "$@"
