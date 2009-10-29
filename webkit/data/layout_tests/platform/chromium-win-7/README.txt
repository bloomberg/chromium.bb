This directory is intentionally empty.

Win 7 represents the latest version of Windows, and hence its expectations
can actually be found in the "win" port.

However, if the directory is actually empty then folks using git to track the
Chromium repo will not get a directory at all which makes run_webkit_tests.py
a sad panda.  Thus this README is here to make sure the directory is added so
tests can run.

This file should be removed whenever Windows 7-specific test expectations are
checked into this directory.
