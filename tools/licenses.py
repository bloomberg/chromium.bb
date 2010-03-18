#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Utilities for checking and processing licensing information in third_party
directories.
"""

import os


class LicenseError(Exception):
    """We raise this exception when a directory's licensing info isn't
    fully filled out."""
    pass


def ParseDir(path):
    """Examine a third_party/foo component and extract its metadata."""

    # Try to find README.chromium.
    readme_path = os.path.join(path, 'README.chromium')
    if not os.path.exists(readme_path):
        raise LicenseError("missing README.chromium")

    # Parse metadata fields out of README.chromium.
    # We provide a default value of "LICENSE" for the license file.
    metadata = {
        "License File": "LICENSE",  # Relative path to license text.
        "Name": None,               # Short name (for header on about:credits).
        "URL": None,                # Project home page.
        }
    for line in open(readme_path):
        line = line.strip()
        for key in metadata.keys():
            field = key + ": "
            if line.startswith(field):
                metadata[key] = line[len(field):]

    # Check that all expected metadata is present.
    for key, value in metadata.iteritems():
        if not value:
            raise LicenseError("couldn't find '" + key + "' line "
                               "in README.chromium")

    # Check that the license file exists.
    license_file = metadata["License File"]
    license_path = os.path.join(path, license_file)
    if not os.path.exists(license_path):
        raise LicenseError("License file '" + license_file + "' doesn't exist. "
                           "Either add a 'License File:' section to "
                           "README.chromium or add the missing file.")

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

    for path, error in sorted(errors):
        print path + ": " + error


def FindThirdPartyDirs():
    """Find all third_party directories underneath the current directory."""
    skip_dirs = ('.svn', '.git',             # VCS metadata
                 'out', 'Debug', 'Release',  # build files
                 'layout_tests')             # lots of subdirs

    third_party_dirs = []
    for path, dirs, files in os.walk('.'):
        path = path[len('./'):]  # Pretty up the path.

        # Prune out directories we want to skip.
        for skip in skip_dirs:
            if skip in dirs:
                dirs.remove(skip)

        if os.path.basename(path) == 'third_party':
            third_party_dirs.extend([os.path.join(path, dir) for dir in dirs])
            # Don't recurse into any subdirs from here.
            dirs[:] = []
            continue

    return third_party_dirs


if __name__ == '__main__':
    third_party_dirs = FindThirdPartyDirs()
    ScanThirdPartyDirs(third_party_dirs)
