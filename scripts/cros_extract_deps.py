#!/usr/bin/python2.6
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import portage
import sys
from parallel_emerge import DepGraphGenerator

def FlattenDepTree(deptree, pkgtable=None, parentcpv=None):
  """
  Turn something like this (the parallel_emerge DepsTree format):
{
  "app-admin/eselect-1.2.9": {
    "action": "merge",
    "deps": {
      "sys-apps/coreutils-7.5-r1": {
        "action": "merge",
        "deps": {},
        "deptype": "runtime"
      },
      ...
    }
  }
}
  ...into something like this (the cros_extract_deps format):
{
  "app-admin/eselect-1.2.9": {
    "deps": ["coreutils-7.5-r1"],
    "rev_deps": [],
    "name": "eselect",
    "category": "app-admin",
    "version": "1.2.9",
    "full_name": "app-admin/eselect-1.2.9",
    "action": "merge"
  },
  "sys-apps/coreutils-7.5-r1": {
    "deps": [],
    "rev_deps": ["app-admin/eselect-1.2.9"],
    "name": "coreutils",
    "category": "sys-apps",
    "version": "7.5-r1",
    "full_name": "sys-apps/coreutils-7.5-r1",
    "action": "merge"
  }
}
  """
  if pkgtable is None:
    pkgtable = {}
  for cpv, record in deptree.items():
    if cpv not in pkgtable:
      cat, nam, ver, rev = portage.versions.catpkgsplit(cpv)
      pkgtable[cpv] = {"deps": [],
                       "rev_deps": [],
                       "name": nam,
                       "category": cat,
                       "version": "%s-%s" % (ver, rev),
                       "full_name": cpv,
                       "action": record["action"]}
    # If we have a parent, that is a rev_dep for the current package.
    if parentcpv:
      pkgtable[cpv]["rev_deps"].append(parentcpv)
    # If current package has any deps, record those.
    for childcpv in record["deps"]:
      pkgtable[cpv]["deps"].append(childcpv)
    # Visit the subtree recursively as well.
    FlattenDepTree(record["deps"], pkgtable=pkgtable, parentcpv=cpv)
  return pkgtable


def Usage():
  """Print usage."""
  print """Usage:
cros_extract_deps [--board=BOARD] [emerge args] package

This extracts the dependency tree for the specified package, and outputs it
to stdout, in a serialized JSON format. Emerge flags such as --root-deps
can be used to influence what sort of dependency tree is extracted.
"""


def main(argv):
  if len(argv) == 1:
    Usage()
    sys.exit(1)

  # We want the toolchain to be quiet because we are going to output
  # a well-formed json payload.
  argv = ['--quiet', '--pretend']

  # cros_extract_deps defaults to rdeps. However, only use this if
  # there was no explicit --root-deps command line switch.
  default_rootdeps_arg = ['--root-deps=rdeps']
  for arg in argv:
    if arg.startswith('--root-deps'):
      default_rootdeps_arg = []

  # Now, assemble the overall argv as the concatenation of the
  # default list + possible rootdeps-default + actual command line.
  argv.extend(default_rootdeps_arg)
  argv.extend(sys.argv[1:])

  deps = DepGraphGenerator()
  deps.Initialize(argv)
  deps_tree, _deps_info = deps.GenDependencyTree()
  print json.dumps(FlattenDepTree(deps_tree), sort_keys=True, indent=2)
