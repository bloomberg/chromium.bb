# Traffic Annotation Scripts
This file describes the scripts in `tools/traffic_annotation/scripts`.


# check_annotations.py
Runs traffic annotation tests on the changed files or all repository. The tests
are run in error resilient mode. Requires a compiled build directory to run.

# traffic_annotation_auditor_tests.py
Runs tests to ensure traffic_annotation_auditor is performing as expected. Tests
include:
 - Checking if auditor and underlying clang tool run, and an expected minimum
   number of outputs are returned.
 - Checking if enabling or disabling heuristics that make tests faster has any
   effect on the output.
 - Checking if running on the whole repository (instead of only changed files)
   would result in any error.
This test may take a few hours to run and requires a compiled build directory.

# annotation_tools.py
Provides tools for annotation test scripts.