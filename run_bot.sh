#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
DIR="$DIR/cpp/build"
cd "$DIR"
exec "$DIR/uhp"
