// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const HIDDEN_CLASS = 'hidden';

function setupEvents() {
  $('details-certerror-button').addEventListener('click', function(event) {
    toggleHidden('details-certerror', 'details-certerror-button');
  });
  $('details-connectnetwork-button').addEventListener('click', function(event) {
    toggleHidden('details-connectnetwork', 'details-connectnetwork-button');
  });
  $('details-clock-button').addEventListener('click', function(event) {
    toggleHidden('details-clock', 'details-clock-button');
  });
  if (loadTimeData.getBoolean('isWindows')) {
    $('windows-only').classList.remove(HIDDEN_CLASS);
    $('details-mitmsoftware-button').addEventListener('click', function(event) {
      toggleHidden('details-mitmsoftware', 'details-mitmsoftware-button');
    });
  }
  switch (window.location.hash) {
    case '#' + loadTimeData.getInteger('certCommonNameInvalid'):
    case '#' + loadTimeData.getInteger('certAuthorityInvalid'):
    case '#' + loadTimeData.getInteger('certWeakSignatureAlgorithm'):
    case '#' + loadTimeData.getInteger('certKnownInterceptionBlocked'):
      toggleHidden('details-certerror', 'details-certerror-button');
      break;
    case '#' + loadTimeData.getInteger('certExpired'):
      toggleHidden('details-clock', 'details-clock-button');
      break;
  }
}

function toggleHidden(className, buttonName) {
  const hiddenDetails = $(className).classList.toggle(HIDDEN_CLASS);
  $(buttonName).innerText = hiddenDetails ?
      loadTimeData.getString('connectionHelpShowMore') :
      loadTimeData.getString('connectionHelpShowLess');
}

document.addEventListener('DOMContentLoaded', setupEvents);
