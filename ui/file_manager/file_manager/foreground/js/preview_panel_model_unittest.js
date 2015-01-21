// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Timeout in milliseconds to determine if the event is dispatched or not.
 * If the event is dispached in the timeout, the test regards the event is
 * dispatched, otherwise not. Otherwise it regards the event is not dispatched.
 */
var TIMEOUT_MS = 3000;

function testPreviewPanelModel(callback) {
  document.querySelector('body').innerHTML =
      '<command class="cloud-import" hidden></command>';
  var command = document.querySelector('command.cloud-import');
  var model = new PreviewPanelModel(PreviewPanelModel.VisibilityType.AUTO);

  var testStep = function(step, changed) {
    var waitPromise = new Promise(function(fulfill) {
      model.addEventListener(
          PreviewPanelModel.EventType.CHANGE, fulfill.bind(null, true));
      setTimeout(fulfill.bind(null, false), TIMEOUT_MS);
    });

    switch (step) {
      case 0:
        assertFalse(model.visible);
        model.setSelection({entries: ["NotEmpty"]});
        break;
      case 1:
        assertTrue(changed);
        assertTrue(model.visible);
        model.setSelection({entries: []});
        break;
      case 2:
        assertTrue(changed);
        assertFalse(model.visible);
        command.setHidden(false);
        break;
      case 3:
        assertTrue(changed);
        assertTrue(model.visible);
        command.setHidden(true);
        break;
      case 4:
        assertTrue(changed);
        assertFalse(model.visible);
        model.setVisibilityType(
            PreviewPanelModel.VisibilityType.ALWAYS_VISIBLE);
        break;
      case 5:
        assertTrue(changed);
        assertTrue(model.visible);
        model.setVisibilityType(
            PreviewPanelModel.VisibilityType.ALWAYS_HIDDEN);
        model.setSelection({entries: ["NotEmpty"]});
        break;
      case 6:
        assertTrue(changed);
        assertFalse(model.visible);
        command.setHidden(false);
        break;
      case 7:
        assertFalse(changed);
        assertFalse(model.visible);
        return;
    }

    return waitPromise.then(function(newChanged) {
      return testStep(step + 1, newChanged);
    });
  };

  reportPromise(testStep(0), callback);
}
