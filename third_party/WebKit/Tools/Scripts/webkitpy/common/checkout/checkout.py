# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import StringIO

from webkitpy.common.config import urls
from webkitpy.common.checkout.scm import CommitMessage
from webkitpy.common.checkout.deps import DEPS
from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError


# This class represents the WebKit-specific parts of the checkout.
# NOTE: All paths returned from this class should be absolute.
class Checkout(object):
    def __init__(self, scm, executive=None, filesystem=None):
        self._scm = scm
        # FIXME: We shouldn't be grabbing at private members on scm.
        self._executive = executive or self._scm._executive
        self._filesystem = filesystem or self._scm._filesystem

    def _modified_files_matching_predicate(self, git_commit, predicate, changed_files=None):
        # SCM returns paths relative to scm.checkout_root
        # Callers (especially those using the ChangeLog class) may
        # expect absolute paths, so this method returns absolute paths.
        if not changed_files:
            changed_files = self._scm.changed_files(git_commit)
        return filter(predicate, map(self._scm.absolute_path, changed_files))

    def recent_commit_infos_for_files(self, paths):
        revisions = set(sum(map(self._scm.revisions_changing_file, paths), []))
        return set(map(self.commit_info_for_revision, revisions))

    def apply_patch(self, patch):
        # It's possible that the patch was not made from the root directory.
        # We should detect and handle that case.
        # FIXME: Move _scm.script_path here once we get rid of all the dependencies.
        # --force (continue after errors) is the common case, so we always use it.
        args = [self._scm.script_path('svn-apply'), "--force"]
        if patch.reviewer():
            args += ['--reviewer', patch.reviewer().full_name]
        self._executive.run_command(args, input=patch.contents(), cwd=self._scm.checkout_root)

    def apply_reverse_diff(self, revision):
        self._scm.apply_reverse_diff(revision)

        conflicts = self._scm.conflicted_files()
        if len(conflicts):
            raise ScriptError(message="Failed to apply reverse diff for revision %s because of the following conflicts:\n%s" % (revision, "\n".join(conflicts)))

    def apply_reverse_diffs(self, revision_list):
        for revision in sorted(revision_list, reverse=True):
            self.apply_reverse_diff(revision)
