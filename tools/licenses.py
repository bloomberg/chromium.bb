#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Utilities for checking and processing licensing information in third_party
directories.
"""

import os

# Paths from the root of the tree to directories to skip.
PRUNE_PATHS = set([
    # This is just a tiny vsprops file, presumably written by the googleurl
    # authors.  Not third-party code.
    "googleurl/third_party/icu",

    # We don't bundle o3d samples into our resulting binaries.
    "o3d/samples",

    # Written as part of Chromium.
    "third_party/fuzzymatch",

    # Two directories that are the same as those in base/third_party.
    "v8/src/third_party/dtoa",
    "v8/src/third_party/valgrind",
])

# Directories we don't scan through.
PRUNE_DIRS = ('.svn', '.git',             # VCS metadata
              'out', 'Debug', 'Release',  # build files
              'layout_tests')             # lots of subdirs

# Directories where we check out directly from upstream, and therefore
# can't provide a README.chromium.  Please prefer a README.chromium
# wherever possible.
SPECIAL_CASES = {
    'third_party/ots': {
        "Name": "OTS (OpenType Sanitizer)",
        "URL": "http://code.google.com/p/ots/",
    },
    'third_party/pywebsocket': {
        "Name": "pywebsocket",
        "URL": "http://code.google.com/p/pywebsocket/",
    },
}

class LicenseError(Exception):
    """We raise this exception when a directory's licensing info isn't
    fully filled out."""
    pass


def ParseDir(path):
    """Examine a third_party/foo component and extract its metadata."""

    # Parse metadata fields out of README.chromium.
    # We examine "LICENSE" for the license file by default.
    metadata = {
        "License File": "LICENSE",  # Relative path to license text.
        "Name": None,               # Short name (for header on about:credits).
        "URL": None,                # Project home page.
        }

    if path in SPECIAL_CASES:
        metadata.update(SPECIAL_CASES[path])
    else:
        # Try to find README.chromium.
        readme_path = os.path.join(path, 'README.chromium')
        if not os.path.exists(readme_path):
            raise LicenseError("missing README.chromium")

        for line in open(readme_path):
            line = line.strip()
            if not line:
                break
            for key in metadata.keys():
                field = key + ": "
                if line.startswith(field):
                    metadata[key] = line[len(field):]

    # Check that all expected metadata is present.
    for key, value in metadata.iteritems():
        if not value:
            raise LicenseError("couldn't find '" + key + "' line "
                               "in README.chromium or licences.py "
                               "SPECIAL_CASES")

    # Check that the license file exists.
    for filename in (metadata["License File"], "COPYING"):
        license_path = os.path.join(path, filename)
        if os.path.exists(license_path):
            metadata["License File"] = filename
            break
        license_path = None

    if not license_path:
        raise LicenseError("License file not found. "
                           "Either add a file named LICENSE, "
                           "import upstream's COPYING if available, "
                           "or add a 'License File:' line to README.chromium "
                           "with the appropriate path.")

    return metadata


def ScanThirdPartyDirs(third_party_dirs):
    """Scan a list of directories and report on any problems we find."""
    errors = []
    for path in sorted(third_party_dirs):
        try:
            metadata = ParseDir(path)
        except LicenseError, e:
            errors.append((path, e.args[0]))
            continue
        print path, "OK:", metadata["License File"]

    print

    for path, error in sorted(errors):
        print path + ": " + error


def FindThirdPartyDirs():
    """Find all third_party directories underneath the current directory."""
    third_party_dirs = []
    for path, dirs, files in os.walk('.'):
        path = path[len('./'):]  # Pretty up the path.

        if path in PRUNE_PATHS:
            dirs[:] = []
            continue

        # Prune out directories we want to skip.
        # (Note that we loop over PRUNE_DIRS so we're not iterating over a
        # list that we're simultaneously mutating.)
        for skip in PRUNE_DIRS:
            if skip in dirs:
                dirs.remove(skip)

        if os.path.basename(path) == 'third_party':
            # Add all subdirectories that are not marked for skipping.
            for dir in dirs:
                dirpath = os.path.join(path, dir)
                if dirpath not in PRUNE_PATHS:
                    third_party_dirs.append(dirpath)

            # Don't recurse into any subdirs from here.
            dirs[:] = []
            continue

    return third_party_dirs


if __name__ == '__main__':
    third_party_dirs = FindThirdPartyDirs()
    ScanThirdPartyDirs(third_party_dirs)
