/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('base.bbox2');
base.require('model.constants');
base.require('model.tile');
base.require('model.layer_tree_impl');

base.exportTo('ccfv.model', function() {

  var constants = ccfv.model.constants;

  /**
   * Represents the history of a specific cc::LayerTreeHostImpl over time.
   *
   * @constructor
   */
  function LayerTreeHostImplHistory(id) {
    this.id = id;
    this.lthiSnapshots = [];

    this.allTileHistories_ = {};
    this.allTilesBBox_ = undefined;
  };

  LayerTreeHostImplHistory.prototype = {
    createNewLTHI: function() {
      var snapshotNumber = this.lthiSnapshots.length + 1;
      var lthi = new LayerTreeHostImpl(this, snapshotNumber);
      this.lthiSnapshots.push(lthi);
      this.allTilesBBox_ = undefined;
      return lthi;
    },

    getOrCreateTileHistory: function(tileID) {
      if (!this.allTileHistories_[tileID])
        this.allTileHistories_[tileID] = new ccfv.model.TileHistory(tileID);
      return this.allTileHistories_[tileID];
    },

    get allTilesBBox() {
      if (!this.allTilesBBox_) {
        var bbox = new base.BBox2();
        for (var tileID in this.allTileHistories_) {
          var tileHistory = this.allTileHistories_[tileID];
          for (var snapshotNumber in tileHistory.tilesBySnapshotNumber) {
            var tile = tileHistory.tilesBySnapshotNumber[snapshotNumber];
            if (tile.priority[0].current_screen_quad)
              bbox.addQuad(tile.priority[0].current_screen_quad);
            if (tile.priority[1].current_screen_quad)
              bbox.addQuad(tile.priority[1].current_screen_quad);
          }
        }
        this.allTilesBBox_ = bbox;
      }
      return this.allTilesBBox_;
    },
  };

  /**
   * Represents a snapshot of the cc::LayerTreeHostImpl at an instant in time.
   *
   * @constructor
   */
  function LayerTreeHostImpl(history, snapshotNumber) {
    this.history = history;
    this.snapshotNumber = snapshotNumber;
    this.deviceViewportSize = {}

    this.allTiles = [];

    this.rebuildNeeded_ = false;

    // These fields are affected by the rebuildIfNeeded_() flow.
    this.pendingTree_ = new ccfv.model.LayerTreeImpl(constants.PENDING_TREE);
    this.activeTree_ = new ccfv.model.LayerTreeImpl(constants.ACTIVE_TREE);
    this.inactiveTiles_ = [];
  }

  LayerTreeHostImpl.prototype = {
    get id() {
      return this.history.id;
    },

    getOrCreateTile: function(tile_id) {
      var tileHistory = this.history.getOrCreateTileHistory(tile_id);
      var tile = tileHistory.getOrCreateTileForLTHI(this);
      this.allTiles.push(tile);
      this.rebuildNeeded_ = true;
      return tile;
    },

    get activeTree() {
      this.rebuildIfNeeded_();
      return this.activeTree_;
    },

    get pendingTree() {
      this.rebuildIfNeeded_();
      return this.pendingTree_;
    },

    get inactiveTiles() {
      this.rebuildIfNeeded_();
      return this.inactiveTiles_;
    },

    rebuildIfNeeded_: function()  {
      if (!this.rebuildNeeded_)
        return;
      this.activeTree_.resetTiles();
      this.pendingTree_.resetTiles();
      this.allTiles.forEach(function(tile) {
        if (tile.priority[constants.ACTIVE_TREE].is_live)
          this.activeTree_.tiles.push(tile);
        if (tile.priority[constants.PENDING_TREE].is_live)
          this.pendingTree_.tiles.push(tile);
        if (tile.priority[constants.PENDING_TREE].is_live == false &&
            tile.priority[constants.ACTIVE_TREE].is_live == false)
          this.inactiveTiles_.push(tile);
      }, this);
      this.rebuildNeeded_ = false;
    },
  };

  return {
    LayerTreeHostImpl: LayerTreeHostImpl,
    LayerTreeHostImplHistory: LayerTreeHostImplHistory
  };
});
