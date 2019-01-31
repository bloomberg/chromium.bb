#!/bin/bash

RESULTS="$(2>&1 >/dev/null git rev-parse --remotes | git cat-file --batch-check)"

if [[ -n "$RESULTS" ]]; then
    echo "Repairing ${PWD}"
    git fetch --all
fi
