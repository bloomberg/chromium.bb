# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse

from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.layout_tests.controllers.test_result_writer import baseline_name
from webkitpy.tool.commands.rebaseline import AbstractRebaseliningCommand


_log = logging.getLogger(__name__)


class OptimizeBaselines(AbstractRebaseliningCommand):
    name = "optimize-baselines"
    help_text = "Reshuffles the baselines for the given tests to use as litte space on disk as possible."
    show_in_main_help = True
    argument_names = "TEST_NAMES"

    def __init__(self):
        super(OptimizeBaselines, self).__init__(options=[
            self.suffixes_option,
            optparse.make_option('--no-modify-scm', action='store_true', default=False,
                                 help='Dump SCM commands as JSON instead of actually committing changes.'),
        ] + self.platform_options)

    def _optimize_baseline(self, optimizer, test_name):
        files_to_delete = []
        files_to_add = []
        for suffix in self._baseline_suffix_list:
            name = baseline_name(self._tool.filesystem, test_name, suffix)
            succeeded, more_files_to_delete, more_files_to_add = optimizer.optimize(name)
            if not succeeded:
                _log.error("Heuristics failed to optimize %s", name)
            files_to_delete.extend(more_files_to_delete)
            files_to_add.extend(more_files_to_add)
        return files_to_delete, files_to_add

    def execute(self, options, args, tool):
        self._tool = tool
        self._baseline_suffix_list = options.suffixes.split(',')
        port_names = tool.port_factory.all_port_names(options.platform)
        if not port_names:
            _log.error("No port names match '%s'", options.platform)
            return
        port = tool.port_factory.get(port_names[0])
        optimizer = BaselineOptimizer(tool, port, port_names, skip_scm_commands=options.no_modify_scm)
        tests = port.tests(args)
        for test_name in tests:
            files_to_delete, files_to_add = self._optimize_baseline(optimizer, test_name)
            for path in files_to_delete:
                self._delete_from_scm_later(path)
            for path in files_to_add:
                self._add_to_scm_later(path)
        self._print_scm_changes()
