#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    # Same module occurs in both the top-level third_party and others.
    os.path.join('base','third_party','icu'),

    # Assume for now that breakpad has their licensing in order.
    os.path.join('breakpad'),

    # This is just a tiny vsprops file, presumably written by the googleurl
    # authors.  Not third-party code.
    os.path.join('googleurl','third_party','icu'),

    # Assume for now that native client has their licensing in order.
    os.path.join('native_client'),

    # Same module occurs in chrome/ and in net/, so skip one of them.
    os.path.join('net','third_party','mozilla_security_manager'),

    # Same module occurs in base/, net/, and src/ so skip all but one of them.
    os.path.join('third_party','nss'),
    os.path.join('net','third_party','nss'),

    # We don't bundle o3d samples into our resulting binaries.
    os.path.join('o3d','samples'),

    # Not in the public Chromium tree.
    os.path.join('third_party','adobe'),

    # Written as part of Chromium.
    os.path.join('third_party','fuzzymatch'),

    # Same license as Chromium.
    os.path.join('third_party','lss'),

    # Only binaries, used during development.
    os.path.join('third_party','valgrind'),

    # Two directories that are the same as those in base/third_party.
    os.path.join('v8','src','third_party','dtoa'),
    os.path.join('v8','src','third_party','valgrind'),

    # Used for development and test, not in the shipping product.
    os.path.join('third_party','bidichecker'),
    os.path.join('third_party','cygwin'),
    os.path.join('third_party','lighttpd'),
    os.path.join('third_party','mingw-w64'),
    os.path.join('third_party','pefile'),
    os.path.join('third_party','python_26'),

    # Stuff pulled in from chrome-internal for official builds/tools.
    os.path.join('third_party', 'clear_cache'),
    os.path.join('third_party', 'gnu'),
    os.path.join('third_party', 'googlemac'),
    os.path.join('third_party', 'pcre'),
    os.path.join('third_party', 'psutils'),
    os.path.join('third_party', 'sawbuck'),

    # Redistribution does not require attribution in documentation.
    os.path.join('third_party','directxsdk'),
    os.path.join('third_party','platformsdk_win2008_6_1'),
    os.path.join('third_party','platformsdk_win7'),

    # Harfbuzz-ng is not currently shipping in any product:
    os.path.join('third_party','harfbuzz-ng'),
])

# Directories we don't scan through.
PRUNE_DIRS = ('.svn', '.git',             # VCS metadata
              'out', 'Debug', 'Release',  # build files
              'layout_tests')             # lots of subdirs

ADDITIONAL_PATHS = (
    # The directory with the word list for Chinese and Japanese segmentation
    # with different license terms than ICU.
    os.path.join('third_party','icu','source','data','brkitr'),
)


# Directories where we check out directly from upstream, and therefore
# can't provide a README.chromium.  Please prefer a README.chromium
# wherever possible.
SPECIAL_CASES = {
    os.path.join('third_party', 'angle'): {
        "Name": "Almost Native Graphics Layer Engine",
        "URL": "http://code.google.com/p/angleproject/",
    },
    os.path.join('third_party', 'lss'): {
        "Name": "linux-syscall-support",
        "URL": "http://code.google.com/p/lss/",
    },
    os.path.join('third_party', 'ots'): {
        "Name": "OTS (OpenType Sanitizer)",
        "URL": "http://code.google.com/p/ots/",
    },
    os.path.join('third_party', 'ppapi'): {
        "Name": "ppapi",
        "URL": "http://code.google.com/p/ppapi/",
    },
    os.path.join('third_party', 'pywebsocket'): {
        "Name": "pywebsocket",
        "URL": "http://code.google.com/p/pywebsocket/",
    },
    os.path.join('third_party', 'WebKit'): {
        "Name": "WebKit",
        "URL": "http://webkit.org/",
        # Absolute path here is resolved as relative to the source root.
        "License File": "/webkit/LICENSE",
    },
    os.path.join('third_party', 'GTM'): {
        "Name": "Google Toolbox for Mac",
        "URL": "http://code.google.com/p/google-toolbox-for-mac/",
        "License File": "COPYING",
    },
    os.path.join('third_party', 'pdfsqueeze'): {
        "Name": "pdfsqueeze",
        "URL": "http://code.google.com/p/pdfsqueeze/",
        "License File": "COPYING",
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

    for dir in ADDITIONAL_PATHS:
        third_party_dirs.append(dir)

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


def main():
    command = 'help'
    if len(sys.argv) > 1:
        command = sys.argv[1]

    if command == 'scan':
        if not ScanThirdPartyDirs():
            return 1
    elif command == 'credits':
        if not GenerateCredits():
            return 1
    else:
        print __doc__
        return 1


if __name__ == '__main__':
  sys.exit(main())
