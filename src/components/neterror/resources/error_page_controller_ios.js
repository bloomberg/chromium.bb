// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.errorPageController = {
  // Execute a button click to download page later.
  downloadButtonClick: function() {},

  // Execute a click on the reload button.
  reloadButtonClick: function() {},

  // Execute a "Details" button click.
  detailsButtonClick: function() {},

  // Execute a "Diagnose Errors" button click.
  diagnoseErrorsButtonClick: function() {},

  // ???
  launchOfflineItem: function() {},
  savePageForLater: function() {},
  cancelSavePage: function() {},
  listVisibilityChange: function() {},

  // Track easter egg plays and high scores.
  trackEasterEgg: function() {
    __gCrWeb.message.invokeOnHost(
        {'command': 'errorPageController.trackEasterEgg'});
  },

  updateEasterEggHighScore: function(highScore) {
    __gCrWeb.message.invokeOnHost({
      'command': 'errorPageController.updateEasterEggHighScore',
      'highScore': highScore.toString()
    });
  },

  resetEasterEggHighScore: function() {
    __gCrWeb.message.invokeOnHost(
        {'command': 'errorPageController.resetEasterEggHighScore'});
  }
};

// Create a __gCrWeb binding of initializeEasterEggHighScore so it can be
// called using JS messaging.
__gCrWeb.errorPageController = {};
__gCrWeb['errorPageController'] = __gCrWeb.errorPageController;
__gCrWeb.errorPageController['initializeEasterEggHighScore'] = function(
    highscore) {
  initializeEasterEggHighScore(highscore);
};
