# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for merging layout tests results directories together.

This is split into three parts:

 * Generic code to merge JSON data together.
 * Generic code to merge directories together.
 * Code to specifically merge the layout tests result data together.

The JSON data merger will recursively merge dictionaries by default.

 * Helper functions can be provided to do more complex merging.
 * Helper are called when a given Match object returns true for a given key or
   value.

The directory merger will recursively merge the contents of directories.

 * Helper functions can be provided to deal with merging specific file objects.
 * Helper functions are called when a given Match object returns true for the
   filenames.
 * The default helper functions only merge if file contents match or the file
   only exists in one input directory.

The quickest way to understand how the mergers, helper functions and match
objects work together is to look at the unit tests.
"""

import json
import logging
import pprint
import re
import types

from webkitpy.common.system.filesystem import FileSystem


_log = logging.getLogger(__name__)


# Classes for recursively merging a JSON like dictionary together.
# ------------------------------------------------------------------------


def join_name(prefix, name):
    return "%s:%s" % (prefix, name)


class Match(object):
    """Base class for matching objects."""

    def __call__(self, obj, name=None):
        return False


class TypeMatch(Match):
    """Match based on instance of given types."""

    def __init__(self, *match_types):
        self.types = match_types

    def __call__(self, obj, name=None):
        return isinstance(obj, self.types)


class NameMatch(Match):
    """Match based on regex being found in name.

    Use start line (^) and end of line ($) anchors if you want to match on
    exact name.
    """

    def __init__(self, regex):
        self.regex = re.compile(regex)

    def __call__(self, obj, name=None):
        if name is None:
            return False
        return self.regex.search(name) is not None


class ValueMatch(Match):
    """Match based on equaling a given value."""

    def __init__(self, value):
        self.value = value

    def __call__(self, obj, name=None):
        return obj == self.value


class MergeFailure(Exception):
    """Base exception for merge failing."""

    def __init__(self, msg, name, obj_a, obj_b):
        emsg = (
            "Failure merging {name}: "
            " {msg}\nTrying to merge {a} and {b}."
        ).format(
            name=name,
            msg=msg,
            a=obj_a,
            b=obj_b,
        )
        Exception.__init__(self, emsg)

    @classmethod
    def assert_type_eq(cls, name, obj_a, obj_b):
        if type(obj_a) != type(obj_b):
            raise cls("Types don't match", name, obj_a, obj_b)


class Merger(object):
    """Base class for merger objects."""

    def __init__(self):
        self.helpers = []

    def add_helper(self, match_func, merge_func):
        """Add function which merges values.

        match_func and merge_func are dependent on the merger object type.
        When the function returns true, the merge_func will be called.

        Helpers are searched in last added, first checked order. This allows
        more specific helpers to be added after more generic helpers.
        """
        self.helpers.append((match_func, merge_func))


class JSONMerger(Merger):
    """Merge two JSON-like objects.

    For adding helpers;

        match_func is a function of form
            def f(obj, name=None) -> bool
        When the function returns true, the merge_func will be called.

        merge_func is a function of the form
            def f(obj_a, obj_b, name=None) -> obj_merged
        Merge functions should *never* modify the input arguments.
    """

    def __init__(self):
        Merger.__init__(self)

        self.add_helper(
            TypeMatch(types.ListType, types.TupleType), self.merge_listlike)
        self.add_helper(
            TypeMatch(types.DictType), self.merge_dictlike)

    def fallback_matcher(self, obj_a, obj_b, name=None):
        raise MergeFailure(
            "No merge helper found!", name, obj_a, obj_b)

    def merge_equal(self, obj_a, obj_b, name=None):
        """Merge two equal objects together."""
        if obj_a != obj_b:
            raise MergeFailure(
                "Unable to merge!", name, obj_a, obj_b)
        return obj_a

    def merge_listlike(self, list_a, list_b, name=None):  # pylint: disable=unused-argument
        """Merge two things which are "list like" (tuples, lists, sets)."""
        assert type(list_a) == type(list_b), (
            "Types of %r and %r don't match, refusing to merge." % (
                list_a, list_b))
        output = list(list_a)
        output.extend(list_b)
        return list_a.__class__(output)

    def merge_dictlike(self, dict_a, dict_b, name=None):
        """Merge two things which are dictionaries."""
        assert type(dict_a) == type(dict_b), (
            "Types of %r and %r don't match, refusing to merge." % (
                dict_a, dict_b))
        dict_out = dict_a.__class__({})
        for key in dict_a.keys() + dict_b.keys():
            if key in dict_a and key in dict_b:
                dict_out[key] = self.merge(
                    dict_a[key], dict_b[key],
                    name=join_name(name, key))
            elif key in dict_a:
                dict_out[key] = dict_a[key]
            elif key in dict_b:
                dict_out[key] = dict_b[key]
            else:
                assert False
        return dict_out

    def merge(self, obj_a, obj_b, name=""):
        """Generic merge function.

        name is a string representing the current key value separated by
        semicolons. For example, if file.json had the following;

            {'key1': {'key2': 3}}

        Then the name of the value 3 is 'file.json:key1:key2'
        """
        if obj_a is None and obj_b is None:
            return None
        elif obj_b is None:
            return obj_a
        elif obj_a is None:
            return obj_b

        MergeFailure.assert_type_eq(name, obj_a, obj_b)

        # Try the merge helpers.
        for match_func, merge_func in reversed(self.helpers):
            if match_func(obj_a, name):
                return merge_func(obj_a, obj_b, name=name)
            if match_func(obj_b, name):
                return merge_func(obj_a, obj_b, name=name)

        return self.fallback_matcher(obj_a, obj_b, name=name)


# Classes for recursively merging a directory together.
# ------------------------------------------------------------------------


class FilenameMatch(object):
    """Match based on name matching a regex."""

    def __init__(self, regex):
        self.regex = re.compile(regex)

    def __call__(self, filename, to_merge):
        return self.regex.search(filename) is not None

    def __str__(self):
        return "FilenameMatch(%r)" % self.regex.pattern

    __repr__ = __str__


class MergeFiles(object):
    """Base class for things which merge files."""

    def __init__(self, filesystem):
        assert filesystem
        self.filesystem = filesystem

    def __call__(self, out_filename, to_merge):
        raise NotImplementedError()


class MergeFilesOne(MergeFiles):
    """Dummy function which 'merges' a single file into output."""

    def __call__(self, out_filename, to_merge):
        assert len(to_merge) == 1
        self.filesystem.copyfile(to_merge[0], out_filename)


class MergeFilesMatchingContents(MergeFiles):
    """Merge if the contents of each files given matches exactly."""

    def __call__(self, out_filename, to_merge):
        data = self.filesystem.read_binary_file(to_merge[0])

        nonmatching = []
        for filename in to_merge[1:]:
            other_data = self.filesystem.read_binary_file(filename)
            if data != other_data:
                nonmatching.append(filename)

        if nonmatching:
            raise MergeFailure(
                '\n'.join(
                    ['File contents don\'t match:'] + nonmatching),
                out_filename,
                to_merge[0], to_merge[1:])

        self.filesystem.write_binary_file(out_filename, data)


class MergeFilesLinesSorted(MergeFiles):
    """Merge and sort the files of the given files."""

    def __call__(self, out_filename, to_merge):
        lines = []
        for filename in to_merge:
            with self.filesystem.open_text_file_for_reading(filename) as f:
                lines.extend(f.readlines())
        lines.sort()
        with self.filesystem.open_text_file_for_writing(out_filename) as f:
            f.writelines(lines)


class MergeFilesKeepFiles(MergeFiles):
    """Merge by copying each file appending a number to filename."""

    def __call__(self, out_filename, to_merge):
        for i, filename in enumerate(to_merge):
            self.filesystem.copyfile(filename, "%s_%i" % (out_filename, i))


class MergeFilesJSONP(MergeFiles):
    """Merge JSONP (and JSON) files.

    filesystem:
        filesystem.FileSystem object.

    json_data_merger:
        JSONMerger object used for merging the JSON data inside the files.

    json_value_data_overrides:
        Dictionary of {'key': 'value'} values to override in the resulting
        output.
    """

    def __init__(self, filesystem, json_data_merger=None, json_data_value_overrides=None):
        MergeFiles.__init__(self, filesystem)
        self._json_data_merger = json_data_merger or JSONMerger()
        self._json_data_value_overrides = json_data_value_overrides or {}

    def __call__(self, out_filename, to_merge):
        try:
            before_a, output_data, after_a = self.load_jsonp(
                self.filesystem.open_binary_file_for_reading(to_merge[0]))
        except ValueError as e:
            raise MergeFailure(e.message, to_merge[0], None, None)

        for filename in to_merge[1:]:
            try:
                before_b, new_json_data, after_b = self.load_jsonp(
                    self.filesystem.open_binary_file_for_reading(filename))
            except ValueError as e:
                raise MergeFailure(e.message, filename, None, None)

            if before_a != before_b:
                raise MergeFailure(
                    "jsonp starting data from %s doesn't match." % filename,
                    out_filename,
                    before_a, before_b)

            if after_a != after_b:
                raise MergeFailure(
                    "jsonp ending data from %s doesn't match." % filename,
                    out_filename,
                    after_a, after_b)

            output_data = self._json_data_merger.merge(output_data, new_json_data, filename)

        output_data.update(self._json_data_value_overrides)

        self.dump_jsonp(
            self.filesystem.open_binary_file_for_writing(out_filename),
            before_a, output_data, after_a)

    @staticmethod
    def load_jsonp(fd):
        """Load a JSONP file and return the JSON data parsed.

        JSONP files have a JSON data structure wrapped in a function call or
        other non-JSON data.
        """
        in_data = fd.read()

        begin = in_data.find('{')
        end = in_data.rfind('}') + 1

        before = in_data[:begin]
        data = in_data[begin:end]
        after = in_data[end:]

        # If just a JSON file, use json.load to get better error message output.
        if before == '' and after == '':
            fd.seek(0)
            json_data = json.load(fd)
        else:
            json_data = json.loads(data)

        return before, json_data, after

    @staticmethod
    def dump_jsonp(fd, before, json_data, after):
        """Write a JSONP file.

        JSONP files have a JSON data structure wrapped in a function call or
        other non-JSON data.
        """
        fd.write(before)
        fd.write(
            re.subn(
                '\\s+\n', '\n',
                json.dumps(json_data, indent=2, sort_keys=True))[0])
        fd.write(after)


class DeferredPrettyPrint(object):
    """Defer pretty print generation until it actually is getting produced.

    Needed so that we don't do this work if the logging statement is disabled.
    """
    def __init__(self, *args, **kw):
        self.args = args
        self.kw = kw

    def __str__(self):
        return pprint.pformat(*self.args, **self.kw)


class DirMerger(Merger):
    """Merge directory of files.

    For adding helpers;
        match_func is a function of form
            def f(output filename, list(input filepaths to merge)) -> bool

        merge_func is a function of the form
            def f(output filename, list(input filepaths to merge))
    """

    def __init__(self, filesystem=None):
        Merger.__init__(self)

        self.filesystem = filesystem or FileSystem()

        # Default to just checking the file contents matches.
        self.add_helper(lambda *args: True, MergeFilesMatchingContents(self.filesystem))
        # Copy the file it it's the only one.
        self.add_helper(lambda _, to_merge: len(to_merge) == 1, MergeFilesOne(self.filesystem))

    def merge(self, output_dir, to_merge_dirs):
        output_dir = self.filesystem.realpath(self.filesystem.abspath(output_dir))

        merge_dirs = []
        # Normalize the given directory values.
        for base_dir in to_merge_dirs:
            merge_dirs.append(self.filesystem.realpath(self.filesystem.abspath(base_dir)))
        merge_dirs.sort()

        _log.debug("Merging following paths:")
        _log.debug(DeferredPrettyPrint(merge_dirs))

        # Walk all directories and create a list of files found in any. The
        # result will look like;
        # ----
        # files = {
        #    'path to file common between shards': [
        #        'directory to shard A which contains file',
        #        'directory to shard B which contains file',
        #        ...],
        #    ...}
        # ----
        files = {}
        for base_dir in merge_dirs:
            for dir_path, _, filenames in self.filesystem.walk(base_dir):
                assert dir_path.startswith(base_dir)
                for f in filenames:
                    # rel_file is the path of f relative to the base directory
                    rel_file = self.filesystem.join(dir_path, f)[len(base_dir) + 1:]
                    files.setdefault(rel_file, []).append(base_dir)

        # Go through each file and try to merge it.
        # partial_file_path is the file relative to the directories.
        for partial_file_path, in_dirs in sorted(files.iteritems()):
            out_path = self.filesystem.join(output_dir, partial_file_path)
            if self.filesystem.exists(out_path):
                raise MergeFailure(
                    'File %s already exist in output.', out_path, None, None)

            dirname = self.filesystem.dirname(out_path)
            if not self.filesystem.exists(dirname):
                self.filesystem.maybe_make_directory(dirname)

            to_merge = [self.filesystem.join(d, partial_file_path) for d in in_dirs]

            _log.debug("Creating merged %s from %s", out_path, to_merge)

            for match_func, merge_func in reversed(self.helpers):

                if not match_func(partial_file_path, to_merge):
                    continue

                merge_func(out_path, to_merge)
                break


# Classes specific to merging LayoutTest results directory.
# ------------------------------------------------------------------------


class JSONTestResultsMerger(JSONMerger):
    """Merger for the 'json test result' format.

    The JSON format is described at
    https://dev.chromium.org/developers/the-json-test-results-format

    allow_unknown_if_matching:
        Allow unknown keys found in multiple files if the value matches in all.
    """
    def __init__(self, allow_unknown_if_matching=False):
        JSONMerger.__init__(self)

        self.allow_unknown_if_matching = allow_unknown_if_matching

        # Most of the JSON file can be merged by the default merger but we need
        # some extra helpers for some special fields.

        # These keys are constants which should be the same across all the shards.
        matching = [
            ':builder_name$',
            ':build_number$',
            ':chromium_revision$',
            ':has_pretty_patch$',
            ':has_wdiff$',
            ':path_delimiter$',
            ':pixel_tests_enabled$',
            ':random_order_seed$',
            ':version$',
        ]
        for match_name in matching:
            self.add_helper(
                NameMatch(match_name),
                self.merge_equal)

        # These keys are accumulated sums we want to add together.
        addable = [
            ':fixable$',
            ':num_flaky$',
            ':num_passes$',
            ':num_regressions$',
            ':skipped$',
            ':skips$',
            # All keys inside the num_failures_by_type entry.
            ':num_failures_by_type:',
        ]
        for match_name in addable:
            self.add_helper(
                NameMatch(match_name),
                lambda a, b, name=None: a + b)

        # If any shard is interrupted, mark the whole thing as interrupted.
        self.add_helper(
            NameMatch(':interrupted$'),
            lambda a, b, name=None: a or b)

        # Layout test directory value is randomly created on each shard, so
        # clear it.
        self.add_helper(
            NameMatch(':layout_tests_dir$'),
            lambda a, b, name=None: None)

        # seconds_since_epoch is the start time, so we just take the earliest.
        self.add_helper(
            NameMatch(':seconds_since_epoch$'),
            lambda a, b, name=None: min(a, b))

    def fallback_matcher(self, obj_a, obj_b, name=None):
        if self.allow_unknown_if_matching:
            result = self.merge_equal(obj_a, obj_b, name)
            _log.warning('Unknown value %s, accepting anyway as it matches.', name)
            return result
        return JSONMerger.fallback_matcher(self, obj_a, obj_b, name)


class LayoutTestDirMerger(DirMerger):
    """Merge layout test result directory."""

    def __init__(self, filesystem=None,
                 results_json_value_overrides=None,
                 results_json_allow_unknown_if_matching=False):
        DirMerger.__init__(self, filesystem)

        # JSON merger for non-"result style" JSON files.
        basic_json_data_merger = JSONMerger()
        basic_json_data_merger.fallback_matcher = basic_json_data_merger.merge_equal
        self.add_helper(
            FilenameMatch('\\.json'),
            MergeFilesJSONP(self.filesystem, basic_json_data_merger))

        # access_log and error_log are httpd log files which are sortable.
        self.add_helper(
            FilenameMatch('access_log\\.txt'),
            MergeFilesLinesSorted(self.filesystem))
        self.add_helper(
            FilenameMatch('error_log\\.txt'),
            MergeFilesLinesSorted(self.filesystem))

        # pywebsocket files aren't particularly useful, so just save them.
        self.add_helper(
            FilenameMatch('pywebsocket\\.ws\\.log-.*-err.txt'),
            MergeFilesKeepFiles(self.filesystem))

        # These JSON files have "result style" JSON in them.
        results_json_file_merger = MergeFilesJSONP(
            self.filesystem,
            JSONTestResultsMerger(
                allow_unknown_if_matching=results_json_allow_unknown_if_matching),
            json_data_value_overrides=results_json_value_overrides or {})

        self.add_helper(
            FilenameMatch('failing_results.json'),
            results_json_file_merger)
        self.add_helper(
            FilenameMatch('full_results.json'),
            results_json_file_merger)
        self.add_helper(
            FilenameMatch('output.json'),
            results_json_file_merger)
