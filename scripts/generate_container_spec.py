# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script generates brick runtime environment metadata from appc manifests.

On the device image we encode the outlines of the brick runtime environment in
ContainerSpecs, a protocol buffer understood by somad.  Brick developers specify
the information that goes into a ContainerSpec in the form of an appc pod
manifest, which a JSON blob adhering to an open standard.  This scripts maps
from pod manifests to ContainerSpecs.
"""

from __future__ import print_function

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import container_spec_generator


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--sysroot', type='path',
                      help='The sysroot to use for brick metadata validation.')
  parser.add_argument('appc_pod_manifest_path', type='path',
                      help='path to appc pod manifest')
  parser.add_argument('container_spec_path', type='path',
                      help='path to file to write resulting ContainerSpec to. '
                           'Must not exist.')
  options = parser.parse_args(argv)
  options.Freeze()

  cros_build_lib.AssertInsideChroot()

  generator = container_spec_generator.ContainerSpecGenerator(options.sysroot)
  generator.WriteContainerSpec(options.appc_pod_manifest_path,
                               options.container_spec_path)
  return 0
