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
 * Calcualte initial models according default template.
 */
Profiler.prototype.reparse = function() {
  this.models_ = this.parseTemplate_();
  this.emit('changed', this.models_);
};

/**
 * To be called by view when new model being selected.
 * And then triggers all relative views to update.
 */
Profiler.prototype.setSelected = function(id) {
  this.selected_ = id;
  this.emit('changed:selected', id);
};

/**
 * Get all models throughout the whole timeline of given id.
 * @param {string} id Model id.
 * @return {Array.<Object>} model array of given id.
 */
Profiler.prototype.getModelsbyId = function(id) {
  function find(model) {
    if (model.id === id)
      return model;
    if ('children' in model)
      return model.children.reduce(function(previous, current) {
        var matched = find(current);
        if (matched)
          previous = matched;
        return previous;
      }, null);
  }

  return this.models_.reduce(function(previous, current) {
    var matched = find(current);
    if (matched)
      previous.push(matched);
    return previous;
  }, []);
};

/**
 * Get current sub of given model, return undefined if sub dont exist.
 * @param {string} id Model id.
 * @return {undefined|string} world-breakdown like 'vm-map'.
 */
Profiler.prototype.getCurSubById = function(id) {
  // Root won't has breakdown.
  var path = id.split(',').splice(1);
  if (!path.length) return null;

  var tmpl = this.template_;
  var curSub = path.reduce(function(previous, current, index) {
    return previous[2][current];
  }, tmpl);

  // return
  return curSub && curSub[0] + ',' + curSub[1];
};

/**
 * Generate and then reparse new template when new sub was selected.
 * @param {string|null} sub World-breakdown like 'vm-map'.
 */
Profiler.prototype.setSub = function(sub) {
  var selected = this.selected_;
  var path = selected.split(',');
  var key = path[path.length-1];

  // Add sub breakdown to template.
  var models = this.getModelsbyId(selected);
  var subTmpl = sub.split(',');
  subTmpl.push({});
  models[0].template[2][key] = subTmpl;

  // Recalculate new template.
  this.reparse();
};

Profiler.prototype.accumulate_ = function(
  template, snapshot, worldUnits, localUnits, name) {
  var self = this;
  var totalSize = 0;
  var worldName = template[0];
  var breakdownName = template[1];
  var categories = snapshot.worlds[worldName].breakdown[breakdownName];
  // Make deep copy of localUnits.
  var remainderUnits = localUnits.slice(0);
  var model = {
    name: name || worldName + '-' + breakdownName,
    children: []
  };

  Object.keys(categories).forEach(function(categoryName) {
    var category = categories[categoryName];
    if (category['hidden'] === true)
      return;

    // Filter units.
    var matchedUnits = intersection(category.units, localUnits);
    remainderUnits = difference(remainderUnits, matchedUnits);

    // Accumulate categories.
    var size = matchedUnits.reduce(function(previous, current) {
      return previous + worldUnits[worldName][current];
    }, 0);
    totalSize += size;

    // Handle subs options if exists.
    var child = null;
    if (!(categoryName in template[2])) {
      // Calculate child for current category.
      child = {
        name: categoryName,
        size: size
      };
      if ('subs' in category && category.subs.length) {
        child.subs = category.subs;
        child.template = template;
      }

      model.children.push(child);
    } else {
      // Calculate child recursively.
      var subTemplate = template[2][categoryName];
      var subWorldName = subTemplate[0];
      var retVal = null;

      if (subWorldName === worldName) {
        // If subs is in the same world, units should be filtered.
        retVal = self.accumulate_(subTemplate, snapshot, worldUnits,
          matchedUnits, categoryName);
        if ('subs' in category && category.subs.length) {
          retVal.model.subs = category.subs;
          retVal.model.template = template;
        }
        model.children.push(retVal.model);
        // Don't output remaining item without any unit.
        if (!retVal.remainderUnits.length)
          return;

        // Sum up remaining units size.
        var remainSize =
          retVal.remainderUnits.reduce(function(previous, current) {
            return previous + worldUnits[subWorldName][current];
          }, 0);

        model.children.push({
          name: categoryName + '-remaining',
          size: remainSize
        });
      } else {
        // If subs is in different world, use all units in that world.
        var subLocalUnits = Object.keys(worldUnits[subWorldName]);
        subLocalUnits = subLocalUnits.map(function(unitID) {
          return parseInt(unitID, 10);
        });

        retVal = self.accumulate_(subTemplate, snapshot, worldUnits,
          subLocalUnits, categoryName);
        if ('subs' in category && category.subs.length) {
          retVal.model.subs = category.subs;
          retVal.model.template = template;
        }
        model.children.push(retVal.model);

        if (size > retVal.totalSize) {
          model.children.push({
            name: categoryName + '-remaining',
            size: size - retVal.totalSize
          });
        } else {
          // Output WARNING when sub-breakdown size is larger.
          console.log('WARNING: size of sub-breakdown is larger');
        }
      }
    }
  });

  return {
    model: model,
    totalSize: totalSize,
    remainderUnits: remainderUnits
  };
};

Profiler.prototype.parseTemplate_ = function() {
  function calModelId(model, localPath) {
    // Create unique id for every model.
    model.id = localPath.length ?
      localPath.join() + ',' + model.name : model.name;

    if ('children' in model) {
      model.children.forEach(function(child, index) {
        var childPath = localPath.slice(0);
        childPath.push(model.name);
        calModelId(child, childPath);
      });
    }
  }

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

    var retVal =
      self.accumulate_(self.template_, snapshot, worldUnits, localUnits);
    calModelId(retVal.model, []);
    return retVal.model;
  });
};