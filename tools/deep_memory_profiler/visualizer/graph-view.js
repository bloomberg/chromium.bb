// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a view class showing flot graph.
 * @param {Object} model Must have addListener method.
 * @construct
 */
var GraphView = function(model) {
  this.model_ = model;
  // Update graph view and menu view when model changed.
  model.addListener('changed', this.redraw.bind(this));
};

/**
 * Generate lines for flot plotting.
 * @param {Array.<Object>} categories
 * @return {Array.<Object>}
 */
GraphView.prototype.generateLines_ = function(categories) {
  function getLeaves(node, breakdowns) {
    if ('breakdowns' in node) {
      node.breakdowns.forEach(function(breakdown) {
        getLeaves(breakdown, breakdowns);
      });
    } else {
      breakdowns.push(node);
    }
  }

  var lines = {};
  var snapshotNum = categories.length;
  // Initialize lines with all zero.
  categories.forEach(function(category) {
    var breakdowns = [];
    getLeaves(category, breakdowns);
    breakdowns.forEach(function(breakdown) {
      var name = breakdown.name;
      if (lines[name])
        return;
      lines[name] = [];
      for (var i = 0; i < snapshotNum; ++i)
        lines[name].push([i, 0]);
    });
  });

  // Assignment lines with values of categories.
  categories.forEach(function(category, index) {
    var breakdowns = [];
    getLeaves(category, breakdowns);
    breakdowns.forEach(function(breakdown) {
      var name = breakdown.name;
      var memory = breakdown.memory;
      lines[name][index] = [index, memory];
    });
  });

  return Object.keys(lines).map(function(name) {
    return {
      label: name,
      data: lines[name]
    };
  });
};

/**
 * Update garph view when model updated.
 * TODO(junjianx): use redraw function to improve perfomance.
 * @param {Array.<Object>} categories
 */
GraphView.prototype.redraw = function(categories) {
  var placeholder = '#graph-div';
  var lines = this.generateLines_(categories);
  var graph = $.plot(placeholder, lines, {
    series: {
      stack: true,
      lines: { show: true, fill: true }
    },
    grid: {
      hoverable: true,
      clickable: true
    }
  });

  // Bind click event so that user can select breakdown by clicking stack
  // area. It firstly checks x range which clicked point is in, and all lines
  // share same x values, so it is checked only once at first. Secondly, it
  // checked y range by accumulated y values because this is a stack graph.
  $(placeholder).bind('plotclick', function(event, pos, item) {
    // If only <=1 line exists or axis area clicked, return.
    var right = binarySearch.call(lines[0].data.map(function(point) {
      return point[0];
    }), pos.x);
    if (lines.length <= 1 || right === lines.length || right === 0)
      return;

    // Calculate interpolate y value of every line.
    for (var i = 0; i < lines.length; ++i) {
      var line = lines[i].data;
      // [left, right] is the range including clicked point.
      var left = right - 1;
      var leftPoint = {
        x: line[left][0],
        y: (leftPoint ? leftPoint.y : 0) + line[left][1]
      };
      var rightPoint = {
        x: line[right][0],
        y: (rightPoint ? rightPoint.y : 0) + line[right][1]
      };

      // Calculate slope of the linear equation.
      var slope = (rightPoint.y - leftPoint.y) / (rightPoint.x - leftPoint.x);
      var interpolateY = slope * (pos.x - rightPoint.x) + rightPoint.y;
      if (interpolateY >= pos.y)
        break;
    }

    // If pos.y is higher than all lines, return.
    if (i === lines.length)
      return;

    // TODO(junjianx): temporary log for checking selected object.
    console.log('line ' + i + ' is selected.');
  });
};
