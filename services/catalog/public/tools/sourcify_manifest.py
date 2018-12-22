#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source and header files defining function to create an
in-memory representation of a static catalog manifest at runtime."""


import argparse
import imp
import json
import os.path
import sys

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir,
    "build", "android", "gyp"))
from util import build_utils


_H_FILE_TEMPLATE = "catalog.h.tmpl"
_CC_FILE_TEMPLATE = "catalog.cc.tmpl"


eater_relative = "../../../../../tools/json_comment_eater"
eater_relative = os.path.join(os.path.abspath(__file__), eater_relative)
sys.path.insert(0, os.path.normpath(eater_relative))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)


# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path


try:
  imp.find_module("jinja2")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("services"), "third_party"))
import jinja2


def ApplyTemplate(path_to_template, output_path, global_vars, **kwargs):
  with build_utils.AtomicOutput(output_path) as output_file:
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
        keep_trailing_newline=True, **kwargs)
    jinja_env.globals.update(global_vars)
    output_file.write(jinja_env.get_template(path_to_template).render())


def main():
  parser = argparse.ArgumentParser(
      description="Generates a C++ constant containing a catalog manifest.")
  parser.add_argument("--root-manifest")
  parser.add_argument("--submanifest-info")
  parser.add_argument("--output-filename-base")
  parser.add_argument("--output-function-name")
  parser.add_argument("--module-path")
  args, _ = parser.parse_known_args()

  if args.submanifest_info is None:
    raise Exception("--submanifest-info required")
  if args.output_filename_base is None:
    raise Exception("--output-filename-base is required")
  if args.output_function_name is None:
    raise Exception("--output-function-name is required")
  if args.module_path is None:
    args.module_path = args.output_filename_base

  if args.root_manifest:
    with open(args.root_manifest, "r") as input_file:
      root_manifest = json.loads(json_comment_eater.Nom(input_file.read()))
  else:
    root_manifest = None

  qualified_function_name = args.output_function_name.split("::")
  namespaces = qualified_function_name[0:-1]
  function_name = qualified_function_name[-1]

  def raise_error(error, value):
    raise Exception(error)

  overlays = []
  packaged_services = []
  with open(args.submanifest_info, "r") as info_file:
    for line in info_file.readlines():
      submanifest_type, namespace_file, header_base = line.strip().split("@", 3)
      with open(namespace_file, "r") as namespace_file:
        namespace = namespace_file.readline().strip()
      info = { "namespace": namespace, "header": header_base + ".h" }
      if submanifest_type == "overlay":
        overlays.append(info)
      else:
        packaged_services.append(info)

  global_vars = {
    "root_manifest": root_manifest,
    "overlays": overlays,
    "packaged_services": packaged_services,
    "function_name": function_name,
    "namespaces": namespaces,
    "path": args.module_path,
    "raise": raise_error,
  }

  input_h_filename = _H_FILE_TEMPLATE
  output_h_filename = "%s.h" % args.output_filename_base
  ApplyTemplate(input_h_filename, output_h_filename, global_vars)

  input_cc_filename = _CC_FILE_TEMPLATE
  output_cc_filename = "%s.cc" % args.output_filename_base
  ApplyTemplate(input_cc_filename, output_cc_filename, global_vars)

  return 0

if __name__ == "__main__":
  sys.exit(main())
