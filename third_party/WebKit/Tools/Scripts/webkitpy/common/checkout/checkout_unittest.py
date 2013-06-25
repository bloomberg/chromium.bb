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
import unittest2 as unittest

from .checkout import Checkout
from .scm import CommitMessage, SCMDetector
from .scm.scm_mock import MockSCM
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem import FileSystem  # FIXME: This should not be needed.
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock


_changelog1entry1 = u"""2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage
"""
_changelog1entry2 = u"""2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py:
"""
_changelog1 = u"\n".join([_changelog1entry1, _changelog1entry2])
_changelog2 = u"""2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Second part of this complicated change by me, Tor Arne Vestb\u00f8!

        * Path/To/Complicated/File: Added.

2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Filler change.
"""

class CommitMessageForThisCommitTest(unittest.TestCase):
    expected_commit_message = u"""Unreviewed build fix to un-break webkit-patch land.

Tools: 

Move commit_message_for_this_commit from scm to checkout
https://bugs.webkit.org/show_bug.cgi?id=36629

* Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage

LayoutTests: 

Second part of this complicated change by me, Tor Arne Vestb\u00f8!

* Path/To/Complicated/File: Added.
"""

    def setUp(self):
        # FIXME: This should not need to touch the filesystem, however
        # ChangeLog is difficult to mock at current.
        self.filesystem = FileSystem()
        self.temp_dir = str(self.filesystem.mkdtemp(suffix="changelogs"))
        self.old_cwd = self.filesystem.getcwd()
        self.filesystem.chdir(self.temp_dir)
        self.webkit_base = WebKitFinder(self.filesystem).webkit_base()

        # Trick commit-log-editor into thinking we're in a Subversion working copy so it won't
        # complain about not being able to figure out what SCM is in use.
        # FIXME: VCSTools.pm is no longer so easily fooled.  It logs because "svn info" doesn't
        # treat a bare .svn directory being part of an svn checkout.
        self.filesystem.maybe_make_directory(".svn")

        self.changelogs = map(self.filesystem.abspath, (self.filesystem.join("Tools", "ChangeLog"), self.filesystem.join("LayoutTests", "ChangeLog")))
        for path, contents in zip(self.changelogs, (_changelog1, _changelog2)):
            self.filesystem.maybe_make_directory(self.filesystem.dirname(path))
            self.filesystem.write_text_file(path, contents)

    def tearDown(self):
        self.filesystem.rmtree(self.temp_dir)
        self.filesystem.chdir(self.old_cwd)


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
