// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides data access interface for dump file profiler.
 * @constructor
 */
var Profiler = function(jsonData) {
  this.jsonData_ = jsonData;
  // Initialize template and calculate categories with roots information.
  // TODO(junjianx): Make file path an argument.
  this.template_ = jsonData.templates['l2'];

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
 * Calcualte initial categories according default template.
 */
Profiler.prototype.initializeCategories = function() {
  this.categories_ = this.calculateCategories_();
  this.emit('changed', this.categories_);
};

Profiler.prototype.accumulate_ = function(
  template, snapshot, worldUnits, localUnits, nodePath, nodeName) {
  var self = this;
  var totalMemory = 0;
  var worldName = template[0];
  var rootBreakdownName = template[1];
  var breakdowns = snapshot.worlds[worldName].breakdown[rootBreakdownName];
  // Make deep copy of localUnits.
  var remainderUnits = localUnits.slice(0);
  var categories = {
    name: nodeName || worldName + '-' + rootBreakdownName,
    nodePath: nodePath.slice(0),
    breakdowns: []
  };

  Object.keys(breakdowns).forEach(function(breakdownName) {
    var breakdown = breakdowns[breakdownName];
    if (breakdown['hidden'] === true)
      return;

    // Accumulate breakdowns.
    var matchedUnits = intersection(breakdown.units, localUnits);
    var memory = matchedUnits.reduce(function(previous, current) {
      return previous + worldUnits[worldName][current];
    }, 0);
    totalMemory += memory;
    remainderUnits = difference(remainderUnits, matchedUnits);

    // Handle subs options if exists.
    if (!(breakdownName in template[2])) {
      categories.breakdowns.push({
        name: breakdownName,
        memory: memory
      });

      if ('subs' in breakdown && breakdown.subs.length) {
        var length = categories.breakdowns.length;
        categories.breakdowns[length-1].subs = breakdown.subs;
      }
    } else {
      var subTemplate = template[2][breakdownName];
      var subWorldName = subTemplate[0];
      var subRootBreakdownName = subTemplate[1];
      var subNodePath = nodePath.slice(0).concat([breakdownName, 2]);
      var result = null;

      // If subs is in the same world, units should be filtered.
      if (subWorldName === worldName) {
        result = self.accumulate_(subTemplate, snapshot, worldUnits,
          matchedUnits, subNodePath, breakdownName);
        categories.breakdowns.push(result.categories);
        if (!result.remainderUnits.length)
          return;

        var remainMemory =
          result.remainderUnits.reduce(function(previous, current) {
            return previous + worldUnits[subWorldName][current];
          }, 0);

        categories.breakdowns.push({
          name: breakdownName + '-remaining',
          memory: remainMemory
        });
      } else {
        var subLocalUnits = Object.keys(worldUnits[subWorldName]);
        subLocalUnits = subLocalUnits.map(function(unitName) {
          return parseInt(unitName, 10);
        });

        result = self.accumulate_(subTemplate, snapshot, worldUnits,
          subLocalUnits, subNodePath, breakdownName);
        categories.breakdowns.push(result.categories);

        if (memory > result.totalMemory) {
          categories.breakdowns.push({
            name: breakdownName + '-remaining',
            memory: memory - result.totalMemory
          });
        }
      }
    }
  });

  return {
    categories: categories,
    totalMemory: totalMemory,
    remainderUnits: remainderUnits
  };
};

Profiler.prototype.calculateCategories_ = function() {
  var self = this;

  return self.jsonData_.snapshots.map(function(snapshot) {
    var worldUnits = {};
    for (var worldName in snapshot.worlds) {
      worldUnits[worldName] = {};
      var units = snapshot.worlds[worldName].units;
      for (var unitName in units)
        worldUnits[worldName][unitName] = units[unitName][0];
    }
    var localUnits = Object.keys(worldUnits[self.template_[0]]);
    localUnits = localUnits.map(function(unitName) {
      return parseInt(unitName, 10);
    });

    var result =
      self.accumulate_(self.template_, snapshot, worldUnits, localUnits, [2]);
    return result.categories;
  });
};