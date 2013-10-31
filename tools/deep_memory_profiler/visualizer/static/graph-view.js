// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a view class showing flot graph.
 * @param {Object} profiler Must have addListener method.
 * @construct
 */
var GraphView = function(profiler) {
  this.profiler_ = profiler;
  this.placeholder_ = '#graph-div';
  // Update graph view and menu view when profiler model changed.
  profiler.addListener('changed', this.redraw_.bind(this));
};

/**
 * Generate lines for flot plotting.
 * @param {Array.<Object>} models
 * @return {Array.<Object>}
 * @private
 */
GraphView.prototype.generateLines_ = function(models) {
  function mergeCategoryTree(snapNode, treeNode) {
    if ('children' in snapNode) {
      // If |snapNode| is not a leaf node, we should go deeper.
      if (!('children' in treeNode))
        treeNode.children = {};
      snapNode.children.forEach(function(child) {
        if (!(child.id in treeNode.children))
          treeNode.children[child.id] = {};
        mergeCategoryTree(child, treeNode.children[child.id]);
      });
    } else {
      treeNode.name = snapNode.name;
    }
  }

  function getCategoriesMap(node, id, categories) {
    if ('children' in node) {
      Object.keys(node.children).forEach(function(id) {
        getCategoriesMap(node.children[id], id, categories);
      });
    } else {
      if (!(id in categories)) {
        categories[id] = {
          name: node.name,
          data: []
        };
        for (var i = 0; i < models.length; ++i)
          categories[id].data.push([models[i].time - models[0].time, 0]);
      }
    }
  }

  function getLineValues(snapNode, index, categories) {
    if ('children' in snapNode) {
      snapNode.children.forEach(function(child) {
        getLineValues(child, index, categories);
      });
    } else {
      categories[snapNode.id].data[index][1] = snapNode.size;
    }
  }

  // TODO(dmikurube): Remove this function after adding "color" attribute
  //     in each category.
  function getHashColorCode(id) {
    var color = 0;
    for (var i = 0; i < id.length; ++i)
      color = (color * 0x57 + id.charCodeAt(i)) & 0xffffff;
    color = color.toString(16);
    while (color.length < 6)
      color = '0' + color;
    return '#' + color;
  }

  var categoryTree = {};
  models.forEach(function(model) {
    mergeCategoryTree(model, categoryTree);
  });
  // Convert layout of categories from tree style to hash map style.
  var categoryMap = {};
  getCategoriesMap(categoryTree, '', categoryMap);
  // Get size of each category.
  models.forEach(function(model, index) {
    getLineValues(model, index, categoryMap);
  });

  return Object.keys(categoryMap).map(function(id) {
    return {
      color: getHashColorCode(id),
      data: categoryMap[id].data,
      id: id,
      label: categoryMap[id].name
    };
  });
};

/**
 * Update graph view when model updated.
 * TODO(junjianx): use redraw function to improve perfomance.
 * @param {Array.<Object>} models
 * @private
 */
GraphView.prototype.redraw_ = function(models) {
  var self = this;
  var data = this.generateLines_(models);
  if (!this.graph_) {
    var $graph = $(this.placeholder_);
    this.graph_ = $.plot($graph, data, {
      series: {
        stack: true,
        lines: { show: true, fill: true }
      },
      grid: {
        hoverable: true,
        clickable: true
      }
    });

    // Bind click event so that user can select category by clicking stack
    // area. It firstly checks x range which clicked point is in, and all lines
    // share same x values, so it is checked only once at first. Secondly, it
    // checked y range by accumulated y values because this is a stack graph.
    $graph.bind('plotclick', function(event, pos, item) {
      // Get newest lines data from graph.
      var lines = self.graph_.getData();
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
      if (i === lines.length) {
        self.profiler_.setSelected(null);
        return;
      }

      self.profiler_.setSelected(lines[i].id, pos);
    });
  } else {
    this.graph_.setData(data);
    this.graph_.setupGrid();
    this.graph_.draw();
  }
};
