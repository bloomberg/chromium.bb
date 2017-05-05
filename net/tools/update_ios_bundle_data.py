#!/usr/bin/python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper to update the "net_unittests_bundle_data" section of
//net/BUILD.gn so that it lists all of the current files (based on
simpler globbing rules).
"""

import glob
import os
import re
import sys


def get_net_path():
  """Returns the path to //net"""
  return os.path.realpath(os.path.join(os.path.dirname(__file__), os.pardir))


INCLUSIONS = [
    "data/cert_issuer_source_aia_unittest/*.pem",
    "data/cert_issuer_source_static_unittest/*.pem",
    "data/certificate_policies_unittest/*.pem",
    "data/filter_unittests/*",
    "data/name_constraints_unittest/*.pem",
    "data/parse_certificate_unittest/*.pem",
    "data/parse_ocsp_unittest/*.pem",
    "data/test.html",
    "data/url_request_unittest/*",
    "data/verify_certificate_chain_unittest/**/*.pem",
    "data/verify_certificate_chain_unittest/**/*.test",
    "data/verify_name_match_unittest/names/*.pem",
    "data/verify_signed_data_unittest/*.pem",
    "third_party/nist-pkits/certs/*.crt",
    "third_party/nist-pkits/crls/*.crl",
]


def do_file_glob(rule):
  # Do the globbing relative to //net
  prefix = get_net_path()
  matches = glob.glob(prefix + os.sep + rule)

  # Strip off the prefix.
  return [f[len(prefix) + 1:] for f in matches]


def get_all_data_file_paths():
  paths = []
  for rule in INCLUSIONS:
    paths.extend(do_file_glob(rule))
  return paths


def read_file_to_string(path):
  with open(path, 'r') as f:
    return f.read()


def write_string_to_file(data, path):
  with open(path, 'w') as f:
    f.write(data)


def fatal(message):
  print "FATAL: " + message
  sys.exit(1)


# This regular expression identifies the "sources" section of
# net_unittests_bundle_data
bundle_data_regex = re.compile(r"""
bundle_data\("net_unittests_bundle_data"\) \{
  testonly = true
  sources = \[
(.+?)
  \]
  outputs = \[""", re.MULTILINE | re.DOTALL)


def format_file_list(files):
  # Keep the file list in sorted order.
  files = sorted(files)

  # Format to a string for GN (assume the filepaths don't contain
  # characters that need escaping).
  return ",\n".join('    "%s"' % f for f in files) + ","


def main():
  # Find all the data files.
  all_files = get_all_data_file_paths()

  # Read in //net/BUILD.gn
  path = os.path.join(get_net_path(), "BUILD.gn")
  data = read_file_to_string(path)

  # Replace the sources part of "net_unittests_bundle_data" with
  # the newly collected file list.
  m = bundle_data_regex.search(data)
  if not m:
    fatal("Couldn't find the net_unittests_bundle_data section")
  data = data[0:m.start(1)] + format_file_list(all_files) + data[m.end(1):]

  write_string_to_file(data, path)
  print "Wrote %s" % path


if __name__ == '__main__':
  sys.exit(main())
