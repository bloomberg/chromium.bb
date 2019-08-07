# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Android Java code.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.

This presubmit checks for the following:
  - No new calls to Notification.Builder or NotificationCompat.Builder
    constructors. Callers should use ChromeNotificationBuilder instead.
  - No new calls to AlertDialog.Builder. Callers should use ModalDialogView
    instead.
"""

import re

NEW_NOTIFICATION_BUILDER_RE = re.compile(
    r'\bnew\sNotification(Compat)?\.Builder\b')

NEW_ALERTDIALOG_BUILDER_RE = re.compile(
    r'\bnew\sAlertDialog\.Builder\b')

COMMENT_RE = re.compile(r'^\s*(//|/\*|\*)')

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckNotificationConstructors(input_api, output_api))
  result.extend(_CheckAlertDialogBuilder(input_api, output_api))
  # Add more checks here
  return result

def _CheckNotificationConstructors(input_api, output_api):
  # "Blacklist" because the following files are excluded from the check.
  blacklist = (
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
          'NotificationBuilder.java',
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
          'NotificationCompatBuilder.java'
  )
  error_msg = '''
  Android Notification Construction Check failed:
  Your new code added one or more calls to the Notification.Builder and/or
  NotificationCompat.Builder constructors, listed below.

  This is banned, please construct notifications using
  NotificationBuilderFactory.createChromeNotificationBuilder instead,
  specifying a channel for use on Android O.

  See https://crbug.com/678670 for more information.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                               NEW_NOTIFICATION_BUILDER_RE)


def _CheckAlertDialogBuilder(input_api, output_api):
  browser_root = 'chrome/android/java/src/org/chromium/chrome/browser/'

  # "Blacklist" because the following files are excluded from the check.
  blacklist = (
      browser_root + 'LoginPrompt.java',
      browser_root + 'SSLClientCertificateRequest.java',
      browser_root + 'RepostFormWarningDialog.java',
      browser_root + 'autofill/AutofillPopupBridge.java',
      browser_root + 'autofill/CardUnmaskPrompt.java',
      browser_root + 'autofill/keyboard_accessory/'
          'AutofillKeyboardAccessoryBridge.java',
      browser_root + 'browserservices/ClearDataDialogActivity.java',
      browser_root + 'datausage/DataUseTabUIManager.java',
      browser_root + 'dom_distiller/DistilledPagePrefsView.java',
      browser_root + 'dom_distiller/DomDistillerUIUtils.java',
      browser_root + 'download/DownloadController.java',
      browser_root + 'download/OMADownloadHandler.java',
      browser_root + 'externalnav/ExternalNavigationDelegateImpl.java',
      browser_root + 'init/InvalidStartupDialog.java',
      browser_root + 'omnibox/SuggestionView.java',
      browser_root + 'payments/AndroidPaymentApp.java',
      browser_root + 'password_manager/AccountChooserDialog.java',
      browser_root + 'password_manager/AutoSigninFirstRunDialog.java',
      browser_root + 'permissions/AndroidPermissionRequester.java',
      browser_root + r'preferences[\\\/].*',
      browser_root + 'share/ShareHelper.java',
      browser_root + 'signin/AccountAdder.java',
      browser_root + 'signin/AccountPickerDialogFragment.java',
      browser_root + 'signin/AccountSigninView.java',
      browser_root + 'signin/ConfirmImportSyncDataDialog.java',
      browser_root + 'signin/ConfirmManagedSyncDataDialog.java',
      browser_root + 'signin/ConfirmSyncDataStateMachineDelegate.java',
      browser_root + 'signin/SigninFragmentBase.java',
      browser_root + 'signin/SignOutDialogFragment.java',
      browser_root + 'sync/ui/PassphraseCreationDialogFragment.java',
      browser_root + 'sync/ui/PassphraseDialogFragment.java',
      browser_root + 'sync/ui/PassphraseTypeDialogFragment.java',
      browser_root + 'util/AccessibilityUtil.java',
      browser_root + 'webapps/AddToHomescreenDialog.java',
      browser_root + 'webapps/WebappOfflineDialog.java',
  )
  error_msg = '''
  AlertDialog.Builder Check failed:
  Your new code added one or more calls to the AlertDialog.Builder, listed
  below.

  This breaks browsing when in VR, please use ModalDialogView instead of
  AlertDialog.
  Contact asimjour@chromium.org if you have any questions.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                               NEW_ALERTDIALOG_BUILDER_RE)

def _CheckReIgnoreComment(input_api, output_api, error_msg, blacklist,
                          regular_expression):
  problems = []
  sources = lambda x: input_api.FilterSourceFile(
      x, white_list=(r'.*\.java$',), black_list=blacklist)
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=sources):
    for line_number, line in f.ChangedContents():
      if (regular_expression.search(line)
          and not COMMENT_RE.search(line)):
        problems.append(
          '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if problems:
    return [output_api.PresubmitError(
      error_msg,
      problems)]
  return []
