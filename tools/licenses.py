#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for checking and processing licensing information in third_party
directories.

Usage: licenses.py <command>

Commands:
  scan     scan third_party directories, verifying that we have licensing info
  credits  generate about:credits on stdout

(You can also import this as a module.)
"""

import cgi
import os
import sys

# Paths from the root of the tree to directories to skip.
PRUNE_PATHS = set([
    # Assume for now that breakpad has their licensing in order.
    "breakpad",

    # This is just a tiny vsprops file, presumably written by the googleurl
    # authors.  Not third-party code.
    "googleurl/third_party/icu",

    # Assume for now that native client has their licensing in order.
    "native_client",

    # We don't bundle o3d samples into our resulting binaries.
    "o3d/samples",

    # Not in the public Chromium tree.
    "third_party/adobe",

    # Written as part of Chromium.
    "third_party/fuzzymatch",

    # Two directories that are the same as those in base/third_party.
    "v8/src/third_party/dtoa",
    "v8/src/third_party/valgrind",

    # Same module occurs in base/ and in net/, so skip one of them.
    "net/third_party/nss",
])

# Directories we don't scan through.
PRUNE_DIRS = ('.svn', '.git',             # VCS metadata
              'out', 'Debug', 'Release',  # build files
              'layout_tests')             # lots of subdirs

# Directories where we check out directly from upstream, and therefore
# can't provide a README.chromium.  Please prefer a README.chromium
# wherever possible.
SPECIAL_CASES = {
    os.path.join('third_party', 'angle'): {
        "Name": "Almost Native Graphics Layer Engine",
        "URL": "http://code.google.com/p/angleproject/",
    },
    os.path.join('third_party', 'ots'): {
        "Name": "OTS (OpenType Sanitizer)",
        "URL": "http://code.google.com/p/ots/",
    },
    os.path.join('third_party', 'ppapi'): {
        "Name": "ppapi",
        "URL": "http://code.google.com/p/ppapi/",
    },
    os.path.join('third_party', 'protobuf2'): {
        "Name": "protobuf",
        "URL": "http://code.google.com/p/protobuf/",
    },
    os.path.join('third_party', 'pywebsocket'): {
        "Name": "pywebsocket",
        "URL": "http://code.google.com/p/pywebsocket/",
    },
    os.path.join('third_party', 'skia'): {
        "Name": "skia",
        "URL": "http://code.google.com/p/skia/",
    },
    os.path.join('third_party', 'WebKit'): {
        "Name": "WebKit",
        "URL": "http://webkit.org/",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/webkit/LICENSE",
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
        if filename.startswith('/'):
            # Absolute-looking paths are relative to the source root
            # (which is the directory we're run from).
            license_path = os.path.join(os.getcwd(), filename[1:])
        else:
            license_path = os.path.join(path, filename)
        if os.path.exists(license_path):
            metadata["License File"] = license_path
            break
        license_path = None

    if not license_path:
        raise LicenseError("License file not found. "
                           "Either add a file named LICENSE, "
                           "import upstream's COPYING if available, "
                           "or add a 'License File:' line to README.chromium "
                           "with the appropriate path.")

    return metadata


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

def ScanThirdPartyDirs():
    """Scan a list of directories and report on any problems we find."""
    third_party_dirs = FindThirdPartyDirs()

    errors = []
    for path in sorted(third_party_dirs):
        try:
            metadata = ParseDir(path)
        except LicenseError, e:
            errors.append((path, e.args[0]))
            continue

    for path, error in sorted(errors):
        print path + ": " + error

    return len(errors) == 0

def GenerateCredits():
    """Generate about:credits, dumping the result to stdout."""

    def EvaluateTemplate(template, env, escape=True):
        """Expand a template with variables like {{foo}} using a
        dictionary of expansions."""
        for key, val in env.items():
            if escape:
                val = cgi.escape(val)
            template = template.replace('{{%s}}' % key, val)
        return template

    third_party_dirs = FindThirdPartyDirs()

    entry_template = open('chrome/browser/resources/about_credits_entry.tmpl',
                          'rb').read()
    entries = []
    for path in sorted(third_party_dirs):
        try:
            metadata = ParseDir(path)
        except LicenseError:
            print >>sys.stderr, ("WARNING: licensing info for " + path +
                                 " is incomplete, skipping.")
            continue
        env = {
            'name': metadata['Name'],
            'url': metadata['URL'],
            'license': open(metadata['License File'], 'rb').read(),
        }
        entries.append(EvaluateTemplate(entry_template, env))

    file_template = open('chrome/browser/resources/about_credits.tmpl',
                         'rb').read()
    print "<!-- Generated by licenses.py; do not edit. -->"
    print EvaluateTemplate(file_template, {'entries': '\n'.join(entries)},
                           escape=False)

if __name__ == '__main__':
    command = 'help'
    if len(sys.argv) > 1:
        command = sys.argv[1]

    if command == 'scan':
        if not ScanThirdPartyDirs():
            sys.exit(1)
    elif command == 'credits':
        if not GenerateCredits():
            sys.exit(1)
    else:
        print __doc__
        sys.exit(1)
