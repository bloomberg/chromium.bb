# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic generator for configuration files in JSON5 format.

The configuration file is expected to contain either a data array or a data map,
an optional parameters validation map, and an optional metdata map. Examples:
{
  data: [
    "simple_item",
    "simple_item2",
    {name:"complex_item", param:"Hello World"},
  ],
}

{
  metadata: {
    namespace: "css",
  },
  parameters: {
    param1: {default: 1, valid_values:[1,2,3]},
    param2: {valid_type: "str"},
  },
  data: {
    "simple_item": {},
    "item": {param1:1, param2: "Hello World"},
    "bad_item_fails_validation": {
      name: "bad_item_fails_validation",
      param1: "bad_value_fails_valid_values_check",
      param2: 1.9,
      unknown_param_fails_validation: true,
    },
  },
}

The entries in data array/map are stored in the array self.name_dictionaries.
Each entry in name_dictionaries is always stored as a dictionary.
A simple non-map item is converted to a dictionary containing one entry with
key of "name" and its value the simple item.

The order of entries in name_dictionaries is the same as that specified in the
data array. While for the data map case, by default the entries are sorted
alphabetically by name.

The optional map "parameters" specifies the default values and the valid values
or valid types contained in the data entries. If parameters is specified, then
data entries may not contain keys not present in parameters.

The optional map "metadata" overrides the values specified in default_metadata
if present, and stored as self.metadata. Keys in "metadata" must be present in
default_metadata or an exception raised.
"""

import argparse
import ast
import copy
import os
import os.path
import re


def _json5_load(lines):
    # Use json5.loads when json5 is available. Currently we use simple
    # regexs to convert well-formed JSON5 to PYL format.
    # Strip away comments and quote unquoted keys.
    re_comment = re.compile(r"^\s*//.*$|//+ .*$", re.MULTILINE)
    re_map_keys = re.compile(r"^\s*([$A-Za-z_][\w]*)\s*:", re.MULTILINE)
    pyl = re.sub(re_map_keys, r"'\1':", re.sub(re_comment, "", lines))
    # Convert map values of true/false to Python version True/False.
    re_true = re.compile(r":\s*true\b")
    re_false = re.compile(r":\s*false\b")
    pyl = re.sub(re_true, ":True", re.sub(re_false, ":False", pyl))
    return ast.literal_eval(pyl)


def _merge_doc(doc, doc2):
    def _merge_dict(key):
        if key in doc or key in doc2:
            merged = doc.get(key, {})
            merged.update(doc2.get(key, {}))
            doc[key] = merged

    _merge_dict("metadata")
    _merge_dict("parameters")
    if type(doc["data"]) is list:
        doc["data"].extend(doc2["data"])
    else:
        _merge_dict("data")


class Json5File(object):
    def __init__(self, file_paths, doc, default_metadata=None, default_parameters=None):
        self.file_paths = file_paths
        self.name_dictionaries = []
        self.metadata = copy.deepcopy(default_metadata if default_metadata else {})
        self.parameters = copy.deepcopy(default_parameters if default_parameters else {})
        self._defaults = {}
        self._process(doc)

    @classmethod
    def load_from_files(cls, file_paths, default_metadata=None, default_parameters=None):
        merged_doc = dict()
        for path in file_paths:
            assert path.endswith(".json5")
            with open(os.path.abspath(path)) as json5_file:
                doc = _json5_load(json5_file.read())
                if not merged_doc:
                    merged_doc = doc
                else:
                    _merge_doc(merged_doc, doc)
        return Json5File(file_paths, merged_doc, default_metadata, default_parameters)

    def _process(self, doc):
        # Process optional metadata map entries.
        for key, value in doc.get("metadata", {}).items():
            self._process_metadata(key, value)
        # Get optional parameters map, and get the default value map from it.
        self.parameters.update(doc.get("parameters", {}))
        if self.parameters:
            self._get_defaults()
        # Process normal entries.
        items = doc["data"]
        if type(items) is list:
            for item in items:
                entry = self._get_entry(item)
                self.name_dictionaries.append(entry)
        else:
            for key, value in items.items():
                value["name"] = key
                entry = self._get_entry(value)
                self.name_dictionaries.append(entry)
            self.name_dictionaries.sort(key=lambda entry: entry["name"])

    def _process_metadata(self, key, value):
        if key not in self.metadata:
            raise Exception("Unknown metadata: '%s'\nKnown metadata: %s" %
                            (key, self.metadata.keys()))
        self.metadata[key] = value

    def _get_defaults(self):
        for key, value in self.parameters.items():
            if value and "default" in value:
                self._defaults[key] = value["default"]
            else:
                self._defaults[key] = None

    def _get_entry(self, item):
        entry = copy.deepcopy(self._defaults)
        if type(item) is not dict:
            entry["name"] = item
            return entry
        if "name" not in item:
            raise Exception("Missing name in item: %s" % item)
        if not self.parameters:
            entry.update(item)
            return entry
        assert "name" not in self.parameters, "The parameter 'name' is reserved, use a different name."
        entry["name"] = item.pop("name")
        # Validate parameters if it's specified.
        for key, value in item.items():
            if key not in self.parameters:
                raise Exception(
                    "Unknown parameter: '%s'\nKnown params: %s" %
                    (key, self.parameters.keys()))
            assert self.parameters[key] is not None, \
                "Specification for parameter 'key' cannot be None. Use {} instead."
            self._validate_parameter(self.parameters[key], value)
            entry[key] = value
        return entry

    def _validate_parameter(self, parameter, value):
        valid_type = parameter.get("valid_type")
        if valid_type and type(value).__name__ != valid_type:
            raise Exception("Incorrect type: '%s'\nExpected type: %s" %
                            (type(value).__name__, valid_type))
        valid_values = parameter.get("valid_values")
        if not valid_values:
            return
        # If valid_values is a list of simple items and not list of list, then
        # validate each item in the value list against valid_values.
        if valid_type == "list" and type(valid_values[0]) is not list:
            for item in value:
                if item not in valid_values:
                    raise Exception("Unknown value: '%s'\nKnown values: %s" %
                                    (item, valid_values))
        elif value not in valid_values:
            raise Exception("Unknown value: '%s'\nKnown values: %s" %
                            (value, valid_values))


class Writer(object):
    # Subclasses should override.
    class_name = None
    default_metadata = None
    default_parameters = None

    def __init__(self, json5_files):
        self._input_files = copy.copy(json5_files)
        self._outputs = {}  # file_name -> generator
        self.gperf_path = None
        if json5_files:
            self.json5_file = Json5File.load_from_files(json5_files,
                                                        self.default_metadata,
                                                        self.default_parameters)

    def _write_file_if_changed(self, output_dir, contents, file_name):
        path = os.path.join(output_dir, file_name)

        # The build system should ensure our output directory exists, but just
        # in case.
        directory = os.path.dirname(path)
        if not os.path.exists(directory):
            os.makedirs(directory)

        # Only write the file if the contents have changed. This allows ninja to
        # skip rebuilding targets which depend on the output.
        with open(path, "a+") as output_file:
            output_file.seek(0)
            if output_file.read() != contents:
                output_file.truncate(0)
                output_file.write(contents)

    def write_files(self, output_dir):
        for file_name, generator in self._outputs.items():
            self._write_file_if_changed(output_dir, generator(), file_name)

    def set_gperf_path(self, gperf_path):
        self.gperf_path = gperf_path


class Maker(object):
    def __init__(self, writer_class):
        self._writer_class = writer_class

    def main(self):
        parser = argparse.ArgumentParser()
        # Require at least one input file.
        parser.add_argument("files", nargs="+")

        parser.add_argument("--gperf", default="gperf")
        parser.add_argument("--developer_dir", help="Path to Xcode.")
        parser.add_argument("--output_dir", default=os.getcwd())
        args = parser.parse_args()

        if args.developer_dir:
            os.environ["DEVELOPER_DIR"] = args.developer_dir

        writer = self._writer_class(args.files)
        writer.set_gperf_path(args.gperf)
        writer.write_files(args.output_dir)
