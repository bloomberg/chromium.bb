// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides data access interface for dump file profiler.
 * @constructor
 */
var Profiler = function(jsonData) {
  this.jsonData_ = jsonData;
  // Initialize template with templates information.
  // TODO(junjianx): Make file path an argument.
  this.template_ = jsonData.templates['l2'];
  // Initialize selected category, and nothing selected at first.
  this.selected_ = null;

  // Trigger event.
  this.callbacks_ = {};
};

/**
 * Mimic Eventemitter in node. Add new listener for event.
 * @param {string} event
 * @param {Function} callback
 */
Profiler.prototype.addListener = function(event, callback) {
  if (!this.callbacks_[event])
    this.callbacks_[event] = $.Callbacks();
  this.callbacks_[event].add(callback);
};

/**
 * This function will emit the event.
 * @param {string} event
 */
Profiler.prototype.emit = function(event) {
  // Listeners should be able to receive arbitrary number of parameters.
  var eventArguments = Array.prototype.slice.call(arguments, 1);

  if (this.callbacks_[event])
    this.callbacks_[event].fire.apply(this, eventArguments);
};

/**
 * Remove listener from event.
 * @param {string} event
 * @param {Function} callback
 */
Profiler.prototype.removeListener = function(event, callback) {
  if (this.callbacks_[event])
    this.callbacks_[event].remove(callback);
};

/**
 * Calcualte initial tree according default template.
 */
Profiler.prototype.initialize = function() {
  this.tree_ = this.parseTemplate_();
  this.emit('changed', this.tree_);
};

Profiler.prototype.accumulate_ = function(
  template, snapshot, worldUnits, localUnits, nodePath, nodeName) {
  var self = this;
  var totalMemory = 0;
  var worldName = template[0];
  var breakdownName = template[1];
  var categories = snapshot.worlds[worldName].breakdown[breakdownName];
  // Make deep copy of localUnits.
  var remainderUnits = localUnits.slice(0);
  var node = {
    name: nodeName || worldName + '-' + breakdownName,
    nodePath: nodePath.slice(0),
    children: []
  };

  Object.keys(categories).forEach(function(categoryName) {
    var category = categories[categoryName];
    if (category['hidden'] === true)
      return;

    // Accumulate categories.
    var matchedUnits = intersection(category.units, localUnits);
    var memory = matchedUnits.reduce(function(previous, current) {
      return previous + worldUnits[worldName][current];
    }, 0);
    totalMemory += memory;
    remainderUnits = difference(remainderUnits, matchedUnits);

    // Handle subs options if exists.
    var child = null;
    if (!(categoryName in template[2])) {
      // Calculate child for current category.
      child = {
        name: categoryName,
        memory: memory
      };
      if ('subs' in category && category.subs.length)
        node.subs = category.subs;

      node.children.push(child);
    } else {
      // Calculate child recursively.
      var subTemplate = template[2][categoryName];
      var subWorldName = subTemplate[0];
      var subNodePath = nodePath.slice(0).concat([categoryName, 2]);
      var returnValue = null;

      if (subWorldName === worldName) {
        // If subs is in the same world, units should be filtered.
        returnValue = self.accumulate_(subTemplate, snapshot, worldUnits,
          matchedUnits, subNodePath, categoryName);
        node.children.push(returnValue.node);
        if (!returnValue.remainderUnits.length)
          return;

        var remainMemory =
          returnValue.remainderUnits.reduce(function(previous, current) {
            return previous + worldUnits[subWorldName][current];
          }, 0);

        node.children.push({
          name: categoryName + '-remaining',
          memory: remainMemory
        });
      } else {
        // If subs is in different world, use all units in that world.
        var subLocalUnits = Object.keys(worldUnits[subWorldName]);
        subLocalUnits = subLocalUnits.map(function(unitID) {
          return parseInt(unitID, 10);
        });

        returnValue = self.accumulate_(subTemplate, snapshot, worldUnits,
          subLocalUnits, subNodePath, categoryName);
        node.children.push(returnValue.node);

        if (memory > returnValue.totalMemory) {
          node.children.push({
            name: categoryName + '-remaining',
            memory: memory - returnValue.totalMemory
          });
        }
      }
    }
  });

  return {
    node: node,
    totalMemory: totalMemory,
    remainderUnits: remainderUnits
  };
};

Profiler.prototype.parseTemplate_ = function() {
  var self = this;

  return self.jsonData_.snapshots.map(function(snapshot) {
    var worldUnits = {};
    for (var worldName in snapshot.worlds) {
      worldUnits[worldName] = {};
      var units = snapshot.worlds[worldName].units;
      for (var unitID in units)
        worldUnits[worldName][unitID] = units[unitID][0];
    }
    var localUnits = Object.keys(worldUnits[self.template_[0]]);
    localUnits = localUnits.map(function(unitID) {
      return parseInt(unitID, 10);
    });

    var returnValue =
      self.accumulate_(self.template_, snapshot, worldUnits, localUnits, [2]);
    return returnValue.node;
  });
};