# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import multiprocessing
import tempfile
import os

from profile_creators import fast_navigation_profile_extender


class HistoryProfileExtender(
    fast_navigation_profile_extender.FastNavigationProfileExtender):
  """This extender navigates Chrome to a large number of URIs pointing to local
  files. It continues running until the history DB becomes full."""
  _HISTORY_DB_MAX_SIZE_IN_MB = 10

  def __init__(self):
    # The rate limiting factors are the speed of page navigation, and the speed
    # of python bindings. The former is larger than the latter, so having a
    # large batch size skews the amortized average time per page load towards
    # the latter.
    maximum_batch_size = multiprocessing.cpu_count() * 2
    super(HistoryProfileExtender, self).__init__(maximum_batch_size)

    # A list of paths of temporary files. The instance is responsible for
    # making sure that the files are deleted before they are removed from this
    # list.
    self._generated_temp_files = []

  def _MakeTemporaryFile(self):
    """Makes a temporary file and returns a URI to the file.

    This method has the side effect of appending the temporary file to
    self._generated_temp_files. The instance is responsible for deleting the
    file at a later point in time.
    """
    # Adding a long suffix to the name of the file fills up the history
    # database faster. The suffix can't be too long, since some file systems
    # have a 256 character limit on the length of the path. While we could
    # dynamically vary the length of the path, that would generate different
    # profiles on different OSes, which is not ideal.
    suffix = "reallylongsuffixintendedtoartificiallyincreasethelengthoftheurl"

    # Neither tempfile.TemporaryFile() nor tempfile.NamedTemporaryFile() work
    # well here. The former doesn't work at all, since it doesn't gaurantee a
    # file-system visible path. The latter doesn't work well, since the
    # returned file cannot be opened a second time on Windows. The returned
    # file would have to be closed, and the method would need to be called with
    # Delete=False, which makes its functionality no simpler than
    # tempfile.mkstemp().
    handle, path = tempfile.mkstemp(suffix=suffix)
    os.close(handle)
    self._generated_temp_files.append(path)

    file_url = "file://" + path
    return file_url

  def GetUrlIterator(self):
    """Superclass override."""
    while True:
      yield self._MakeTemporaryFile()

  def ShouldExitAfterBatchNavigation(self):
    """Superclass override."""
    return self._IsHistoryDBAtMaxSize()

  def TearDown(self):
    """Superclass override."""
    super(HistoryProfileExtender, self).TearDown()
    for path in self._generated_temp_files:
      os.remove(path)
    self._generated_temp_files = []

  def CleanUpAfterBatchNavigation(self):
    """Superclass override."""
    for path in self._generated_temp_files:
      os.remove(path)
    self._generated_temp_files = []

  def _IsHistoryDBAtMaxSize(self):
    """Whether the history DB has reached its maximum size."""
    history_db_path = os.path.join(self.profile_path, "Default", "History")
    stat_info = os.stat(history_db_path)
    size = stat_info.st_size

    max_size_threshold = 0.95
    bytes_in_megabyte = 2**20
    max_size = (bytes_in_megabyte *
        HistoryProfileExtender._HISTORY_DB_MAX_SIZE_IN_MB * max_size_threshold)
    return size > max_size
