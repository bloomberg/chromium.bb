// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a view class showing tree-menu.
 * @param {Object} model Must have addListener method.
 * @construct
 */
var MenuView = function(model) {
  this.model_ = model;
  // Update graph view and menu view when model changed.
  model.addListener('changed', this.redraw.bind(this));
};

/**
 * Update menu view when model updated.
 * @param {Array.<Object>} modelTrees
 */
MenuView.prototype.redraw = function(modelTrees) {
  function convert(origin, target) {
    target.label = origin.name;

    if ('children' in origin) {
      target.children = [];
      origin.children.forEach(function(originChild) {
        var targetChild = {};
        target.children.push(targetChild);
        convert(originChild, targetChild);
      });
    }
  }

  function merge(left, right) {
    if (!('children' in right) && 'children' in left)
      return;
    if ('children' in right && !('children' in left))
      left.children = right.children;
    if ('children' in right && 'children' in left) {
      right.children.forEach(function(child) {
        // Find child with the same label in right tree.
        var index = left.children.reduce(function(previous, current, index) {
          if (child.label === current.label)
            return index;
          return previous;
        }, -1);
        if (index === -1)
          left.children.push(child);
        else
          merge(child, left.children[index]);
      });
    }
 }

  // Merge trees in all snapshots.
  var union = null;
  modelTrees.forEach(function(modelTree) {
    var viewTree = {};
    convert(modelTree, viewTree);
    if (!union)
      union = viewTree;
    else
      merge(union, viewTree);
  });

  var placeholder = '#category-menu';
  // Draw breakdown menu.
  $(placeholder).tree({
    data: [union],
    autoOpen: true,
    onCreateLi: function(node, $li) {
      // TODO(junjianx): Add checkbox to decide the breakdown visibility.
    }
  });
};
