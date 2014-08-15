# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from threading import Lock

import crash_utils


REVIEW_URL_PATTERN = re.compile(r'Review URL:( *)(.*)')


class Match(object):
  """Represents a match entry.

  A match is a CL that is suspected to have caused the crash. A match object
  contains information about files it changes, their authors, etc.

  Attributes:
    is_reverted: True if this CL is reverted by other CL.
    revert_of: If this CL is a revert of some other CL, a revision number/
               git hash of that CL.
    crashed_line_numbers: The list of lines that caused crash for this CL.
    function_list: The list of functions that caused the crash.
    min_distance: The minimum distance between the lines that CL changed and
                  lines that caused the crash.
    changed_files: The list of files that the CL changed.
    changed_file_urls: The list of URLs for the file.
    author: The author of the CL.
    component_name: The name of the component that this CL belongs to.
    stack_frame_indices: For files that caused crash, list of where in the
                         stackframe they occur.
    rank: The highest priority among the files the CL changes. Priority = 1
          if it changes the crashed line, and priority = 2 if it is a simple
          file change.
    priorities: A list of priorities for each of the changed file.
    reivision_url: The revision URL of the CL.
    review_url: The codereview URL that reviews this CL.
    reviewers: The list of people that reviewed this CL.
    reason: The reason why this CL is suspected.
  """
  REVERT_PATTERN = re.compile(r'(revert\w*) r?(\d+)', re.I)

  def __init__(self, revision, component_name):
    self.is_reverted = False
    self.revert_of = None
    self.crashed_line_numbers = []
    self.function_list = []
    self.min_distance = crash_utils.INFINITY
    self.changed_files = []
    self.changed_file_urls = []
    self.author = revision['author']
    self.component_name = component_name
    self.stack_frame_indices = []
    self.rank = crash_utils.INFINITY
    self.priorities = []
    self.revision_url = revision['url']
    self.review_url = ''
    self.reviewers = []
    self.reason = None

  def ParseMessage(self, message, codereview_api_url):
    """Parses the message.

    It checks the message to extract the code review website and list of
    reviewers, and it also checks if the CL is a revert of another CL.

    Args:
      message: The message to parse.
      codereview_api_url: URL to retrieve codereview data from.
    """
    for line in message.splitlines():
      line = line.strip()
      review_url_line_match = REVIEW_URL_PATTERN.match(line)

      # Check if the line has the code review information.
      if review_url_line_match:

        # Get review number for the code review site from the line.
        issue_number = review_url_line_match.group(2)

        # Get JSON from the code review site, ignore the line if it fails.
        url = codereview_api_url % issue_number
        json_string = crash_utils.GetDataFromURL(url)
        if not json_string:
          logging.warning('Failed to retrieve code review information from %s',
                          url)
          continue

        # Load the JSON from the string, and get the list of reviewers.
        code_review = crash_utils.LoadJSON(json_string)
        if code_review:
          self.reviewers = code_review['reviewers']

      # Check if this CL is a revert of other CL.
      if line.lower().startswith('revert'):
        self.is_reverted = True

        # Check if the line says what CL this CL is a revert of.
        revert = self.REVERT_PATTERN.match(line)
        if revert:
          self.revert_of = revert.group(2)
        return


class MatchSet(object):
  """Represents a set of matches.

  Attributes:
    matches: A map from CL to a match object.
    cls_to_ignore: A set of CLs to ignore.
    matches_lock: A lock guarding matches dictionary.
  """

  def __init__(self, codereview_api_url):
    self.codereview_api_url = codereview_api_url
    self.matches = {}
    self.cls_to_ignore = set()
    self.matches_lock = Lock()

  def RemoveRevertedCLs(self):
    """Removes CLs that are revert."""
    for cl in self.matches:
      if cl in self.cls_to_ignore:
        del self.matches[cl]
