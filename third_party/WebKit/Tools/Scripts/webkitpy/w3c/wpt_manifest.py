# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPTManifest is responsible for handling MANIFEST.json.

The MANIFEST.json file contains metadata about files in web-platform-tests,
such as what tests exist, and extra information about each test, including
test type, options, URLs to use, and reference file paths if applicable.
"""

import json
import logging

from webkitpy.common.path_finder import PathFinder
from webkitpy.common.memoized import memoized

_log = logging.getLogger(__file__)


class WPTManifest(object):

    def __init__(self, json_content):
        # TODO(tkent): Create a Manifest object by Manifest.from_json().
        # See ../thirdparty/wpt/wpt/tools/manifest/manifest.py.
        self.raw_dict = json.loads(json_content)
        self.test_types = ('manual', 'reftest', 'testharness')

    def _items_for_path(self, path_in_wpt):
        """Returns manifest items for the given WPT path, or None if not found.

        The format of a manifest item depends on:
          https://github.com/w3c/web-platform-tests/blob/master/tools/manifest/item.py

        For most testharness tests, the returned items is expected
        to look like this: [["/some/test/path.html", {}]]. For reference tests,
        it will be a list with three items ([url, references, extras]).
        """
        items = self.raw_dict['items']
        for test_type in self.test_types:
            if path_in_wpt in items[test_type]:
                return items[test_type][path_in_wpt]
        return None

    @memoized
    def all_urls(self):
        """Returns a set of the urls for all items in the manifest."""
        urls = set()
        if 'items' in self.raw_dict:
            items = self.raw_dict['items']
            for category in self.test_types:
                if category in items:
                    for records in items[category].values():
                        urls.update([item[0] for item in records])
        return urls

    @memoized
    def all_wpt_tests(self):
        # TODO(qyearsley): Rename this method to indicate that it
        # returns wpt test file paths, which may not be "test names".
        if 'items' not in self.raw_dict:
            return []

        all_tests = []
        for test_type in self.test_types:
            for path_in_wpt in self.raw_dict['items'][test_type]:
                all_tests.append(path_in_wpt)
        return all_tests

    def is_test_file(self, path_in_wpt):
        return self._items_for_path(path_in_wpt) is not None

    def is_test_url(self, url):
        """Checks if url is a valid test in the manifest.

        The url must be the WPT test name with a leading slash (/).
        """
        if url[0] != '/':
            raise Exception('Test url missing leading /: %s' % url)
        return url in self.all_urls()

    def file_path_to_url_paths(self, path_in_wpt):
        manifest_items = self._items_for_path(path_in_wpt)
        assert manifest_items is not None
        return [item[0][1:] for item in manifest_items]

    @staticmethod
    def _get_extras_from_item(item):
        return item[-1]

    def is_slow_test(self, test_name):
        items = self._items_for_path(test_name)
        if not items:
            return False
        extras = WPTManifest._get_extras_from_item(items[0])
        return 'timeout' in extras and extras['timeout'] == 'long'

    def extract_reference_list(self, path_in_wpt):
        """Extracts reference information of the specified reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        pairs, like:
           [("==", "foo/bar/baz-match.html"),
            ("!=", "foo/bar/baz-mismatch.html")]
        """
        all_items = self.raw_dict['items']
        if path_in_wpt not in all_items['reftest']:
            return []
        reftest_list = []
        for item in all_items['reftest'][path_in_wpt]:
            for ref_path_in_wpt, expectation in item[1]:
                reftest_list.append((expectation, ref_path_in_wpt))
        return reftest_list

    @staticmethod
    def ensure_manifest(host):
        """Generates the MANIFEST.json file if it does not exist."""
        finder = PathFinder(host.filesystem)
        manifest_path = finder.path_from_layout_tests('external', 'wpt', 'MANIFEST.json')
        base_manifest_path = finder.path_from_layout_tests('external', 'WPT_BASE_MANIFEST.json')

        if not host.filesystem.exists(base_manifest_path):
            _log.error('Manifest base not found at "%s".', base_manifest_path)
            host.filesystem.write_text_file(base_manifest_path, '{}')

        if not host.filesystem.exists(manifest_path):
            _log.debug('Manifest not found, copying from base "%s".', base_manifest_path)
            host.filesystem.copyfile(base_manifest_path, manifest_path)

        wpt_path = manifest_path = finder.path_from_layout_tests('external', 'wpt')
        WPTManifest.generate_manifest(host, wpt_path)

        # Adding this log line to diagnose https://crbug.com/714503
        _log.debug('Manifest generation completed.')

    @staticmethod
    def generate_manifest(host, dest_path):
        """Generates MANIFEST.json on the specified directory."""
        executive = host.executive
        finder = PathFinder(host.filesystem)
        manifest_exec_path = finder.path_from_tools_scripts('webkitpy', 'thirdparty', 'wpt', 'wpt', 'manifest')

        cmd = ['python', manifest_exec_path, '--work', '--tests-root', dest_path]
        _log.debug('Running command: %s', ' '.join(cmd))
        proc = executive.popen(cmd, stdout=executive.PIPE, stderr=executive.PIPE, stdin=executive.PIPE)
        out, err = proc.communicate('')
        if proc.returncode:
            _log.info('# ret> %d', proc.returncode)
            if out:
                _log.info(out)
            if err:
                _log.info(err)
            host.exit(proc.returncode)
        return proc.returncode, out
