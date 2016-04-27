# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import logging
import os
import re
import shutil
import sys
import tempfile
import time


def VerboseCompileRegexOrAbort(regex):
  """Compiles a user-provided regular expression, exits the program on error."""
  try:
    return re.compile(regex)
  except re.error as e:
    sys.stderr.write('invalid regex: {}\n{}\n'.format(regex, e))
    sys.exit(2)


def PollFor(condition, condition_name, interval=5):
  """Polls for a function to return true.

  Args:
    condition: Function to wait its return to be True.
    condition_name: The condition's name used for logging.
    interval: Periods to wait between tries in seconds.

  Returns:
    What condition has returned to stop waiting.
  """
  while True:
    result = condition()
    logging.info('Polling condition %s is %s' % (
        condition_name, 'met' if result else 'not met'))
    if result:
      return result
    time.sleep(interval)


def SerializeAttributesToJsonDict(json_dict, instance, attributes):
  """Adds the |attributes| from |instance| to a |json_dict|.

  Args:
    json_dict: (dict) Dict to update.
    instance: (object) instance to take the values from.
    attributes: ([str]) List of attributes to serialize.

  Returns:
    json_dict
  """
  json_dict.update({attr: getattr(instance, attr) for attr in attributes})
  return json_dict


def DeserializeAttributesFromJsonDict(json_dict, instance, attributes):
  """Sets a list of |attributes| in |instance| according to their value in
    |json_dict|.

  Args:
    json_dict: (dict) Dict containing values dumped by
               SerializeAttributesToJsonDict.
    instance: (object) instance to modify.
    attributes: ([str]) List of attributes to set.

  Raises:
    AttributeError if one of the attribute doesn't exist in |instance|.

  Returns:
    instance
  """
  for attr in attributes:
    getattr(instance, attr) # To raise AttributeError if attr doesn't exist.
    setattr(instance, attr, json_dict[attr])
  return instance


@contextlib.contextmanager
def TemporaryDirectory():
  """Returns a freshly-created directory that gets automatically deleted after
  usage.
  """
  name = tempfile.mkdtemp()
  try:
    yield name
  finally:
    shutil.rmtree(name)


def EnsureParentDirectoryExists(path):
  """Verifies that the parent directory exists or creates it if missing."""
  parent_directory_path = os.path.abspath(os.path.dirname(path))
  if not os.path.isdir(parent_directory_path):
    os.makedirs(parent_directory_path)
