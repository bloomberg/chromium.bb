// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
/**
 * CSS classes for different statuses.
 * @enum {string}
 */
const StatusClass = {
  GOOD: 'good',
  BAD: 'bad',
  MEDIUM: 'medium',
  INFO: 'info'
};

/**
 * Adds a row to the sandbox status table.
 * @param {string} name The name of the status item.
 * @param {string} value The status of the item.
 * @param {string?} cssClass A CSS class to apply to the row.
 * @return {Element} The newly added TR.
 */
function addStatusRow(name, value, cssClass) {
  let row = cr.doc.createElement('tr');

  let nameCol = row.appendChild(cr.doc.createElement('td'));
  let valueCol = row.appendChild(cr.doc.createElement('td'));

  nameCol.textContent = name;
  valueCol.textContent = value;

  if (cssClass != null) {
    nameCol.classList.add(cssClass);
    valueCol.classList.add(cssClass);
  }

  $('sandbox-status').appendChild(row);
  return row;
}

/**
 * Adds a status row that reports either Yes or No.
 * @param {string} name The name of the status item.
 * @param {boolean} result The status (good/bad) result.
 * @return {Element} The newly added TR.
 */
function addGoodBadRow(name, result) {
  return addStatusRow(
      name, result ? 'Yes' : 'No', result ? StatusClass.GOOD : StatusClass.BAD);
}

/**
 * Reports the overall sandbox status evaluation message.
 * @param {boolean}
 */
function setEvaluation(result) {
  let message = result ? 'You are adequately sandboxed.' :
                         'You are NOT adequately sandboxed.';
  $('evaluation').innerText = message;
}

/**
 * Main page handler for Android.
 */
function androidHandler() {
  chrome.getAndroidSandboxStatus((status) => {
    var isIsolated = false;
    var isTsync = false;
    var isChromeSeccomp = false;

    addStatusRow('PID', status.pid, StatusClass.INFO);
    addStatusRow('UID', status.uid, StatusClass.INFO);
    isIsolated = status.secontext.indexOf(':isolated_app:') != -1;
    addStatusRow(
        'SELinux Context', status.secontext,
        isIsolated ? StatusClass.GOOD : StatusClass.BAD);

    let procStatus = status.procStatus.split('\n');
    for (let line of procStatus) {
      if (line.startsWith('Seccomp')) {
        var value = line.split(':')[1].trim();
        var cssClass = StatusClass.BAD;
        if (value == '2') {
          value = 'Yes - TSYNC (' + line + ')';
          cssClass = StatusClass.GOOD;
          isTsync = true;
        } else if (value == '1') {
          value = 'Yes (' + line + ')';
        } else {
          value = line;
        }
        addStatusRow('Seccomp-BPF Enabled (Kernel)', value, cssClass);
        break;
      }
    }

    var seccompStatus = 'Unknown';
    switch (status.seccompStatus) {
      case 0:
        seccompStatus = 'Not Supported';
        break;
      case 1:
        seccompStatus = 'Run-time Detection Failed';
        break;
      case 2:
        seccompStatus = 'Disabled by Field Trial';
        break;
      case 3:
        seccompStatus = 'Enabled by Field Trial (not started)';
        break;
      case 4:
        seccompStatus = 'Sandbox Started';
        isChromeSeccomp = true;
        break;
    }
    addStatusRow(
        'Seccomp-BPF Enabled (Chrome)', seccompStatus,
        status.seccompStatus == 4 ? StatusClass.GOOD : StatusClass.BAD);

    addStatusRow('Android Build ID', status.androidBuildId, StatusClass.INFO);

    setEvaluation(isIsolated && isTsync && isChromeSeccomp);
  });
}

/**
 * Main page handler for desktop Linux.
 */
function linuxHandler() {
  let suidSandbox = loadTimeData.getBoolean('suid');
  let nsSandbox = loadTimeData.getBoolean('userNs');

  let layer1SandboxType = 'None';
  let layer1SandboxCssClass = StatusClass.BAD;
  if (suidSandbox) {
    layer1SandboxType = 'SUID';
    layer1SandboxCssClass = StatusClass.MEDIUM;
  } else if (nsSandbox) {
    layer1SandboxType = 'Namespace';
    layer1SandboxCssClass = StatusClass.GOOD;
  }

  addStatusRow('Layer 1 Sandbox', layer1SandboxType, layer1SandboxCssClass);
  addGoodBadRow('PID namespaces', loadTimeData.getBoolean('pidNs'));
  addGoodBadRow('Network namespaces', loadTimeData.getBoolean('netNs'));
  addGoodBadRow('Seccomp-BPF sandbox', loadTimeData.getBoolean('seccompBpf'));
  addGoodBadRow(
      'Seccomp-BPF sandbox supports TSYNC',
      loadTimeData.getBoolean('seccompTsync'));

  let enforcingYamaBroker = loadTimeData.getBoolean('yamaBroker');
  addGoodBadRow(
      'Ptrace Protection with Yama LSM (Broker)', enforcingYamaBroker);

  let enforcingYamaNonbroker = loadTimeData.getBoolean('yamaNonbroker');
  // If there is no ptrace protection anywhere, that is bad.
  // If there is no ptrace protection for nonbroker processes because of the
  // user namespace sandbox, that is fine and we display as medium.
  let yamaNonbrokerCssClass = enforcingYamaBroker ?
      (enforcingYamaNonbroker ? StatusClass.GOOD : StatusClass.MEDIUM) :
      StatusClass.BAD;
  addStatusRow(
      'Ptrace Protection with Yama LSM (Non-broker)',
      enforcingYamaNonbroker ? 'Yes' : 'No', yamaNonbrokerCssClass);

  setEvaluation(loadTimeData.getBoolean('sandboxGood'));
}

document.addEventListener('DOMContentLoaded', () => {
  if (cr.isAndroid) {
    androidHandler();
  } else {
    linuxHandler();
  }
});
})();
