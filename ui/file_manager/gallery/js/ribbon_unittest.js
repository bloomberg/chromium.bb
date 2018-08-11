// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testEmptySpliceEvent() {
  var dataModel = new cr.ui.ArrayDataModel([]);
  var selectionModel = new cr.ui.ListSelectionModel();
  var thumbnailModel = /** @type{!ThumbnailModel} */ ({});
  var ribbon =
      new Ribbon(document, window, dataModel, selectionModel, thumbnailModel);
  ribbon.enable();
  var event = new Event('splice');
  event.added = [];
  event.removed = [];
  dataModel.dispatchEvent(event);
}
