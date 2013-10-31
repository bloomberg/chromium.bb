// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a view class showing subs of selected item.
 * TODO(junjianx): use dropdown menu to show.
 * @param {Object} profiler Must have addListener method.
 * @construct
 */
var DropdownView = function(profiler) {
  this.profiler_ = profiler;
  this.placeholder_ = '#subs-dropdown';
  // Clear state when profiler model changed.
  profiler.addListener('changed', this.redraw_.bind(this));
  profiler.addListener('changed:selected', this.update_.bind(this));
};

/**
 * Render new dropdown at first time being called and recover otherwise.
 * @private
 */
DropdownView.prototype.redraw_ = function() {
  var self = this;

  var data = [{ label: 'subs' }];
  if (!this.$tree_) {
    this.$tree_ = $(this.placeholder_).tree({
      data: data,
      autoOpen: true
    });

    // Delegate click event to profiler.
    this.$tree_.bind('tree.click', function(event) {
      event.preventDefault();
      self.profiler_.setSub(event.node.id);
    });
  } else {
    this.$tree_.tree('loadData', data);
    $(this.placeholder_).css('display', 'none');
  }
};

/**
 * Update dropdown view when new model is selected in menu view.
 * @param {string} id Model id.
 * @param {Object} pos Clicked position.
 * @private
 */
DropdownView.prototype.update_ = function(id, pos) {
  if (id == null) {
    $(this.placeholder_).css('display', 'none');
    return;
  }

  var self = this;

  // Get all subs of selected model.
  var prof = this.profiler_;
  var models = prof.getModelsbyId(id);
  var children = models.reduce(function(previous, current) {
    if ('subs' in current) {
      current.subs.forEach(function(sub) {
        var id = sub.join(',');
        var label = sub.join(':');
        if (!previous.some(function(sub) {
          return sub.id === id;
        })) {
          var child = {
            id: id,
            label: label
          };
          previous.push(child);
        }
      });
    }
    return previous;
  }, []);

  // Update data of subs tree.
  var data = [{
    label: 'subs',
    children: children
  }];
  var $tree = this.$tree_;
  $tree.tree('loadData', data);

  // Select current sub if exists.
  var curSub = prof.getCurSubById(id);
  if (curSub) {
    var node = $tree.tree('getNodeById', curSub);
    $tree.tree('selectNode', node);
  }

  // If selected category has subs, display subs box.
  $(this.placeholder_).css('display', 'none');
  if (children.length > 0) {
    var view = $(this.placeholder_);
    view.css('display', 'block');
    if (pos != undefined) {
      view.css('position', 'fixed');
      view.css('top', pos.pageY);
      view.css('left', pos.pageX);
    } else {
      view.css('position', 'static');
    }
  }
};
