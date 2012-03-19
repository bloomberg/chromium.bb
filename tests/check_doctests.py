#!/usr/bin/python
import doctest
import glob
import sys

exit_value = 0
for test in glob.iglob('doctests/*_test.txt'):
    failure_count, ignore = doctest.testfile(test, report=True, encoding='utf-8')
    if failure_count > 0: 
        exit_value = 1

sys.exit(exit_value)
