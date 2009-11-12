# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys

EXCLUDED_PATHS = (
    r"breakpad[\\\/].*",
    r"o3d_assets[\\\/].*",
    r"third_party[\\\/].*",
)

def FindOnPage(input_api, url, regex):
  """Given a url, download it and find the part matching a regex.
  Arguments:
    input_api: the limited set of input modules allowed in presubmit
    url: url to download
    regex: regex to match on
  Returns:
    A string extracted from the match, or None if no match or error.
  """
  try:
    connection = input_api.urllib2.urlopen(url)
    text = connection.read()
    connection.close()
    match = input_api.re.search(regex, text)
    if match:
      return match.group(1)
    else:
      return None
  except IOError, e:
    print str(e)
    return None


def CheckTreeIsOpen(input_api, output_api, url, url_text):
  """Similar to the one in presubmit_canned_checks except it shows an helpful
  status text instead.
  Arguments:
    input_api: the limited set of input modules allowed in presubmit
    output_api: the limited set of output modules allowed in presubmit
    url: url to get numerical tree status from
    url_text: url to get human readable tree status from
    regex: regex to match on
  Returns:
    A list of presubmit warnings.
  """
  assert(input_api.is_committing)
  # Check if tree is open.
  status = FindOnPage(input_api, url, '([0-9]+)')
  if status and int(status):
    return []
  # Try to find out what failed.
  message = FindOnPage(input_api, url_text,
                       r'\<div class\="Notice"\>(.*)\<\/div\>')
  if message:
    return [output_api.PresubmitPromptWarning("The tree is closed.",
                                              long_text=message.strip())]
  # Report unknown reason.
  return [output_api.PresubmitPromptWarning(
      "The tree status can't be checked.")]


def CheckChangeOnUpload(input_api, output_api):
  report = []
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  report.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  report.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  report.extend(CheckTreeIsOpen(
      input_api, output_api,
      'http://o3d-status.appspot.com/status',
      'http://o3d-status.appspot.com/current'))
  return report
