// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addLog(logText) {
  const logDiv = $('log-entries');
  if (!logDiv) {
    return;
  }
  logDiv.appendChild(document.createElement('hr'));
  const textDiv = document.createElement('div');
  textDiv.innerText = logText;
  logDiv.appendChild(textDiv);
}

function setUpAutofillInternals() {
    document.title = "Autofill Internals";
    document.getElementById("h1-title").innerHTML = "Autofill Internals";
    document.getElementById("logging-note").innerText =
      "Captured autofill logs are listed below. Logs are cleared and no longer \
      captured when all autofill-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured autofill logs are not available in Incognito.";
}

function setUpPasswordManagerInternals() {
    document.title = "Password Manager Internals";
    document.getElementById("h1-title").innerHTML =
      "Password Manager Internals";
    document.getElementById("logging-note").innerText =
      "Captured password manager logs are listed below. Logs are cleared and \
      no longer captured when all password-manager-internals pages are closed.";
    document.getElementById("logging-note-incognito").innerText =
      "Captured password manager logs are not available in Incognito.";
}

function notifyAboutIncognito(isIncognito) {
  document.body.dataset.incognito = isIncognito;
}