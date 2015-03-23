# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command to extract the dependancy tree for a given package."""

from __future__ import print_function

import json
import portage  # pylint: disable=F0401

from parallel_emerge import DepGraphGenerator

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

def FlattenDepTree(deptree, pkgtable=None, parentcpv=None, get_cpe=False):
  """Simplify dependency json.

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

  Args:
    deptree: The dependency tree.
    pkgtable: The package table to update. If None, create a new one.
    parentcpv: The parent CPV.
    get_cpe: If set True, include CPE in the flattened dependency tree.

  Returns:
    A flattened dependency tree.
  """
  if pkgtable is None:
    pkgtable = {}
  for cpv, record in deptree.iteritems():
    if cpv not in pkgtable:
      cat, nam, ver, rev = portage.versions.catpkgsplit(cpv)
      pkgtable[cpv] = {'deps': [],
                       'rev_deps': [],
                       'name': nam,
                       'category': cat,
                       'version': '%s-%s' % (ver, rev),
                       'full_name': cpv,
                       'cpes': [],
                       'action': record['action']}
      if get_cpe:
        pkgtable[cpv]['cpes'].extend(GetCPEFromCPV(cat, nam, ver))

    # If we have a parent, that is a rev_dep for the current package.
    if parentcpv:
      pkgtable[cpv]['rev_deps'].append(parentcpv)
    # If current package has any deps, record those.
    for childcpv in record['deps']:
      pkgtable[cpv]['deps'].append(childcpv)
    # Visit the subtree recursively as well.
    FlattenDepTree(record['deps'], pkgtable=pkgtable, parentcpv=cpv,
                   get_cpe=get_cpe)
  return pkgtable


def GetCPEFromCPV(category, package, version):
  """Look up the CPE for a specified Portage package.

  Args:
    category: The Portage package's category, e.g. "net-misc"
    package: The Portage package's name, e.g. "curl"
    version: The Portage version, e.g. "7.30.0"

  Returns:
    A list of CPE Name strings, e.g.
    ["cpe:/a:curl:curl:7.30.0", "cpe:/a:curl:libcurl:7.30.0"]
  """
  equery_cmd = ['equery', 'm', '-U', '%s/%s' % (category, package)]
  lines = cros_build_lib.RunCommand(equery_cmd, error_code_ok=True,
                                    print_cmd=False,
                                    redirect_stdout=True).output.splitlines()
  # Look for lines like "Remote-ID:   cpe:/a:kernel:linux-pam ID: cpe"
  # and extract the cpe URI.
  cpes = []
  for line in lines:
    if 'ID: cpe' not in line:
      continue
    cpes.append('%s:%s' % (line.split()[1], version.replace('_', '')))
  # Note that we're assuming we can combine the root of the CPE, taken
  # from metadata.xml, and tack on the version number as used by
  # Portage, and come up with a legitimate CPE. This works so long as
  # Portage and CPE agree on the precise formatting of the version
  # number, which they almost always do. The major exception we've
  # identified thus far is that our ebuilds have a pattern of inserting
  # underscores prior to patchlevels, that neither upstream nor CPE
  # use. For example, our code will decide we have
  # cpe:/a:todd_miller:sudo:1.8.6_p7 yet the advisories use a format
  # like cpe:/a:todd_miller:sudo:1.8.6p7, without the underscore. (CPE
  # is "right" in this example, in that it matches www.sudo.ws.)
  #
  # Removing underscores seems to improve our chances of correctly
  # arriving at the CPE used by NVD. However, at the end of the day,
  # ebuild version numbers are rev'd by people who don't have "try to
  # match NVD" as one of their goals, and there is always going to be
  # some risk of minor formatting disagreements at the version number
  # level, if not from stray underscores then from something else.
  #
  # This is livable so long as you do some fuzzy version number
  # comparison in your vulnerability monitoring, between what-we-have
  # and what-the-advisory-says-is-affected.
  return cpes


def ExtractCPEList(deps_list):
  cpe_dump = []
  for cpv, record in deps_list.iteritems():
    if record['cpes']:
      name = '%s/%s' % (record['category'], record['name'])
      cpe_dump.append({'ComponentName': name,
                       'Repository': 'cros',
                       'Targets': sorted(record['cpes'])})
    else:
      logging.warning('No CPE entry for %s', cpv)
  return sorted(cpe_dump, key=lambda k: k['ComponentName'])


def main(argv):
  parser = commandline.ArgumentParser(description="""
This extracts the dependency tree for the specified package, and outputs it
to stdout, in a serialized JSON format.""")
  parser.add_argument('--board', default=None,
                      help='The board to use when computing deps.')
  parser.add_argument('--format', default='deps',
                      choices=['deps', 'cpe'],
                      help='Output either traditional deps or CPE-only JSON.')
  parser.add_argument('--output-path', default=None,
                      help='Write output to the given path.')
  known_args, unknown_args = parser.parse_known_args(argv)

  lib_argv = []
  if known_args.board:
    lib_argv += ['--board=%s' % known_args.board]
  lib_argv += ['--quiet', '--pretend', '--emptytree']
  lib_argv.extend(unknown_args)

  deps = DepGraphGenerator()
  deps.Initialize(lib_argv)
  deps_tree, _deps_info = deps.GenDependencyTree()
  deps_list = FlattenDepTree(deps_tree, get_cpe=(known_args.format == 'cpe'))
  if known_args.format == 'cpe':
    deps_list = ExtractCPEList(deps_list)

  deps_output = json.dumps(deps_list, sort_keys=True, indent=2)
  if known_args.output_path:
    with open(known_args.output_path, 'w') as f:
      f.write(deps_output)
  else:
    print(deps_output)
