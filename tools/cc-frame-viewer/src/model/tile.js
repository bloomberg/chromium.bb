/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('model.constants');

base.exportTo('ccfv.model', function() {

  /**
   * Represents a cc::Tile over the course of its lifetime.
   *
   * @constructor
   */
  function TileHistory(id) {
    this.id = id;
    this.tilesBySnapshotNumber = {};
    this.layerID = undefined;
    this.picturePile = undefined;
    this.contentsScale = undefined;

    this.selected = false;
  }

  TileHistory.prototype = {
    get title() {
      return 'TileHistory ' + this.id;
    },

    getOrCreateTileForLTHI: function(lthi) {
      if (!this.tilesBySnapshotNumber[lthi.snapshotNumber])
        this.tilesBySnapshotNumber[lthi.snapshotNumber] = new Tile(this);
      return this.tilesBySnapshotNumber[lthi.snapshotNumber];
    },

    dumpToSimpleObject: function(obj) {
      obj.id = this.id;
      obj.layerID = this.layerID;
      obj.picturePile = this.picturePile;
      obj.contentsScale = this.contentsScale;
    }
  };

  /**
   * Represents a cc::Tile at an instant in time.
   *
   * @constructor
   */
  function Tile(history) {
    this.history = history;
    this.managedState = undefined;
    this.priority = [undefined, undefined];
  }

  Tile.prototype = {
    get id() {
      return this.history.id;
    },

    get contentsScale() {
      return this.history.contents_scale;
    },

    get picturePile() {
      return this.history.picture_pile;
    },

    get title() {
      return 'Tile ' + this.id;
    },

    get selected() {
      return this.history.selected;
    },

    set selected(s) {
      this.history.selected = s;
    },

    dumpToSimpleObject: function(obj) {
      obj.history = {};
      this.history.dumpToSimpleObject(obj.history);
      obj.managedState = this.managedState;
      obj.priority = [{}, {}];
      dumpAllButQuadToSimpleObject.call(this.priority[0], obj.priority[0]);
      dumpAllButQuadToSimpleObject.call(this.priority[1], obj.priority[1]);
    },
  };

  // Called with priority object as context.
  function dumpAllButQuadToSimpleObject(obj) {
    for (var k in this) {
      if (k == 'current_screen_quad')
        continue;
      obj[k] = this[k];
    }
  }

  return {
    Tile: Tile,
    TileHistory: TileHistory
  }
});

