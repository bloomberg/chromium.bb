# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class ParserInterface(object):
  """The interface for parsers."""

  def ParseChangelog(self, component_path, range_start, range_end):
    """Parses changelog from the URL stored in the parser object.

    Args:
      component_path: A string, path of the component. Path is used because
                      path is unique while component name is not.
      range_start: The start range of the regression.
      range_end: The end range of the regression.

    Returns:
      A tuple containing revision_map and file_to_revision_map,
      revision_map maps a CL number to a dictionary containing revision
      information such as author, commit message and the revision url.
      file_to_revision_map maps a name of a file to a tuple containing the CL
      number and path of the file that CL changes.
    """
    raise NotImplementedError()

  def ParseLineDiff(self, path, component, file_action, githash):
    """Parses the line diff of the given hash.

    Args:
      path: The path of the file.
      component: The component the file is from.
      file_action: Whether file is modified, deleted or added.
      githash: The git hashcode to check the line diff.

    Returns:
      url: The URL of the diff page, returns the changelog page for the
           file if the diff cannot be retrieved.
      changed_line_numbers: The list of the line numbers that has been
                            changed.
      changed_line_contents: The content of the changed lines.
    """
    raise NotImplementedError()

  def ParseBlameInfo(self, component, file_path, line, revision):
    """Parses blame information of the given file/line in revision.

    Args:
      component: The component this line is from.
      file_path: The path of the file.
      line: The line that caused the crash.
      revision: The revision to parse blame information for.

    Returns:
      The content of the line, the last changed revision of the line, author
      and the url of the revision.
    """
    raise NotImplementedError()
