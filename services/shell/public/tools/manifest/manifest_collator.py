#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A collator for Service Manifests """

import argparse
import json
import os
import shutil
import sys
import urlparse

eater_relative = '../../../../../../tools/json_comment_eater'
eater_relative = os.path.join(os.path.abspath(__file__), eater_relative)
sys.path.insert(0, os.path.normpath(eater_relative))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)

def ParseJSONFile(filename):
  with open(filename) as json_file:
    try:
      return json.loads(json_comment_eater.Nom(json_file.read()))
    except ValueError:
      print "%s is not a valid JSON document" % filename
      return None

def MergeDicts(left, right):
  for k, v in right.iteritems():
    if k not in left:
      left[k] = v
    else:
      if isinstance(v, dict):
        assert isinstance(left[k], dict)
        MergeDicts(left[k], v)
      elif isinstance(v, list):
        assert isinstance(left[k], list)
        left[k].extend(v)
      else:
        raise "Refusing to merge conflicting non-collection values."
  return left


def MergeBaseManifest(parent, base):
  MergeDicts(parent["capabilities"], base["capabilities"])

  if "services" in base:
    if "services" not in parent:
      parent["services"] = []
    parent["services"].extend(base["services"])

  if "process-group" in base:
    parent["process-group"] = base["process-group"]


def main():
  parser = argparse.ArgumentParser(
      description="Collate Service Manifests.")
  parser.add_argument("--parent")
  parser.add_argument("--output")
  parser.add_argument("--name")
  parser.add_argument("--base-manifest", default=None)
  args, children = parser.parse_known_args()

  parent = ParseJSONFile(args.parent)
  if parent == None:
    return 1

  if args.base_manifest:
    base = ParseJSONFile(args.base_manifest)
    if base == None:
      return 1
    MergeBaseManifest(parent, base)

  service_path = parent['name'].split(':')[1]
  if service_path.startswith('//'):
    raise ValueError("Service name path component '%s' must not start " \
                     "with //" % service_path)

  if args.name != service_path:
    raise ValueError("Service name '%s' specified in build file does not " \
                     "match name '%s' specified in manifest." %
                     (args.name, service_path))

  services = []
  for child in children:
    service = ParseJSONFile(child)
    if service == None:
      return 1
    services.append(service)

  if len(services) > 0:
    parent['services'] = services

  with open(args.output, 'w') as output_file:
    json.dump(parent, output_file)

  return 0

if __name__ == "__main__":
  sys.exit(main())
