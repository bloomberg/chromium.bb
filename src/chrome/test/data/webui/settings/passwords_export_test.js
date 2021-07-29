// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MockTimer} from 'chrome://test/mock_timer.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';
// clang-format on

// Test that tapping "Export passwords..." notifies the browser.
export function runStartExportTest(exportDialog, passwordManager, done) {
  passwordManager.exportPasswords = (callback) => {
    callback();
    done();
  };

  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
}

// Test the export flow. If exporting is fast, we should skip the
// in-progress view altogether.
export function runExportFlowFastTest(exportDialog, passwordManager, done) {
  const progressCallback = passwordManager.progressCallback;

  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();
  mockTimer.install();

  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});

  flush();
  // When we are done, the export dialog closes completely.
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_error'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_progress'));
  done();

  mockTimer.uninstall();
}

// The error view is shown when an error occurs.
export function runExportFlowErrorTest(exportDialog, passwordManager, done) {
  const progressCallback = passwordManager.progressCallback;

  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();
  mockTimer.install();

  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  progressCallback({
    status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
    folderName: 'tmp',
  });

  flush();
  // Test that the error dialog is shown.
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_error').open);
  // Test that the error dialog can be dismissed.
  exportDialog.shadowRoot.querySelector('#cancelErrorButton').click();
  flush();
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_error'));
  done();

  mockTimer.uninstall();
}

// The error view allows to retry.
export function runExportFlowErrorRetryTest(
    exportDialog, passwordManager, done) {
  const progressCallback = passwordManager.progressCallback;
  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();

  new Promise(resolve => {
    mockTimer.install();

    passwordManager.exportPasswords = resolve;
    exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  }).then(() => {
    // This wait allows the BlockingRequestManager to process the click if
    // the test is running in ChromeOS.
    progressCallback(
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    progressCallback({
      status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
      folderName: 'tmp',
    });

    flush();
    // Test that the error dialog is shown.
    assertTrue(exportDialog.shadowRoot.querySelector('#dialog_error').open);
    // Test that clicking retry will start a new export.
    passwordManager.exportPasswords = (callback) => {
      callback();
      done();
    };
    exportDialog.shadowRoot.querySelector('#tryAgainButton').click();

    mockTimer.uninstall();
  });
}

// Test the export flow. If exporting is slow, Chrome should show the
// in-progress dialog for at least 1000ms.
export function runExportFlowSlowTest(exportDialog, passwordManager, done) {
  const progressCallback = passwordManager.progressCallback;

  const mockTimer = new MockTimer();
  mockTimer.install();

  // The initial dialog remains open for 100ms after export enters the
  // in-progress state.
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);

  // After 100ms of not having completed, the dialog switches to the
  // progress bar. Chrome will continue to show the progress bar for 1000ms,
  // despite a completion event.
  mockTimer.tick(99);
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_start').open);
  mockTimer.tick(1);
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_progress').open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_progress').open);

  // After 1000ms, Chrome will display the completion event.
  mockTimer.tick(999);
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_progress').open);
  mockTimer.tick(1);
  flush();
  // On SUCCEEDED the dialog closes completely.
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_progress'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_error'));
  done();

  mockTimer.uninstall();
}

// Test that canceling the dialog while exporting will also cancel the
// export on the browser.
export function runCancelExportTest(exportDialog, passwordManager, done) {
  const progressCallback = passwordManager.progressCallback;

  passwordManager.cancelExportPasswords = () => {
    done();
  };

  const mockTimer = new MockTimer();
  mockTimer.install();

  // The initial dialog remains open for 100ms after export enters the
  // in-progress state.
  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  // The progress bar only appears after 100ms.
  mockTimer.tick(100);
  flush();
  assertTrue(exportDialog.shadowRoot.querySelector('#dialog_progress').open);
  exportDialog.shadowRoot.querySelector('#cancel_progress_button').click();

  flush();
  // The dialog should be dismissed entirely.
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_progress'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot.querySelector('#dialog_error'));

  mockTimer.uninstall();
}

export function runFireCloseEventAfterExportCompleteTest(
    exportDialog, passwordManager) {
  const wait = eventToPromise('passwords-export-dialog-close', exportDialog);
  exportDialog.shadowRoot.querySelector('#exportPasswordsButton').click();
  passwordManager.progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  passwordManager.progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
  return wait;
}
