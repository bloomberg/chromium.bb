# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import codecs
import os
import shutil
import tempfile
import webkitpy.thirdparty.unittest2 as unittest

from .checkout import Checkout
from .scm import SCMDetector
from .scm.scm_mock import MockSCM
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem import FileSystem  # FIXME: This should not be needed.
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock


class CheckoutTest(unittest.TestCase):
    def _make_checkout(self):
        return Checkout(scm=MockSCM(), filesystem=MockFileSystem(), executive=MockExecutive())

    def test_apply_patch(self):
        checkout = self._make_checkout()
        checkout._executive = MockExecutive(should_log=True)
        checkout._scm.script_path = lambda script: script
        mock_patch = Mock()
        mock_patch.contents = lambda: "foo"
        mock_patch.reviewer = lambda: None
        expected_logs = "MOCK run_command: ['svn-apply', '--force'], cwd=/mock-checkout, input=foo\n"
        OutputCapture().assert_outputs(self, checkout.apply_patch, [mock_patch], expected_logs=expected_logs)
