// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Id for tracking automatic refresh of crash list.  */
let refreshCrashListId = undefined;

/**
 * Requests the list of crashes from the backend.
 */
function requestCrashes() {
  chrome.send('requestCrashList');
}

/**
 * Callback from backend with the list of crashes. Builds the UI.
 * @param {boolean} enabled Whether or not crash reporting is enabled.
 * @param {boolean} dynamicBackend Whether the crash backend is dynamic.
 * @param {boolean} manualUploads Whether the manual uploads are supported.
 * @param {array} crashes The list of crashes.
 * @param {string} version The browser version.
 * @param {string} os The OS name and version.
 * @param {boolean} isGoogleAccount whether primary account is internal.
 */
function updateCrashList(
    enabled, dynamicBackend, manualUploads,
    crashes, version, os, isGoogleAccount) {
  $('crashesCount').textContent = loadTimeData.getStringF(
      'crashCountFormat', crashes.length.toLocaleString());

  const crashList = $('crashList');

  $('disabledMode').hidden = enabled;
  $('crashUploadStatus').hidden = !enabled || !dynamicBackend;

  const template = crashList.getElementsByTagName('template')[0];

  // Clear any previous list.
  crashList.querySelectorAll('.crashRow').forEach((elm) => elm.remove());

  const productName = loadTimeData.getString('shortProductName');

  for (let i = 0; i < crashes.length; i++) {
    const crash = crashes[i];
    if (crash.local_id === '') {
      crash.local_id = productName;
    }

    const crashRow = template.content.cloneNode(true);
    if (crash.state !== 'uploaded') {
      crashRow.querySelector('.crashRow').classList.add('notUploaded');
    }

    const uploaded = crash.state === 'uploaded';

    // Some clients do not distinguish between capture time and upload time,
    // so use the latter if the former is not available.
    crashRow.querySelector('.captureTime').textContent =
        loadTimeData.getStringF(
            'crashCaptureTimeFormat',
            crash.capture_time || crash.upload_time || '');
    crashRow.querySelector('.localId .value').textContent = crash.local_id;

    let stateText = '';
    switch (crash.state) {
      case 'not_uploaded':
        stateText = loadTimeData.getString('crashStatusNotUploaded');
        break;
      case 'pending':
        stateText = loadTimeData.getString('crashStatusPending');
        break;
      case 'pending_user_requested':
        stateText = loadTimeData.getString('crashStatusPendingUserRequested');
        break;
      case 'uploaded':
        stateText = loadTimeData.getString('crashStatusUploaded');
        break;
      default:
        continue;  // Unknown state.
    }
    crashRow.querySelector('.status .value').textContent = stateText;

    const uploadId = crashRow.querySelector('.uploadId');
    const uploadTime = crashRow.querySelector('.uploadTime');
    const sendNowButton = crashRow.querySelector('.sendNow');
    const fileBugButton = crashRow.querySelector('.fileBug');
    if (uploaded) {
      const uploadIdValue = uploadId.querySelector('.value');
      if (isGoogleAccount) {
        const crashLink = document.createElement('a');
        crashLink.href = `https://goto.google.com/crash/${crash.id}`;
        crashLink.target = '_blank';
        crashLink.textContent = crash.id;
        uploadIdValue.appendChild(crashLink);
      } else {
        uploadIdValue.textContent = crash.id;
      }

      uploadTime.querySelector('.value').textContent = crash.upload_time;

      sendNowButton.remove();
      fileBugButton.onclick = () => fileBug(crash.id, os, version);
    } else {
      uploadId.remove();
      uploadTime.remove();
      fileBugButton.remove();
      // Do not allow crash submission if the Chromium build does not support
      // it, or if the user already requested it.
      if (!manualUploads || crash.state === 'pending_user_requested') {
        sendNowButton.remove();
      }
      sendNowButton.onclick = (e) => {
        e.target.disabled = true;
        chrome.send('requestSingleCrashUpload', [crash.local_id]);
      };
    }

    const fileSize = crashRow.querySelector('.fileSize');
    if (crash.file_size === '') {
      fileSize.remove();
    } else {
      fileSize.querySelector('.value').textContent = crash.file_size;
    }

    crashList.appendChild(crashRow);
  }

  $('noCrashes').hidden = crashes.length !== 0;
}

/**
 * Opens a new tab/window to report the crash to crbug.
 * @param {string} The crash report ID.
 * @param {string} The OS name.
 * @param {string} The product version.
 */
function fileBug(crashId, os, version) {
  const commentLines = [
    'IMPORTANT: Your crash has already been automatically reported ' +
    'to our crash system. Please file this bug only if you can provide ' +
    'more information about it.',
    '',
    '',
    'Chrome Version: ' + version,
    'Operating System: ' + os,
    '',
    'URL (if applicable) where crash occurred:',
    '',
    'Can you reproduce this crash?',
    '',
    'What steps will reproduce this crash? (If it\'s not ' +
    'reproducible, what were you doing just before the crash?)',
    '1.', '2.', '3.',
    '',
    '****DO NOT CHANGE BELOW THIS LINE****',
    'Crash ID: crash/' + crashId
  ];
  const params = {
    template: 'Crash Report',
    comment: commentLines.join('\n'),
    // TODO(scottmg): Use add_labels to add 'User-Submitted' rather than
    // duplicating the template's labels (the first two) once
    // https://bugs.chromium.org/p/monorail/issues/detail?id=1488 is done.
    labels: 'Restrict-View-EditIssue,Stability-Crash,User-Submitted',
  };
  let href = 'https://bugs.chromium.org/p/chromium/issues/entry';
  for (const param in params) {
    href = appendParam(href, param, params[param]);
  }

  window.open(href);
}

/**
 * Request crashes get uploaded in the background.
 */
function requestCrashUpload() {
  // Don't need locking with this call because the system crash reporter
  // has locking built into itself.
  chrome.send('requestCrashUpload');

  // Trigger a refresh in 5 seconds.  Clear any previous requests.
  clearTimeout(refreshCrashListId);
  refreshCrashListId = setTimeout(requestCrashes, 5000);
}

/**
 * Toggles hiding/showing the developer details of a crash report, depending
 * on the value of the check box.
 * @param {Event} The DOM event for onclick.
 */
function toggleDevDetails(e) {
  $('crashList').classList.toggle('showingDevDetails', e.target.checked);
}

document.addEventListener('DOMContentLoaded', function() {
  $('uploadCrashes').onclick = requestCrashUpload;
  $('showDevDetails').onclick = toggleDevDetails;
  requestCrashes();
});
