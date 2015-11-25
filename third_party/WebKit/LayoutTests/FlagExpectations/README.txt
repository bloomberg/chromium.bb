FlagExpectations stores flag-specific test expectations.  To run layout tests
with a flag, use:

  run-webkit-tests --additional-driver-flag=--name-of-flag

In addition to passing --name-of-flag to the binary, run-webkit-tests will look
for test expectations in

  FlagExpectations/name-of-flag

which will override the main TestExpectations file.
