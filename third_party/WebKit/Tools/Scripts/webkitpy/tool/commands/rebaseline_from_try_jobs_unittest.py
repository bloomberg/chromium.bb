# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline_from_try_jobs import RebaselineFromTryJobs
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase
from webkitpy.tool.mock_tool import MockOptions
from webkitpy.layout_tests.builder_list import BuilderList


class RebaselineFromTryJobsTest(BaseTestCase):
    command_constructor = RebaselineFromTryJobs

    def setUp(self):
        super(RebaselineFromTryJobsTest, self).setUp()
        self.command.web = MockWeb(urls={
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
            }),
        })
        self.tool.builders = BuilderList({
            "MOCK Try Win": {
                "port_name": "test-win-win7",
                "specifiers": ["Win7", "Release"],
                "is_try_builder": True,
            },
            "MOCK Try Mac": {
                "port_name": "test-mac-mac10.10",
                "specifiers": ["Mac10.10", "Release"],
                "is_try_builder": True,
            },
        })

    def test_execute_with_issue_number_given(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            options = MockOptions(issue=11112222, dry_run=False, optimize=True, verbose=False)
            self.command.execute(options, [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(
            logs,
            ('Tests to rebaseline:\n'
             '  svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html: MOCK Try Win\n'
             '  fast/dom/prototype-inheritance.html: MOCK Try Win\n'
             '  fast/dom/prototype-taco.html: MOCK Try Win\n'
             'Rebaselining fast/dom/prototype-inheritance.html\n'
             'Rebaselining fast/dom/prototype-taco.html\n'
             'Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n'))

    def test_execute_with_no_issue_number(self):
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(MockOptions(issue=None), [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
        self.assertEqual(logs, 'No issue number provided.\n')
