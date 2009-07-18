#!/bin/sh
# Script to run layout tests under valgrind
# Example:
#  sh $0 LayoutTests/fast
# Caveats:
#  More of an example than a universal script.
#  Must be run from src directory.
#  Uses our standard suppressions; edit
#    tools/valgrind/memcheck/suppressions.txt
#  to disable any for bugs you're trying to reproduce.

cat > vlayout-wrapper.sh <<"_EOF_"
#!/bin/sh
valgrind --suppressions=tools/valgrind/memcheck/suppressions.txt --tool=memcheck --smc-check=all --num-callers=30 --trace-children=yes --leak-check=full --log-file=vlayout-%p.log "$@"
_EOF_
chmod +x vlayout-wrapper.sh

rm -f vlayout-*.log
export BROWSER_WRAPPER=`pwd`/vlayout-wrapper.sh
export G_SLICE=always-malloc
export NSS_DISABLE_ARENA_FREE_LIST=1
sh webkit/tools/layout_tests/run_webkit_tests.sh --run-singly -v --noshow-results --time-out-ms=200000 --nocheck-sys-deps --debug "$@"
cat vlayout-*.log
