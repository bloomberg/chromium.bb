# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import chromium_deps


class ChromiumDEPSTest(unittest.TestCase):
  DEPS_TEMPLATE = """
vars = {
  "googlecode_url": "http://%%s.googlecode.com/svn",
  "webkit_trunk": "http://src.chromium.org/blink/trunk",
  "webkit_revision": "%s",
  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  "src/breakpad/src":
    (Var("googlecode_url") %% "google-breakpad") + "/trunk/src@%s",

  "src/third_party/WebKit":
    Var("webkit_trunk") + "@" + Var("webkit_revision"),
}

deps_os = {
  "unix": {
    "src/third_party/liblouis/src":
      Var("chromium_git") +
      "/external/liblouis.git@%s",
  }
}
"""

  def __init__(self, *args, **kwargs):
    super(ChromiumDEPSTest, self).__init__(*args, **kwargs)

  def testGetChromiumComponents(self):
    chromium_revision = '283296'
    webkit_revision = '178200'
    breakpad_revision = '1345'
    liblouis_commit_hashcode = '3c2daee56250162e5a75830871601d74328d39f5'

    def _GetContentOfDEPS(chromium_revision_tmp):
      self.assertEqual(chromium_revision_tmp, chromium_revision)
      return self.DEPS_TEMPLATE % (webkit_revision, breakpad_revision,
                                   liblouis_commit_hashcode)

    expected_results = {
        'src/breakpad/src/': {
            'path': 'src/breakpad/src/',
            'repository_type': 'svn',
            'name': 'breakpad',
            'repository': 'http://google-breakpad.googlecode.com/svn/trunk/src',
            'revision': breakpad_revision
        },
        'src/third_party/liblouis/src/': {
            'path': 'src/third_party/liblouis/src/',
            'repository_type': 'git',
            'name': 'liblouis',
            'repository':
                 'https://chromium.googlesource.com/external/liblouis.git',
            'revision': liblouis_commit_hashcode
        },
        'src/': {
            'path': 'src/',
            'repository_type': 'svn',
            'name': 'chromium',
            'repository': 'https://src.chromium.org/chrome/trunk',
            'revision': chromium_revision
        },
        'src/third_party/WebKit/': {
            'path': 'src/third_party/WebKit/',
            'repository_type': 'svn',
            'name': 'blink',
            'repository': 'http://src.chromium.org/blink/trunk',
            'revision': webkit_revision
        }
    }

    components = chromium_deps.GetChromiumComponents(
        chromium_revision, deps_file_downloader=_GetContentOfDEPS)
    self.assertEqual(expected_results, components)

  def testGetChromiumComponentRange(self):
    chromium_revision1 = '283296'
    webkit_revision1 = '178200'
    breakpad_revision1 = '1345'
    liblouis_commit_hashcode1 = '3c2daee56250162e5a75830871601d74328d39f5'

    chromium_revision2 = '283200'
    webkit_revision2 = '178084'
    breakpad_revision2 = '1345'
    liblouis_commit_hashcode2 = '3c2daee56250162e5a75830871601d74328d39f5'

    def _GetContentOfDEPS(chromium_revision):
      chromium_revision = str(chromium_revision)
      if chromium_revision == chromium_revision1:
        return self.DEPS_TEMPLATE % (webkit_revision1, breakpad_revision1,
                                     liblouis_commit_hashcode1)
      else:
        self.assertEqual(chromium_revision2, chromium_revision)
        return self.DEPS_TEMPLATE % (webkit_revision2, breakpad_revision2,
                                     liblouis_commit_hashcode2)

    expected_results = {
        'src/breakpad/src/': {
            'old_revision': breakpad_revision2,
            'name': 'breakpad',
            'repository': 'http://google-breakpad.googlecode.com/svn/trunk/src',
            'rolled': False,
            'new_revision': breakpad_revision1,
            'path': 'src/breakpad/src/',
            'repository_type': 'svn'
        },
        'src/third_party/liblouis/src/': {
            'old_revision': liblouis_commit_hashcode2,
            'name': 'liblouis',
            'repository':
                'https://chromium.googlesource.com/external/liblouis.git',
            'rolled': False,
            'new_revision': liblouis_commit_hashcode1,
            'path': 'src/third_party/liblouis/src/',
            'repository_type': 'git'
        },
        'src/': {
            'old_revision': chromium_revision2,
            'name': 'chromium',
            'repository': 'https://src.chromium.org/chrome/trunk',
            'rolled': True,
            'new_revision': chromium_revision1,
            'path': 'src/',
            'repository_type': 'svn'
        },
        'src/third_party/WebKit/': {
            'old_revision': webkit_revision2,
            'name': 'blink',
            'repository': 'http://src.chromium.org/blink/trunk',
            'rolled': True,
            'new_revision': webkit_revision1,
            'path': 'src/third_party/WebKit/',
            'repository_type': 'svn'
        }
    }

    components = chromium_deps.GetChromiumComponentRange(
        chromium_revision1, chromium_revision2,
        deps_file_downloader=_GetContentOfDEPS)
    self.assertEqual(expected_results, components)
