// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Generate lines for flot plotting.
 * @param  {Array.<Object>} categories
 * @return {Array.<Object>}
 */
var generateLines = function(categories) {
  var lines = {};
  var snapshotNum = categories.length;

  // Initialize lines with all zero.
  categories.forEach(function(categories) {
    Object.keys(categories).forEach(function(breakdownName) {
      if (lines[breakdownName])
        return;
      lines[breakdownName] = [];
      for (var i = 0; i < snapshotNum; ++i)
        lines[breakdownName].push([i, 0]);
    });
  });

  // Assignment lines with values of categories.
  categories.forEach(function(categories, index) {
    Object.keys(categories).forEach(function(breakdownName) {
      lines[breakdownName][index] = [index, categories[breakdownName]];
    });
  });

  return Object.keys(lines).map(function(breakdownName) {
    return {
      label: breakdownName,
      data: lines[breakdownName]
    };
  });
};

$(function() {
  // Read original data and plot.
  $.getJSON('data/result.json', function(jsonData) {
    var profiler = new Profiler(jsonData);
    var categories = profiler.getCategories();
    var lines = generateLines(categories);
    var placeholder = '#plot';

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

    // Plot stack graph.
    $.plot(placeholder, lines, {
      series: {
        stack: true,
        lines: { show: true, fill: true }
      },
      grid: {
        hoverable: true,
        clickable: true
      }
    });
  });
});
