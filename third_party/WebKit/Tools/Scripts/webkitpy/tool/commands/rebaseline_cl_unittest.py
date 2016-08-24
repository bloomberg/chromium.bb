# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import json

from webkitpy.common.checkout.scm.scm_mock import MockSCM
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.rietveld import Rietveld
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline_cl import RebaselineCL
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase


class RebaselineCLTest(BaseTestCase):
    command_constructor = RebaselineCL

    def setUp(self):
        super(RebaselineCLTest, self).setUp()
        web = MockWeb(urls={
            'https://codereview.chromium.org/api/11112222': json.dumps({
                'patchsets': [1, 2],
            }),
            'https://codereview.chromium.org/api/11112222/2': json.dumps({
                'try_job_results': [
                    {
                        'builder': 'MOCK Try Win',
                        'buildnumber': 5000,
                    },
                    {
                        'builder': 'MOCK Mac Try',
                        'buildnumber': 4000,
                    },
                ],
                'files': {
                    'third_party/WebKit/LayoutTests/fast/dom/prototype-inheritance.html': {'status': 'M'},
                    'third_party/WebKit/LayoutTests/fast/dom/prototype-taco.html': {'status': 'M'},
                },
            }),
        })
        self.tool.builders = BuilderList({
            "MOCK Try Win": {
                "port_name": "test-win-win7",
                "specifiers": ["Win7", "Release"],
                "is_try_builder": True,
            },
            "MOCK Try Linux": {
                "port_name": "test-mac-mac10.10",
                "specifiers": ["Mac10.10", "Release"],
                "is_try_builder": True,
            },
        })
        self.command.rietveld = Rietveld(web)
        self.git = MockSCM()
        self.git.get_issue_number = lambda: 'None'
        self.command.git = lambda: self.git

    @staticmethod
    def command_options(**kwargs):
        options = {
            'only_changed_tests': False,
            'dry_run': False,
            'issue': None,
            'optimize': True,
            'results_directory': None,
            'verbose': False,
            'trigger_jobs': False,
        }
        options.update(kwargs)
        return optparse.Values(dict(**options))

    def test_execute_with_issue_number_given(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.command_options(issue=11112222), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertMultiLineEqual(
            logs,
            ('Tests to rebaseline:\n'
             '  svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-inheritance.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-taco.html: MOCK Try Win (5000)\n'
             'Rebaselining fast/dom/prototype-inheritance.html\n'
             'Rebaselining fast/dom/prototype-taco.html\n'
             'Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n'))

    def test_execute_with_no_issue_number(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.command_options(), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertTrue(logs.startswith('No issue number given and no issue for current branch.'))

    def test_execute_with_issue_number_from_branch(self):
        self.git.get_issue_number = lambda: '11112222'
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.command_options(), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertMultiLineEqual(
            logs,
            ('Tests to rebaseline:\n'
             '  svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-inheritance.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-taco.html: MOCK Try Win (5000)\n'
             'Rebaselining fast/dom/prototype-inheritance.html\n'
             'Rebaselining fast/dom/prototype-taco.html\n'
             'Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n'))

    def test_execute_with_only_changed_tests_option(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.command_options(issue=11112222, only_changed_tests=True), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # is in the list of failed tests, but not in the list of files modified
        # in the given CL; it should be included because all_tests is set to True.
        self.assertMultiLineEqual(
            logs,
            ('Tests to rebaseline:\n'
             '  fast/dom/prototype-inheritance.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-taco.html: MOCK Try Win (5000)\n'
             'Rebaselining fast/dom/prototype-inheritance.html\n'
             'Rebaselining fast/dom/prototype-taco.html\n'))

    def test_execute_with_trigger_jobs_option(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.command_options(issue=11112222, trigger_jobs=True), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        # A message is printed showing that some try jobs are triggered.
        self.assertMultiLineEqual(
            logs,
            ('Triggering try jobs for:\n'
             '  MOCK Try Linux\n'
             'Tests to rebaseline:\n'
             '  svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-inheritance.html: MOCK Try Win (5000)\n'
             '  fast/dom/prototype-taco.html: MOCK Try Win (5000)\n'
             'Rebaselining fast/dom/prototype-inheritance.html\n'
             'Rebaselining fast/dom/prototype-taco.html\n'
             'Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n'))
        # The first executive call, before the rebaseline calls, is triggering try jobs.
        self.assertEqual(
            self.tool.executive.calls,
            [
                ['git', 'cl', 'try', '-b', 'MOCK Try Linux'],
                [
                    ['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'png',
                     'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html'],
                    ['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt',
                     'fast/dom/prototype-inheritance.html'],
                    ['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt',
                     'fast/dom/prototype-taco.html']
                ]
            ])

    def test_rebaseline_calls(self):
        """Tests the list of commands that are invoked when rebaseline is called."""
        # First write test contents to the mock filesystem so that
        # fast/dom/prototype-taco.html is considered a real test to rebaseline.
        # TODO(qyearsley): Change this to avoid accessing protected methods.
        # pylint: disable=protected-access
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            port._filesystem.join(port.layout_tests_dir(), 'fast/dom/prototype-taco.html'),
            'test contents')

        self.command._rebaseline(
            self.command_options(issue=11112222),
            {"fast/dom/prototype-taco.html": {Build("MOCK Try Win", 5000): ["txt", "png"]}})

        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html', '--build-number', '5000']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html', '--build-number', '5000']],
                [['python', 'echo', 'optimize-baselines', '--no-modify-scm', '--suffixes', 'txt', 'fast/dom/prototype-taco.html']]
            ])
