# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Note: running this test requires installing the package python-mock.
# pylint: disable=C0103
# pylint: disable=F0401
import PRESUBMIT

import mock
import os.path
import subprocess
import sys
import unittest

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi
from PRESUBMIT_test_mocks import MockOutputApi
from PRESUBMIT_test_mocks import MockAffectedFile


class Capture(object):
    """Class to capture a call argument that can be tested later on."""

    def __init__(self):
        self.value = None

    def __eq__(self, other):
        self.value = other
        return True


class PresubmitTest(unittest.TestCase):

    @mock.patch('subprocess.Popen')
    def testCheckChangeOnUploadWithWebKitAndChromiumFiles(self, _):
        """This verifies that CheckChangeOnUpload will only call check-webkit-style
        on WebKit files.
        """
        diff_file_webkit_h = ['some diff']
        diff_file_chromium_h = ['another diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('FileWebkit.h', diff_file_webkit_h),
            MockAffectedFile('file_chromium.h', diff_file_chromium_h)
        ]
        # Access to a protected member _CheckStyle
        # pylint: disable=W0212
        PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
        capture = Capture()
        # pylint: disable=E1101
        subprocess.Popen.assert_called_with(capture, stderr=-1)
        self.assertEqual(4, len(capture.value))
        self.assertEqual('../../FileWebkit.h', capture.value[3])

    @mock.patch('subprocess.Popen')
    def testCheckChangeOnUploadWithEmptyAffectedFileList(self, _):
        """This verifies that CheckChangeOnUpload will skip calling
        check-webkit-style if the affected file list is empty.
        """
        diff_file_chromium1_h = ['some diff']
        diff_file_chromium2_h = ['another diff']
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('first_file_chromium.h', diff_file_chromium1_h),
            MockAffectedFile('second_file_chromium.h', diff_file_chromium2_h)
        ]
        # Access to a protected member _CheckStyle
        # pylint: disable=W0212
        PRESUBMIT._CheckStyle(mock_input_api, MockOutputApi())
        # pylint: disable=E1101
        subprocess.Popen.assert_not_called()

    def testCheckPublicHeaderWithBlinkMojo(self):
        """This verifies that _CheckForWrongMojomIncludes detects -blink mojo
        headers in public files.
        """

        mock_input_api = MockInputApi()
        potentially_bad_content = '#include "public/platform/modules/cache_storage.mojom-blink.h"'
        mock_input_api.files = [
            MockAffectedFile('third_party/WebKit/public/AHeader.h',
                             [potentially_bad_content], None)
        ]
        # Access to a protected member _CheckForWrongMojomIncludes
        # pylint: disable=W0212
        errors = PRESUBMIT._CheckForWrongMojomIncludes(mock_input_api,
                                                       MockOutputApi())
        self.assertEquals(
            'Public blink headers using Blink variant mojoms found. ' +
            'You must include .mojom-shared.h instead:',
            errors[0].message)

    def testCheckInternalHeaderWithBlinkMojo(self):
        """This verifies that _CheckForWrongMojomIncludes accepts -blink mojo
        headers in blink internal files.
        """

        mock_input_api = MockInputApi()
        potentially_bad_content = '#include "public/platform/modules/cache_storage.mojom-blink.h"'
        mock_input_api.files = [
            MockAffectedFile('third_party/WebKit/Source/public/AHeader.h',
                             [potentially_bad_content], None)
        ]
        # Access to a protected member _CheckForWrongMojomIncludes
        # pylint: disable=W0212
        errors = PRESUBMIT._CheckForWrongMojomIncludes(mock_input_api,
                                                       MockOutputApi())
        self.assertEquals([], errors)

class CxxDependencyTest(unittest.TestCase):
    allow_list = [
        'gfx::ColorSpace',
        'gfx::CubicBezier',
        'gfx::ICCProfile',
        'gfx::ScrollOffset',
        'scoped_refptr<base::SingleThreadTaskRunner>',
    ]
    disallow_list = [
        'GURL',
        'base::Callback<void()>',
        'base::OnceCallback<void()>',
        'content::RenderFrame',
        'gfx::Point',
        'gfx::Rect',
        'net::IPEndPoint',
        'ui::Clipboard',
    ]
    disallow_message = [
    ]

    def runCheck(self, filename, file_contents):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(filename, file_contents),
        ]
        # Access to a protected member
        # pylint: disable=W0212
        return PRESUBMIT._CheckForForbiddenChromiumCode(mock_input_api, MockOutputApi())

    # References in comments should never be checked.
    def testCheckCommentsIgnored(self):
        filename = 'third_party/WebKit/Source/core/frame/frame.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['// %s' % item])
            self.assertEqual([], errors)

    # core, modules, public, et cetera should all have dependency enforcement.
    def testCheckCoreEnforcement(self):
        filename = 'third_party/WebKit/Source/core/frame/frame.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEquals(1, len(errors))
            self.assertRegexpMatches(
                errors[0].message,
                r'^[^:]+:\d+ uses disallowed identifier .+$')

    def testCheckModulesEnforcement(self):
        filename = 'third_party/WebKit/Source/modules/modules_initializer.cc'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEquals(1, len(errors))
            self.assertRegexpMatches(
                errors[0].message,
                r'^[^:]+:\d+ uses disallowed identifier .+$')

    def testCheckPublicEnforcement(self):
        filename = 'third_party/WebKit/Source/public/platform/WebThread.h'
        for item in self.allow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEqual([], errors)

        for item in self.disallow_list:
            errors = self.runCheck(filename, ['%s' % item])
            self.assertEquals(1, len(errors))
            self.assertRegexpMatches(
                errors[0].message,
                r'^[^:]+:\d+ uses disallowed identifier .+$')

    # platform and controller should be opted out of enforcement, but aren't
    # currently checked because the PRESUBMIT test mocks are missing too
    # much functionality...

    # External module checks should not affect CSS files.
    def testCheckCSSIgnored(self):
        filename = 'third_party/WebKit/Source/devtools/front_end/timeline/someFile.css'
        errors = self.runCheck(filename, ['.toolbar::after { color: pink; }\n'])
        self.assertEqual([], errors)

if __name__ == '__main__':
    unittest.main()
