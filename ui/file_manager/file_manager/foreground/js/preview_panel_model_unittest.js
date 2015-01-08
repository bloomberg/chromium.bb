// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var waitFulfill = null;

function testPreviewPanelModel() {
  var command = util.queryDecoratedElement('#test-command', cr.ui.Command);
  command.disabled = true;
  var model = new PreviewPanelModel(
      PreviewPanelModel.VisibilityType.AUTO,
      [command]);
  var visibleList = [];
  model.addEventListener(PreviewPanelModel.EventType.CHANGE, function() {
    visibleList.push(model.visible);
  });

  assertFalse(model.visible);
  // Shoulld turn visible = true.
  model.setSelection({entries: ["NotEmpty"]});
  // Shoulld turn visible = false.
  model.setSelection({entries: []});
  // Shoulld turn visible = true.
  command.disabled = false;
  // Shoulld turn visible = false.
  command.disabled = true;
  // Shoulld turn visible = true.
  model.setVisibilityType(PreviewPanelModel.VisibilityType.ALWAYS_VISIBLE);
  // Shoulld turn visible = false.
  model.setVisibilityType(PreviewPanelModel.VisibilityType.ALWAYS_HIDDEN);
  command.disabled = true;
  model.setSelection({entries: ["NotEmpty"]});

  assertEquals(6, visibleList.length);
  assertTrue(visibleList[0]);
  assertFalse(visibleList[1]);
  assertTrue(visibleList[2]);
  assertFalse(visibleList[3]);
  assertTrue(visibleList[4]);
  assertFalse(visibleList[5]);
}
