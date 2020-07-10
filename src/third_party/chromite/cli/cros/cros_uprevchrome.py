# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros uprevchrome: Uprev chrome to a new valid version."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import cros_build_lib


@command.CommandDecorator('uprevchrome')
class UprevChromeCommand(command.CliCommand):
  """cros uprevchrome: Uprev chrome to a new valid version

  When a chrome PFQ failure happens, master_chrome_pfq automatically checks the
  status of certain stages and then uploads the ebuild/binhost_mapping
  updates to a staging branch in remote git repository. This tool provides a way
  to fetch the updates from the remote staging branch, generate CLs to manually
  uprev chrome and upload the CLs to Gerrit for review.

  Users need to provide a failed master_chrome_pfq build_id, the bug id to track
  this manual uprev. This tool first verifies if the build_id is valid and if
  the staging branches exist, then fetches the changes into local branches,
  updates commit messages, adds CQ-DEPEND, rebases based on master and uploads
  CLs to Gerrit.

  Examples:
    cros uprevchrome --pfq-build XXXX --bug='chromium:XXXXX'
        [--dry-run] [--debug] [--nowipe] [--draft]

  After successfully executing this tool, review and submit CLs from Gerrit.
  https://chromium-review.googlesource.com
  https://chrome-internal-review.googlesource.com/

  Note:
  A master Chrome PFQ build id argument is required. It must satisfy the
  following requirements:
    * If you are a gardener trying to uprev Chrome please verify if the Chrome
      version you are trying to uprev to is stable and not crashy. Please
      communicate with the corresponding Chrome TPM to find out if that Chrome
      has any known issues or is particularly crashy. If yes, please work with
      them to find a stable Chrome that can be used to uprev.
    * The current  Chrome PFQ run is a failed run;
    * No successful Chrome PFQ runs after this Chrome PFQ run;
    * Master build of this PFQ run must have passed BinhostTest stage;
    * All important slave builds of this PFQ run must have passed
      UploadPrebuiltsTest stage.
    * A bug id is required to track each manual uprev, describing the known
      source of flake that is responsible for this run's failure.
  The public CL and private CL depend on each other,
  please submit them together.
  Please do not revert the generated CLs after they're merged.
  Please use cros_pinchrome to pin/unpin Chrome if needed.
  A detailed guide here:
  http://g3doc/company/teams/chrome/ops/chromeos/continuous_integration/on-call/guides/cros_uprevchrome_guide
  """

  @classmethod
  def AddParser(cls, parser):
    super(cls, UprevChromeCommand).AddParser(parser)
    parser.add_argument('--pfq-build', action='store',
                        metavar='PFQ_BUILD',
                        help='The buildbucket_id of the master chrome pfq'
                        ' build. Note this is a 19 digit ID that can be found'
                        'in the Milo or GoldenEye URL for the build.')
    parser.add_argument('--fake-buildstore', action='store', default=False,
                        help='Use a FakeBuildStore instance')
    parser.add_argument('--bug', action='store',
                        help='Used in the "BUG" field of CLs.')
    parser.add_argument('--nowipe', default=True, dest='wipe',
                        action='store_false',
                        help='Preserve the working directory.')
    parser.add_argument('--draft', action='store_true',
                        help='Upload the uprev CLs to Gerrit as drafts.')
    parser.add_argument('--dry-run', action='store_true',
                        help="Prepare CLs but don't upload them to Gerrit")
    parser.add_argument('--reviewers',
                        action='append', default=[],
                        help='Add reviewers to the CLs.'
                        'ex:--reviewer x@email.com --reviewer y@email.com')
    return parser

  def Run(self):
    """Prints deprecation message."""
    cros_build_lib.Die('***CROS UPREVCHROME HAS BEEN SUPERSEDED BY PUpr!***'
                       '\n***Refer to go/cros-gardening and go/pupr***')
