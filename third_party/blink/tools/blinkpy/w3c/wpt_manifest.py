# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPTManifest is responsible for handling MANIFEST.json.

The MANIFEST.json file contains metadata about files in web-platform-tests,
such as what tests exist, and extra information about each test, including
test type, options, URLs to use, and reference file paths if applicable.

Naming conventions:
* A (file) path is a relative file system path from the root of WPT.
* A (test) URL is the path (with an optional query string) to the test on
  wptserve relative to url_base.
Neither has a leading slash.
"""

import json
import logging

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

_log = logging.getLogger(__file__)

# The default filename of manifest expected by `wpt`.
MANIFEST_NAME = 'MANIFEST.json'
# The filename used for the base manifest includes the version as a
# workaround for trouble landing huge changes to the base manifest when
# the version changes. See https://crbug.com/876717.
BASE_MANIFEST_NAME = 'WPT_BASE_MANIFEST_6.json'

# TODO(robertma): Use the official wpt.manifest module.


class WPTManifest(object):
    """A simple abstraction of WPT MANIFEST.json.

    The high-level structure of the manifest is as follows:
        {
            "items": {
                "manual": {
                    "file/path": [manifest items],
                    ...
                },
                "reftest": {...},
                "testharness": {...}
            },
            // other info...
        }

    The format of a manifest item depends on:
        https://github.com/web-platform-tests/wpt/blob/master/tools/manifest/item.py
    which can be roughly summarized as follows:
        * testharness test: [url, extras]
        * reftest: [url, references, extras]
    where `extras` is a dict with the following optional items:
        * testharness test: {"timeout": "long", "testdriver": True}
        * reftest: {"timeout": "long", "viewport_size": ..., "dpi": ...}
    and `references` is a list that looks like:
        [[reference_url1, "=="], [reference_url2, "!="], ...]
    """

    def __init__(self, json_content):
        self.raw_dict = json.loads(json_content)
        self.test_types = ('manual', 'reftest', 'testharness')

    def _items_for_file_path(self, path_in_wpt):
        """Finds manifest items for the given WPT path.

        Args:
            path_in_wpt: A file path relative to the root of WPT.

        Returns:
            A list of manifest items, or None if not found.
        """
        items = self.raw_dict['items']
        for test_type in self.test_types:
            if test_type not in items:
                continue
            if path_in_wpt in items[test_type]:
                return items[test_type][path_in_wpt]
        return None

    def _item_for_url(self, url):
        """Finds the manifest item for the given WPT URL.

        Args:
            url: A WPT URL.

        Returns:
            A manifest item, or None if not found.
        """
        return self.all_url_items().get(url)

    @staticmethod
    def _get_url_from_item(item):
        return item[0]

    @staticmethod
    def _get_extras_from_item(item):
        return item[-1]

    @staticmethod
    def _is_not_jsshell(item):
        """Returns True if the manifest item isn't a jsshell test.

        "jsshell" is one of the scopes automatically generated from .any.js
        tests. It is intended to run in a thin JavaScript shell instead of a
        full browser, so we simply ignore it in web tests. (crbug.com/871950)
        """
        extras = WPTManifest._get_extras_from_item(item)
        return not extras.get('jsshell', False)

    @memoized
    def all_url_items(self):
        """Returns a dict mapping every URL in the manifest to its item."""
        url_items = {}
        if 'items' not in self.raw_dict:
            return url_items
        items = self.raw_dict['items']
        for test_type in self.test_types:
            if test_type not in items:
                continue
            for records in items[test_type].itervalues():
                for item in filter(self._is_not_jsshell, records):
                    url_items[self._get_url_from_item(item)] = item
        return url_items

    @memoized
    def all_urls(self):
        """Returns a set of the URLs for all items in the manifest."""
        return frozenset(self.all_url_items().keys())

    def is_test_file(self, path_in_wpt):
        """Checks if path_in_wpt is a test file according to the manifest."""
        assert not path_in_wpt.startswith('/')
        return self._items_for_file_path(path_in_wpt) is not None

    def is_test_url(self, url):
        """Checks if url is a valid test in the manifest."""
        assert not url.startswith('/')
        return url in self.all_urls()

    def is_slow_test(self, url):
        """Checks if a WPT is slow (long timeout) according to the manifest.

        Args:
            url: A WPT URL.

        Returns:
            True if the test is found and is slow, False otherwise.
        """
        if not self.is_test_url(url):
            return False

        item = self._item_for_url(url)
        if not item:
            return False
        extras = self._get_extras_from_item(item)
        return extras.get('timeout') == 'long'

    def extract_reference_list(self, path_in_wpt):
        """Extracts reference information of the specified reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        pairs, like:
           [("==", "/foo/bar/baz-match.html"),
            ("!=", "/foo/bar/baz-mismatch.html")]
        """
        items = self.raw_dict['items']
        if path_in_wpt not in items.get('reftest', {}):
            return []
        reftest_list = []
        for item in items['reftest'][path_in_wpt]:
            for ref_path_in_wpt, expectation in item[1]:
                # Ref URLs in MANIFEST should be absolute, but we double check
                # just in case.
                if not ref_path_in_wpt.startswith('/'):
                    ref_path_in_wpt = '/' + ref_path_in_wpt
                reftest_list.append((expectation, ref_path_in_wpt))
        return reftest_list

    @staticmethod
    def ensure_manifest(host, path=None):
        """Updates the MANIFEST.json file, or generates if it does not exist.

        Args:
            path: The path to a WPT root (relative to web_tests, optional).
        """
        if path is None:
            path = host.filesystem.join('external', 'wpt')
        finder = PathFinder(host.filesystem)
        wpt_path = finder.path_from_web_tests(path)
        manifest_path = host.filesystem.join(wpt_path, MANIFEST_NAME)

        # TODO(crbug.com/853815): perhaps also cache the manifest for wpt_internal.
        if 'external' in path:
            base_manifest_path = finder.path_from_web_tests('external', BASE_MANIFEST_NAME)
            if not host.filesystem.exists(base_manifest_path):
                _log.error('Manifest base not found at "%s".', base_manifest_path)
                host.filesystem.write_text_file(base_manifest_path, '{}')

            # Unconditionally replace MANIFEST.json with the base manifest even if
            # the former exists, to avoid regenerating the manifest from scratch
            # when the manifest version changes. Remove the destination first as
            # copyfile will fail if the two files are hardlinked or symlinked.
            if host.filesystem.exists(manifest_path):
                host.filesystem.remove(manifest_path)
            host.filesystem.copyfile(base_manifest_path, manifest_path)

        WPTManifest.generate_manifest(host, wpt_path)

        _log.debug('Manifest generation completed.')

    @staticmethod
    def generate_manifest(host, dest_path):
        """Generates MANIFEST.json on the specified directory."""
        finder = PathFinder(host.filesystem)
        wpt_exec_path = finder.path_from_blink_tools('blinkpy', 'third_party', 'wpt', 'wpt', 'wpt')
        cmd = ['python', wpt_exec_path, 'manifest', '--work', '--no-download', '--tests-root', dest_path]

        # ScriptError will be raised if the command fails.
        host.executive.run_command(
            cmd,
            return_stderr=True  # This will also include stderr in the exception message.
        )
