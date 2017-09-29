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

import collections
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


class NameRegexMatch(Match):
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

    def __init__(self, msg, name, objs):
        emsg = (
            "Failure merging {name}: "
            " {msg}\nTrying to merge {objs}."
        ).format(
            name=name,
            msg=msg,
            objs=objs,
        )
        Exception.__init__(self, emsg)

    @classmethod
    def assert_type_eq(cls, name, objs):
        obj_0 = objs[0]
        for obj_n in objs[1:]:
            if type(obj_0) != type(obj_n):
                raise cls("Types don't match", name, (obj_0, obj_n))


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
    """Merge JSON-like objects.

    For adding helpers;

        match_func is a function of form
            def f(obj, name=None) -> bool
        When the function returns true, the merge_func will be called.

        merge_func is a function of the form
            def f(list_of_objs, name=None) -> obj_merged
        Merge functions should *never* modify the input arguments.
    """

    def __init__(self):
        Merger.__init__(self)

        self.add_helper(
            TypeMatch(types.ListType, types.TupleType), self.merge_listlike)
        self.add_helper(
            TypeMatch(types.DictType), self.merge_dictlike)

    def fallback_matcher(self, objs, name=None):
        raise MergeFailure(
            "No merge helper found!", name, objs)

    def merge_equal(self, objs, name=None):
        """Merge equal objects together."""
        obj_0 = objs[0]
        for obj_n in objs[1:]:
            if obj_0 != obj_n:
                raise MergeFailure(
                    "Unable to merge!", name, (obj_0, obj_n))
        return obj_0

    def merge_listlike(self, lists, name=None):  # pylint: disable=unused-argument
        """Merge things which are "list like" (tuples, lists, sets)."""
        MergeFailure.assert_type_eq(name, lists)
        output = list(lists[0])
        for list_n in lists[1:]:
            output.extend(list_n)
        return lists[0].__class__(output)

    def merge_dictlike(self, dicts, name=None, order_cls=collections.OrderedDict):
        """Merge things which are dictionaries.

        Args:
            dicts (list of dict): Dictionary like objects to merge (should all
                be the same type).
            name (str): Name of the objects being merged (used for error
                messages).
            order_cls: Dict like object class used to produce key ordering.
                Defaults to collections.OrderedDict which means all keys in
                dicts[0] come before all keys in dicts[1], etc.

        Returns:
            dict: Merged dictionary object of same type as the objects in
            dicts.
        """
        MergeFailure.assert_type_eq(name, dicts)

        dict_mid = order_cls()
        for dobj in dicts:
            for key in dobj:
                dict_mid.setdefault(key, []).append(dobj[key])

        dict_out = dicts[0].__class__({})
        for k, v in dict_mid.iteritems():
            assert v
            if len(v) == 1:
                dict_out[k] = v[0]
            elif len(v) > 1:
                dict_out[k] = self.merge(v, name=join_name(name, k))
        return dict_out

    def merge(self, objs, name=""):
        """Generic merge function.

        name is a string representing the current key value separated by
        semicolons. For example, if file.json had the following;

            {'key1': {'key2': 3}}

        Then the name of the value 3 is 'file.json:key1:key2'
        """
        objs = [o for o in objs if o is not None]

        if not objs:
            return None

        MergeFailure.assert_type_eq(name, objs)

        # Try the merge helpers.
        for match_func, merge_func in reversed(self.helpers):
            for obj in objs:
                if match_func(obj, name):
                    return merge_func(objs, name=name)

        return self.fallback_matcher(objs, name=name)


# Classes for recursively merging a directory together.
# ------------------------------------------------------------------------


class FilenameRegexMatch(object):
    """Match based on name matching a regex."""

    def __init__(self, regex):
        self.regex = re.compile(regex)

    def __call__(self, filename, to_merge):
        return self.regex.search(filename) is not None

    def __str__(self):
        return "FilenameRegexMatch(%r)" % self.regex.pattern

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
                to_merge)

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
            before_0, new_json_data_0, after_0 = self.load_jsonp(
                self.filesystem.open_binary_file_for_reading(to_merge[0]))
        except ValueError as e:
            raise MergeFailure(e.message, to_merge[0], None)

        input_data = [new_json_data_0]
        for filename_n in to_merge[1:]:
            try:
                before_n, new_json_data_n, after_n = self.load_jsonp(
                    self.filesystem.open_binary_file_for_reading(filename_n))
            except ValueError as e:
                raise MergeFailure(e.message, filename_n, None)

            if before_0 != before_n:
                raise MergeFailure(
                    "jsonp starting data from %s doesn't match." % filename_n,
                    out_filename,
                    [before_0, before_n])

            if after_0 != after_n:
                raise MergeFailure(
                    "jsonp ending data from %s doesn't match." % filename_n,
                    out_filename,
                    [after_0, after_n])

            input_data.append(new_json_data_n)

        output_data = self._json_data_merger.merge(input_data, name=out_filename)
        output_data.update(self._json_data_value_overrides)

        self.dump_jsonp(
            self.filesystem.open_binary_file_for_writing(out_filename),
            before_0, output_data, after_0)

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
                    'File %s already exist in output.', out_path, None)

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
                NameRegexMatch(match_name),
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
                NameRegexMatch(match_name),
                lambda o, name=None: sum(o))

        # If any shard is interrupted, mark the whole thing as interrupted.
        self.add_helper(
            NameRegexMatch(':interrupted$'),
            lambda o, name=None: bool(sum(o)))

        # Layout test directory value is randomly created on each shard, so
        # clear it.
        self.add_helper(
            NameRegexMatch(':layout_tests_dir$'),
            lambda o, name=None: None)

        # seconds_since_epoch is the start time, so we just take the earliest.
        self.add_helper(
            NameRegexMatch(':seconds_since_epoch$'),
            lambda o, name=None: min(*o))

    def fallback_matcher(self, objs, name=None):
        if self.allow_unknown_if_matching:
            result = self.merge_equal(objs, name)
            _log.warning('Unknown value %s, accepting anyway as it matches.', name)
            return result
        return JSONMerger.fallback_matcher(self, objs, name)


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
            FilenameRegexMatch(r'\.json$'),
            MergeFilesJSONP(self.filesystem, basic_json_data_merger))

        # access_log and error_log are httpd log files which are sortable.
        self.add_helper(
            FilenameRegexMatch(r'access_log\.txt$'),
            MergeFilesLinesSorted(self.filesystem))
        self.add_helper(
            FilenameRegexMatch(r'error_log\.txt$'),
            MergeFilesLinesSorted(self.filesystem))

        # pywebsocket files aren't particularly useful, so just save them.
        self.add_helper(
            FilenameRegexMatch(r'pywebsocket\.ws\.log-.*-err\.txt$'),
            MergeFilesKeepFiles(self.filesystem))

        # These JSON files have "result style" JSON in them.
        results_json_file_merger = MergeFilesJSONP(
            self.filesystem,
            JSONTestResultsMerger(
                allow_unknown_if_matching=results_json_allow_unknown_if_matching),
            json_data_value_overrides=results_json_value_overrides or {})

        self.add_helper(
            FilenameRegexMatch(r'failing_results\.json$'),
            results_json_file_merger)
        self.add_helper(
            FilenameRegexMatch(r'full_results\.json$'),
            results_json_file_merger)
        self.add_helper(
            FilenameRegexMatch(r'output\.json$'),
            results_json_file_merger)
        self.add_helper(
            FilenameRegexMatch(r'full_results_jsonp\.js$'),
            results_json_file_merger)
