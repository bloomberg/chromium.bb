# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import re

from webkitpy.common.path_finder import PathFinder
from webkitpy.common.system.filesystem import FileSystem


class DirectoryOwnersExtractor(object):

    def __init__(self, filesystem=None):
        self.filesystem = filesystem or FileSystem
        self.finder = PathFinder(filesystem)
        self.owner_map = None

    def read_owner_map(self):
        """Reads W3CImportExpectations and stores the owner information."""
        input_path = self.finder.path_from_layout_tests('W3CImportExpectations')
        input_contents = self.filesystem.read_text_file(input_path)
        self.owner_map = self.lines_to_owner_map(input_contents.splitlines())

    def lines_to_owner_map(self, lines):
        """Returns a dict mapping directories to owners.

        Args:
            lines: Lines in W3CImportExpectations.
        """
        current_owners = []
        owner_map = {}
        for line in lines:
            owners = self.extract_owners(line)
            if owners:
                current_owners = owners
            directory = self.extract_directory(line)
            if not owners and not directory:
                current_owners = []
            if current_owners and directory:
                owner_map[directory] = current_owners
        return owner_map

    @staticmethod
    def extract_owners(line):
        """Extracts owner email addresses listed on a line."""
        match = re.match(r'##? Owners?: (?P<addresses>.*)', line)
        if not match or not match.group('addresses'):
            return None
        email_part = match.group('addresses')
        addresses = [email_part] if ',' not in email_part else re.split(r',\s*', email_part)
        addresses = [s for s in addresses if re.match(r'\S+@\S+', s)]
        return addresses or None

    @staticmethod
    def extract_directory(line):
        """Extracts the directory substring for an enabled directory."""
        match = re.match(r'# ?(?P<directory>\S+) \[ (Pass|Skip) \]', line)
        if match and match.group('directory'):
            return match.group('directory')
        match = re.match(r'(?P<directory>\S+) \[ Pass \]', line)
        if match and match.group('directory'):
            return match.group('directory')
        return None

    def list_owners(self, changed_files):
        """Looks up the owners for the given set of changed files.

        Args:
            changed_files: A list of file paths relative to the repository root.

        Returns:
            A dict mapping tuples of owner email addresses to lists of
            owned directories.
        """
        tests = [self.finder.layout_test_name(path) for path in changed_files]
        tests = [t for t in tests if t is not None]
        email_map = collections.defaultdict(list)
        for directory, owners in self.owner_map.iteritems():
            owned_tests = [t for t in tests if t.startswith(directory)]
            if not owned_tests:
                continue
            email_map[tuple(owners)].append(directory)
        return email_map
