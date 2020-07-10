# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage failure message of builds."""

from __future__ import print_function

from chromite.lib import failure_message_lib


class BuildFailureMessage(object):
  """Message indicating that changes failed to be validated.

  A failure message for a failed build, which is used to trige failures and
  detect bad changes.
  """

  def __init__(self, message_summary, failure_messages, internal, reason,
               builder):
    """Create a BuildFailureMessage instance.

    Args:
      message_summary: The message summary string to print.
      failure_messages: A list of failure messages (instances of
        StageFailureMessage), if any.
      internal: Whether this failure occurred on an internal builder.
      reason: A string describing the failure.
      builder: The builder the failure occurred on.
    """
    self.message_summary = str(message_summary)
    self.failure_messages = failure_messages or []
    self.internal = bool(internal)
    self.reason = str(reason)
    # builder should match build_config, e.g. self._run.config.name.
    self.builder = str(builder)

  def __str__(self):
    return self.message_summary

  def BuildFailureMessageToStr(self):
    """Return a string presenting the information in the BuildFailureMessage."""
    to_str = ('[builder] %s [message summary] %s [reason] %s [internal] %s\n' %
              (self.builder, self.message_summary, self.reason, self.internal))
    for f in self.failure_messages:
      to_str += '[failure message] ' + str(f) + '\n'

    return to_str

  def MatchesExceptionCategories(self, exception_categories):
    """Check if all of the failure_messages match the exception_categories.

    Args:
      exception_categories: A set of exception categories (members of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if all of the failure_messages match a member in
      exception_categories; else, False.
    """
    for failure in self.failure_messages:
      if failure.exception_category not in exception_categories:
        if (isinstance(failure, failure_message_lib.CompoundFailureMessage) and
            failure.MatchesExceptionCategories(exception_categories)):
          continue
        else:
          return False

    return True

  def HasExceptionCategories(self, exception_categories):
    """Check if any of the failure_messages match the exception_categories.

    Args:
      exception_categories: A set of exception categories (members of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if any of the failure_messages match a member in
      exception_categories; else, False.
    """
    for failure in self.failure_messages:
      if failure.exception_category in exception_categories:
        return True

      if (isinstance(failure, failure_message_lib.CompoundFailureMessage) and
          failure.HasExceptionCategories(exception_categories)):
        return True

    return False
