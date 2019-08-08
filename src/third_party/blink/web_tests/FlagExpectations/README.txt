FlagExpectations stores flag-specific test expectations.  To run layout tests
with a flag, use:

  run_web_tests.py --additional-driver-flag=--name-of-flag

Create a new file:

  FlagExpectations/name-of-flag

These are formatted:
  crbug.com/123456 path/to/your/test.html [ Expectation ]

Then run the tests with --additional-expectations:

  run_web_tests.py --additional-driver-flag=--name-of-flag
  --additional-expectations=path/to/FlagExpectations/name-of-flag

which will override the main TestExpectations file.
