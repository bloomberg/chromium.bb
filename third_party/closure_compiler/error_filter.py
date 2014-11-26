# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implement filtering out closure compiler errors due to incorrect type-
checking on promise chains.

For example, this code would raise a compiler error:

  Promise.resolve()
    .then(
        /** @return {!Promise<string>} */
        function() { return Promise.resolve('foo'); })
    .then(
        /** @param {string} s */
        function(s) { console.log(s); });

The compiler, while type-checking this code, doesn't unwrap the Promise returned
by the first callback in the Promise chain.  It incorrectly deduces that the
second function needs to take a {Promise<string>} as an argument, and emits an
error.  This class filters out these types of errors.

Note that this is just a coarse filter.  It doesn't, for example, check that the
unwrapped promise type actually matches the type accepted by the next callback
in the Promise chain.  Doing so would be the correct way to fix this problem,
but that fix belongs in the compiler.
"""

import re

class PromiseErrorFilter:
  """Runs checks to filter out promise chain errors."""
  def __init__(self):
    # For the code cited above, the compiler would emit an error like:
    # """
    # ERROR - actual parameter 1 of Promise.prototype.then does not match formal
    # parameter
    # found   : function (string): undefined
    # required: (function (Promise<string>): ?|null|undefined)
    #         function(s) { console.log(s); });
    #         ^
    # """

    # Matches the initial error message.
    self._error_msg = ("ERROR - actual parameter 1 of Promise.prototype.then "
                       "does not match formal parameter")

    # Matches the "found:" line.
    # Examples:
    # - function (string): Promise<string>
    # - function ((SomeType|null)): SomeOtherType
    self._found_line_regex = re.compile("found\s*:\s*function\s*\(.*\):\s*.*")

    # Matches the "required:" line.
    # Examples:
    # - (function(Promise<string>): ?|null|undefined)
    self._required_line_regex = re.compile(
      "required:\s*\(function\s*\(Promise<.*>\):\s*.*\)")

  def filter(self, error_list):
    """Filters promise chain errors out of the given list.

    Args:
        error_list: A list of errors from the closure compiler.

    Return:
        A list of errors, with promise chain errors removed.
    """
    return [error for error in error_list if not self._should_ignore(error)];

  def _should_ignore(self, error):
    """Should the given error be ignored?

    Args:
        error: A single entry from the closure compiler error list.

    Return:
        True if the error should be ignored, False otherwise.
    """
    error_lines = error.split('\n')

    if self._error_msg not in error_lines[0]:
      # Only ignore promise chain errors.
      return False
    else:
      # Check that the rest of the error message matches the expected form.
      return (self._found_line_regex.match(error_lines[1]) and
              self._required_line_regex.match(error_lines[2]))
