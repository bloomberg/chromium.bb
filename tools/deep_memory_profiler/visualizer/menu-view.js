// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a view class showing tree-menu.
 * @param {Object} profiler Must have addListener method.
 * @construct
 */
var MenuView = function(profiler) {
  this.profiler_ = profiler;
  this.placeholder_ = '#category-menu';

  // Update graph view and menu view when profiler model changed.
  profiler.addListener('changed', this.redraw_.bind(this));
  profiler.addListener('changed:selected', this.selectNode_.bind(this));
};

/**
 * Highlight the node being selected.
 * @param {string} id Model id.
 * @private
 */
MenuView.prototype.selectNode_ = function(id) {
  var $tree = this.$tree_;
  var node = $tree.tree('getNodeById', id);
  $tree.tree('selectNode', node);
  $tree.tree('scrollToNode', node);
};

/**
 * Update menu view when model updated.
 * @param {Array.<Object>} models
 * @private
 */
MenuView.prototype.redraw_ = function(models) {
  function convert(origin, target) {
    target.label = origin.name;
    target.id = origin.id;

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

  var self = this;

  // Merge trees in all snapshots.
  var union = null;
  models.forEach(function(model) {
    var data = {};
    convert(model, data);
    if (!union)
      union = data;
    else
      merge(union, data);
  });

  // Draw breakdown menu.
  var data = [union];
  if (!this.$tree_) {
    this.$tree_ = $(this.placeholder_).tree({
      data: data,
      autoOpen: true,
      onCreateLi: function(node, $li) {
        // TODO(junjianx): Add checkbox to decide the breakdown visibility.
      }
    });

    // Delegate click event to profiler.
    this.$tree_.bind('tree.click', function(event) {
      event.preventDefault();
      self.profiler_.setSelected(event.node.id);
    });
  } else {
    this.$tree_.tree('loadData', data);
  }
};
