#!/bin/sh

exec_dir=$(dirname $0)

PYTHON_PROG=python
# When not using the included python, we don't get automatic site.py paths.
# Specifically, rebaseline.py needs the paths in:
# third_party/python_24/Lib/site-packages/google.pth
PYTHONPATH="${exec_dir}/../../../tools/python:$PYTHONPATH"
export PYTHONPATH

"$PYTHON_PROG" "$exec_dir/rebaseline.py" "$@"
