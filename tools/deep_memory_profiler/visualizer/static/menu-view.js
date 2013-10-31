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
 * @param {string|null} id Model id.
 * @param {Object} pos Clicked position. Not used
 * @private
 */
MenuView.prototype.selectNode_ = function(id) {
  var $tree = this.$tree_;

  if (id == null) {
    $tree.tree('selectNode', null);
    return;
  }

  var node = $tree.tree('getNodeById', id);
  $tree.tree('selectNode', node);
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

  function merge(origin, target) {
    if (!('children' in origin))
      return;
    if (!('children' in target)) {
      target.children = origin.children;
      return;
    }

    origin.children.forEach(function(child) {
      // Find child with the same label in target tree.
      var index = target.children.reduce(function(previous, current, index) {
        if (child.label === current.label)
        return index;
        return previous;
      }, -1);
      if (index === -1)
        target.children.push(child);
      else
        merge(child, target.children[index]);
    });
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
      merge(data, union);
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

    // Delegate events
    this.$tree_.bind('tree.click', function(event) {
      event.preventDefault();
      self.profiler_.setSelected(event.node.id);
    });
    this.$tree_.bind('tree.close', function(event) {
      event.preventDefault();
      self.profiler_.unsetSub(event.node.id);
      self.profiler_.setSelected(event.node.id);
    });
  } else {
    this.$tree_.tree('loadData', data);
  }
};
