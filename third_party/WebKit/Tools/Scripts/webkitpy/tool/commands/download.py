# Copyright (c) 2009, 2011 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import logging

from webkitpy.tool import steps

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.config import urls
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.tool.commands.stepsequence import StepSequence
from webkitpy.tool.comments import bug_comment_from_commit_text
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand

_log = logging.getLogger(__name__)


class Clean(AbstractSequencedCommand):
    name = "clean"
    help_text = "Clean the working copy"
    steps = [
        steps.DiscardLocalChanges,
    ]

    def _prepare_state(self, options, args, tool):
        options.force_clean = True


class Update(AbstractSequencedCommand):
    name = "update"
    help_text = "Update working copy (used internally)"
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
    ]


class Build(AbstractSequencedCommand):
    name = "build"
    help_text = "Update working copy and build"
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.Build,
    ]

    def _prepare_state(self, options, args, tool):
        options.build = True


class BuildAndTest(AbstractSequencedCommand):
    name = "build-and-test"
    help_text = "Update working copy, build, and run the tests"
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.Build,
        steps.RunTests,
    ]


class Land(AbstractSequencedCommand):
    name = "land"
    help_text = "Land the current working directory diff and updates the associated bug if any"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        steps.AddSvnMimetypeForPng,
        steps.UpdateChangeLogsWithReviewer,
        steps.ValidateReviewer,
        steps.ValidateChangeLogs, # We do this after UpdateChangeLogsWithReviewer to avoid not having to cache the diff twice.
        steps.Build,
        steps.RunTests,
        steps.Commit,
        steps.CloseBugForLandDiff,
    ]
    long_help = """land commits the current working copy diff (just as svn or git commit would).
land will NOT build and run the tests before committing, but you can use the --build option for that.
If a bug id is provided, or one can be found in the ChangeLog land will update the bug after committing."""

    def _prepare_state(self, options, args, tool):
        changed_files = self._tool.scm().changed_files(options.git_commit)
        return {
            "changed_files": changed_files,
            "bug_id": (args and args[0]) or tool.checkout().bug_id_for_this_commit(options.git_commit, changed_files),
        }


class LandCowboy(AbstractSequencedCommand):
    name = "land-cowboy"
    help_text = "Prepares a ChangeLog and lands the current working directory diff."
    steps = [
        steps.PrepareChangeLog,
        steps.EditChangeLog,
        steps.CheckStyle,
        steps.ConfirmDiff,
        steps.Build,
        steps.RunTests,
        steps.Commit,
        steps.CloseBugForLandDiff,
    ]

    def _prepare_state(self, options, args, tool):
        options.check_style_filter = "-changelog"


class LandCowhand(LandCowboy):
    # Gender-blind term for cowboy, see: http://en.wiktionary.org/wiki/cowhand
    name = "land-cowhand"


class CheckStyleLocal(AbstractSequencedCommand):
    name = "check-style-local"
    help_text = "Run check-webkit-style on the current working directory diff"
    steps = [
        steps.CheckStyle,
    ]


class AbstractPatchProcessingCommand(AbstractDeclarativeCommand):
    # Subclasses must implement the methods below.  We don't declare them here
    # because we want to be able to implement them with mix-ins.
    #
    # pylint: disable=E1101
    # def _fetch_list_of_patches_to_process(self, options, args, tool):
    # def _prepare_to_process(self, options, args, tool):
    # def _process_patch(self, options, args, tool):

    @staticmethod
    def _collect_patches_by_bug(patches):
        bugs_to_patches = {}
        for patch in patches:
            bugs_to_patches[patch.bug_id()] = bugs_to_patches.get(patch.bug_id(), []) + [patch]
        return bugs_to_patches

    def execute(self, options, args, tool):
        self._prepare_to_process(options, args, tool)
        patches = self._fetch_list_of_patches_to_process(options, args, tool)

        # It's nice to print out total statistics.
        bugs_to_patches = self._collect_patches_by_bug(patches)
        _log.info("Processing %s from %s." % (pluralize("patch", len(patches)), pluralize("bug", len(bugs_to_patches))))

        for patch in patches:
            self._process_patch(patch, options, args, tool)


class AbstractPatchSequencingCommand(AbstractPatchProcessingCommand):
    prepare_steps = None
    main_steps = None

    def __init__(self):
        options = []
        self._prepare_sequence = StepSequence(self.prepare_steps)
        self._main_sequence = StepSequence(self.main_steps)
        options = sorted(set(self._prepare_sequence.options() + self._main_sequence.options()))
        AbstractPatchProcessingCommand.__init__(self, options)

    def _prepare_to_process(self, options, args, tool):
        self._prepare_sequence.run_and_handle_errors(tool, options)

    def _process_patch(self, patch, options, args, tool):
        state = { "patch" : patch }
        self._main_sequence.run_and_handle_errors(tool, options, state)


class ProcessAttachmentsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class ProcessBugsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for bug_id in args:
            patches = tool.bugs.fetch_bug(bug_id).reviewed_patches()
            _log.info("%s found on bug %s." % (pluralize("reviewed patch", len(patches)), bug_id))
            all_patches += patches
        if not all_patches:
            _log.info("No reviewed patches found, looking for unreviewed patches.")
            for bug_id in args:
                patches = tool.bugs.fetch_bug(bug_id).patches()
                _log.info("%s found on bug %s." % (pluralize("patch", len(patches)), bug_id))
                all_patches += patches
        return all_patches


class ProcessURLsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for url in args:
            bug_id = urls.parse_bug_id(url)
            if bug_id:
                patches = tool.bugs.fetch_bug(bug_id).patches()
                _log.info("%s found on bug %s." % (pluralize("patch", len(patches)), bug_id))
                all_patches += patches

            attachment_id = urls.parse_attachment_id(url)
            if attachment_id:
                all_patches += tool.bugs.fetch_attachment(attachment_id)

        return all_patches


class CheckStyle(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "check-style"
    help_text = "Run check-webkit-style on the specified attachments"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.CheckStyle,
    ]


class BuildAttachment(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "build-attachment"
    help_text = "Apply and build patches from bugzilla"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.Build,
    ]


class BuildAndTestAttachment(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "build-and-test-attachment"
    help_text = "Apply, build, and test patches from bugzilla"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.Build,
        steps.RunTests,
    ]


class AbstractPatchApplyingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        steps.EnsureLocalCommitIfNeeded,
        steps.CleanWorkingDirectory,
        steps.Update,
    ]
    main_steps = [
        steps.ApplyPatchWithLocalCommit,
    ]
    long_help = """Updates the working copy.
Downloads and applies the patches, creating local commits if necessary."""


class ApplyAttachment(AbstractPatchApplyingCommand, ProcessAttachmentsMixin):
    name = "apply-attachment"
    help_text = "Apply an attachment to the local working directory"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    show_in_main_help = True


class ApplyFromBug(AbstractPatchApplyingCommand, ProcessBugsMixin):
    name = "apply-from-bug"
    help_text = "Apply reviewed patches from provided bugs to the local working directory"
    argument_names = "BUGID [BUGIDS]"
    show_in_main_help = True


class ApplyWatchList(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "apply-watchlist"
    help_text = "Applies the watchlist to the specified attachments"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.ApplyWatchList,
    ]
    long_help = """"Applies the watchlist to the specified attachments.
Downloads the attachment, applies it locally, runs the watchlist against it, and updates the bug with the result."""
