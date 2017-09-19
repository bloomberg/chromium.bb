#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import hashlib
import logging
import os
import re
import sys

import extract_histograms
import merge_xml

_DATE_FILE_PATTERN = r".*MAJOR_BRANCH_DATE=(.+).*"

_SCRIPT_NAME = "generate_expired_histograms_array.py"
_HASH_DATATYPE = "uint64_t"
_HEADER = """// Generated from {script_name}. Do not edit!

#ifndef {include_guard}
#define {include_guard}

#include <stdint.h>

namespace {namespace} {{

// Contains hashes of expired histograms.
const {hash_datatype} kExpiredHistogramsHashes[] = {{
{hashes}
}};

const size_t kNumExpiredHistograms = {hashes_size};

}}  // namespace {namespace}

#endif  // {include_guard}
"""


class Error(Exception):
  pass


def _GetExpiredHistograms(histograms, base_date):
  """Filters histograms to find expired ones.

  Args:
    histograms(Dict[str, Dict]): Histogram descriptions in the form
      {name: content}.
    base_date(datetime.date): A date to check expiry dates against.

  Returns:
    List of strings with names of expired histograms.

  Raises:
    Error if there is an expiry date that doesn't match expected format.
  """
  expired_histograms_names = []
  for name, content in histograms.items():
    if "obsolete" in content or "expiry_date" not in content:
      continue
    expiry_date_str = content["expiry_date"]
    try:
      expiry_date = datetime.datetime.strptime(
          expiry_date_str, extract_histograms.EXPIRY_DATE_PATTERN).date()
    except ValueError:
      raise Error("Unable to parse expiry date {date} in histogram {name}.".
                  format(date=expiry_date_str, name=name))
    if expiry_date < base_date:
      expired_histograms_names.append(name)
  return expired_histograms_names


def _GetBaseDate(content, pattern):
  """Fetches base date from |content| to compare expiry dates with.

  Args:
   content: A string with the base date.
   pattern(str): A regular expression that matches the base date.

  Returns:
   A base date as datetime.date object.

  Raises:
    Error if |content| doesn't match |pattern| or the matched date has invalid
    format.
  """
  match_result = re.search(pattern, content)
  if not match_result:
    raise Error("Unable to match {pattern} with provided content: {content}".
                format(pattern=pattern, content=content))
  base_date_str = match_result.group(1)
  try:
    base_date = datetime.datetime.strptime(
        base_date_str, extract_histograms.EXPIRY_DATE_PATTERN).date()
    return base_date
  except ValueError:
    raise Error("Unable to parse base date {date} from {content}.".
                format(date=base_date_str, content=content))


def _HashName(name):
  """Returns hash for the given histogram |name|."""
  return "0x" + hashlib.md5(name).hexdigest()[:16]


def _GetHashToNameMap(histograms_names):
  """Returns dictionary {hash: histogram_name}."""
  hash_to_name_map = dict()
  for name in histograms_names:
    hash_to_name_map[_HashName(name)] = name
  return hash_to_name_map


def _GenerateHeaderFileContent(header_filename, namespace,
                               histograms_map):
  """Generates header file content.

  Args:
    header_filename: A filename of the generated header file.
    namespace: A namespace to contain generated array.
    hash_datatype: Datatype of histogram names' hash.
    histograms_map(Dict[str, str]): A dictionary {hash: histogram_name}.

  Returns:
    String with the generated content.
  """
  include_guard = re.sub("[^A-Z]", "_", header_filename.upper()) + "_"
  if not histograms_map:
    # Some platforms don't allow creating empty arrays.
    histograms_map["0x0000000000000000"] = "Dummy.Histogram"
  hashes = "\n".join([
      "  {hash},  // {name}".format(hash=value, name=histograms_map[value])
      for value in sorted(histograms_map.keys())
  ])
  return _HEADER.format(
      script_name=_SCRIPT_NAME,
      include_guard=include_guard,
      namespace=namespace,
      hash_datatype=_HASH_DATATYPE,
      hashes=hashes,
      hashes_size=len(histograms_map))


def _GenerateFile(arguments):
  """Generates header file containing array with hashes of expired histograms.

  Args:
    arguments: An object with the following attributes:
      arguments.inputs: A list of xml files with histogram descriptions.
      arguments.header_filename: A filename of the generated header file.
      arguments.namespace: A namespace to contain generated array.
      arguments.hash_datatype: Datatype of histogram names' hash.
      arguments.output_dir: A directory to put the generated file.

  Raises:
    Error if there is an error in input xml files.
  """
  descriptions = merge_xml.MergeFiles(arguments.inputs)
  histograms, had_errors = (
      extract_histograms.ExtractHistogramsFromDom(descriptions))
  if had_errors:
    raise Error("Error parsing inputs.")
  with open(arguments.major_branch_date_filepath, "r") as date_file:
    file_content = date_file.read()
  base_date = _GetBaseDate(file_content, _DATE_FILE_PATTERN)
  expired_histograms_names = _GetExpiredHistograms(histograms, base_date)
  expired_histograms_map = _GetHashToNameMap(expired_histograms_names)
  header_file_content = _GenerateHeaderFileContent(
      arguments.header_filename, arguments.namespace,
      expired_histograms_map)
  with open(os.path.join(arguments.output_dir, arguments.header_filename),
            "w") as generated_file:
    generated_file.write(header_file_content)


def _ParseArguments():
  """Defines and parses arguments from the command line."""
  arg_parser = argparse.ArgumentParser(
      description="Generate array of expired histograms' hashes.")
  arg_parser.add_argument(
      "--output_dir",
      "-o",
      required=True,
      help="Base directory to for generated files.")
  arg_parser.add_argument(
      "--header_filename",
      "-H",
      required=True,
      help="File name of the generated header file.")
  arg_parser.add_argument(
      "--namespace",
      "-n",
      default="",
      help="Namespace of the generated factory function (code will be in "
      "the global namespace if this is omitted).")
  arg_parser.add_argument(
      "--major_branch_date_filepath",
      "-d",
      default="",
      help="A path to the file with the base date.")
  arg_parser.add_argument(
      "inputs",
      nargs="+",
      help="Paths to .xml files with histogram descriptions.")
  return arg_parser.parse_args()


def main():
  arguments = _ParseArguments()
  _GenerateFile(arguments)


if __name__ == "__main__":
  sys.exit(main())
