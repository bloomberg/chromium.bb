# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to fetch new baselines from try jobs for a Rietveld issue.

This command interacts with the Rietveld API to get information about try jobs
with layout test results.

TODO(qyearsley): Finish this module. After getting a list of try bots:
  - For each bot get the list of tests to rebaseline.
  - Invoke webkit-patch rebaseline-test-internal to fetch the baselines.
"""

import logging
import optparse

from webkitpy.common.net.rietveld import latest_try_jobs
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from webkitpy.common.net.web import Web


TRY_BOTS = (
    'linux_chromium_rel_ng',
    'mac_chromium_rel_ng',
    'win_chromium_rel_ng',
)

_log = logging.getLogger(__name__)


class RebaselineFromTryJobs(AbstractParallelRebaselineCommand):
    name = "rebaseline-from-try-jobs"
    help_text = "Fetches new baselines from layout test runs on try bots."
    show_in_main_help = True

    def __init__(self):
        super(RebaselineFromTryJobs, self).__init__(options=[
            optparse.make_option(
                '--issue', type='int', default=None,
                help="Rietveld issue number."),
        ])
        self.web = Web()

    def execute(self, options, args, tool):
        if not options.issue:
            # TODO(qyearsley): Later, the default behavior will be to run
            # git cl issue to get the issue number, then get lists of tests
            # to rebaseline and download new baselines.
            _log.info('No issue number provided.')
            return
        _log.info('Getting results for Rietveld issue %d.' % options.issue)
        jobs = latest_try_jobs(options.issue, TRY_BOTS, self.web)
        for job in jobs:
            _log.info('  Builder: %s\n  Master: %s\n  Build: %d',
                      job.builder_name,
                      job.master_name,
                      job.build_number)
