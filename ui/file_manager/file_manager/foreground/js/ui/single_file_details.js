// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * SingleFileDetailsPanel constructor.
 *
 * Represents grid for the details panel for a single file in Files app.
 * @constructor
 * @extends {HTMLDivElement}
 */
function SingleFileDetailsPanel() {
  throw new Error('Use SingleFileDetailsList.decorate');
}

/**
 * Inherits from cr.ui.List.
 */
SingleFileDetailsPanel.prototype = {
  __proto__: HTMLDivElement.prototype,
  onFileSelectionChanged: function(entry) {
    this.filename_.textContent = entry.name;
  }
};

/**
 * Decorates an HTML element to be a SingleFileDetailsList.
 * @param {!HTMLDivElement} self The grid to decorate.
 * @param {!MetadataModel} metadataModel File system metadata.
 */
SingleFileDetailsPanel.decorate = function(self, metadataModel) {
  self.__proto__ = SingleFileDetailsPanel.prototype;
  self.metadataModel = metadataModel;
  /**
   * Data model of detail infos.
   * @private {!SingleFileDetailsDataModel}
   * @const
   */
  self.model_ = new SingleFileDetailsDataModel();
  self.filename_ = assertInstanceof(queryRequiredElement('.filename', self),
      HTMLDivElement);
  var list = queryRequiredElement('.details-list', self);
  SingleFileDetailsList.decorate(list, metadataModel);
  self.list_ = assertInstanceof(list, SingleFileDetailsList);
  self.list_.dataModel = self.model_;
  self.list_.autoExpands = true;
};

/**
 * SingleFileDetailsList constructor.
 *
 * Represents grid for the details list for a single file in Files app.
 * @constructor
 * @extends {cr.ui.List}
 */
function SingleFileDetailsList() {
  throw new Error('Use SingleFileDetailsList.decorate');
}

/**
 * Inherits from cr.ui.List.
 */
SingleFileDetailsList.prototype = {
  __proto__: cr.ui.List.prototype,
  onFileSelectionChanged: function(entry) {
    console.log(entry);
  }
};

/**
 * Decorates an HTML element to be a SingleFileDetailsList.
 * @param {!Element} self The grid to decorate.
 * @param {!MetadataModel} metadataModel File system metadata.
 */
SingleFileDetailsList.decorate = function(self, metadataModel) {
  cr.ui.Grid.decorate(self);
  self.__proto__ = SingleFileDetailsList.prototype;
  self.metadataModel_ = metadataModel;

  self.scrollBar_ = new ScrollBar();
  self.scrollBar_.initialize(self.parentElement, self);

  self.itemConstructor = function(entry) {
    var item = self.ownerDocument.createElement('li');
    SingleFileDetailsList.Item.decorate(
        item,
        entry,
        /** @type {FileGrid} */ (self));
    return item;
  };
};

/**
 * Item for the Grid View.
 * @constructor
 * @extends {cr.ui.ListItem}
 */
SingleFileDetailsList.Item = function() {
  throw new Error();
};

/**
 * Inherits from cr.ui.DetailsItem.
 */
SingleFileDetailsList.Item.prototype.__proto__ = cr.ui.ListItem.prototype;

/**
 * @param {Element} li List item element.
 * @param {!Entry} entry File entry.
 * @param {FileGrid} grid Owner.
 */
SingleFileDetailsList.Item.decorate = function(li, entry, grid) {
  li.__proto__ = SingleFileDetailsList.Item.prototype;
};

/**
 * Data model for details panel.
 *
 * @constructor
 * @extends {cr.ui.ArrayDataModel}
 */
function SingleFileDetailsDataModel() {
  cr.ui.ArrayDataModel.call(this, []);
}

SingleFileDetailsDataModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype,

};
