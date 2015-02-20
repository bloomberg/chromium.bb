// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * File table list.
 * @constructor
 * @struct
 * @extends {cr.ui.table.TableList}
 * @suppress {checkStructDictInheritance}
 */
function FileTableList() {}

/**
 * Decorates TableList as FileTableList.
 * @param {!cr.ui.table.TableList} self A tabel list element.
 */
FileTableList.decorate = function(self) {
  self.__proto__ = FileTableList.prototype;
}

FileTableList.prototype.__proto__ = cr.ui.table.TableList.prototype;

/** @override */
FileTableList.prototype.mergeItems = function(beginIndex, endIndex) {
  cr.ui.table.TableList.prototype.mergeItems.call(this, beginIndex, endIndex);

  // Make sure that list item's selected attribute is updated just after the
  // mergeItems operation is done. This prevents checkmarks on selected items
  // from being animated unintentinally by redraw.
  for (var i = beginIndex; i < endIndex; i++) {
    var isSelected = this.selectionModel.getIndexSelected(i);
    if (this.cachedItems_[i].selected != isSelected)
      this.cachedItems_[i].selected = isSelected;
  }

  this.table.updateHighPriorityRange(beginIndex, endIndex);
}
