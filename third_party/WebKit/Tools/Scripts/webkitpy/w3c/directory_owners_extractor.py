# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import re

from webkitpy.common.path_finder import PathFinder
from webkitpy.common.system.filesystem import FileSystem


# Format of OWNERS files can be found at //src/third_party/depot_tools/owners.py
# In our use case (under external/wpt), we only process the first enclosing
# non-empty OWNERS file for any given path (i.e. always assuming "set noparent"),
# and we only care about lines that are valid email addresses.

# Recognizes 'X@Y' email addresses. Very simplistic. (from owners.py)
BASIC_EMAIL_REGEXP = r'^[\w\-\+\%\.]+\@[\w\-\+\%\.]+$'


class DirectoryOwnersExtractor(object):

    def __init__(self, filesystem=None):
        self.filesystem = filesystem or FileSystem()
        self.finder = PathFinder(filesystem)
        self.owner_map = None

    def list_owners(self, changed_files):
        """Looks up the owners for the given set of changed files.

        Args:
            changed_files: A list of file paths relative to the repository root.

        Returns:
            A dict mapping tuples of owner email addresses to lists of
            owned directories (paths relative to the root of layout tests).
        """
        email_map = collections.defaultdict(set)
        for relpath in changed_files:
            absolute_path = self.finder.path_from_chromium_base(relpath)
            if not absolute_path.startswith(self.finder.layout_tests_dir()):
                continue
            owners_file, owners = self.find_and_extract_owners(self.filesystem.dirname(relpath))
            if not owners_file:
                continue
            owned_directory = self.filesystem.dirname(owners_file)
            owned_directory_relpath = self.filesystem.relpath(owned_directory, self.finder.layout_tests_dir())
            email_map[tuple(owners)].add(owned_directory_relpath)
        return {owners: sorted(owned_directories) for owners, owned_directories in email_map.iteritems()}

    def find_and_extract_owners(self, start_directory):
        """Find the first enclosing OWNERS file for a given path and extract owners.

        Starting from the given directory, walks up the directory tree until the
        first non-empty OWNERS file is found or LayoutTests/external is reached.
        (OWNERS files with no valid emails are also considered empty.)

        Args:
            start_directory: A relative path from the root of the repository.

        Returns:
            (path, owners): the absolute path to the first non-empty OWNERS file
            found, and a list of valid owners.
            Or (None, None) if not found.
        """
        # Absolute paths do not work with path_from_chromium_base (os.path.join).
        assert not self.filesystem.isabs(start_directory)
        directory = self.finder.path_from_chromium_base(start_directory)
        external_root = self.finder.path_from_layout_tests('external')
        # Changes to both LayoutTests/TestExpectations and the entire
        # LayoutTests/FlagExpectations/ directory should be skipped and not
        # raise an assertion.
        if directory == self.finder.layout_tests_dir() or \
           directory.startswith(self.finder.path_from_layout_tests('FlagExpectations')):
            return None, None
        assert directory.startswith(external_root), '%s must start with %s' % (
            directory, external_root)
        while directory != external_root:
            owners_file = self.filesystem.join(directory, 'OWNERS')
            if self.filesystem.isfile(self.finder.path_from_chromium_base(owners_file)):
                owners = self.extract_owners(owners_file)
                if owners:
                    return owners_file, owners
            directory = self.filesystem.dirname(directory)
        return None, None

    def extract_owners(self, owners_file):
        """Extract owners from an OWNERS file.

        Args:
            owners_file: An absolute path to an OWNERS file.

        Returns:
            A list of valid owners (email addresses).
        """
        contents = self.filesystem.read_text_file(owners_file)
        email_regexp = re.compile(BASIC_EMAIL_REGEXP)
        addresses = []
        for line in contents.splitlines():
            line = line.strip()
            if email_regexp.match(line):
                addresses.append(line)
        return addresses
