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

    this.allLayerImplHistories_ = {};
  };

  LayerTreeHostImplHistory.prototype = {
    createNewLTHI: function() {
      var snapshotNumber = this.lthiSnapshots.length + 1;
      var lthi = new LayerTreeHostImpl(this, snapshotNumber);
      this.lthiSnapshots.push(lthi);
      this.allTilesBBox_ = undefined;
      this.allContentsScales_ = undefined;
      return lthi;
    },

    getOrCreateTileHistory: function(tileID) {
      if (!this.allTileHistories_[tileID])
        this.allTileHistories_[tileID] = new ccfv.model.TileHistory(tileID);
      return this.allTileHistories_[tileID];
    },

    getOrCreateLayerImplHistory: function(layerID) {
      if (!this.allLayerImplHistories_[layerID])
        this.allLayerImplHistories_[layerID] =
            new ccfv.model.LayerImplHistory(layerID);
      return this.allLayerImplHistories_[layerID];
    },

    get allContentsScales() {
      if (this.allContentsScales_ !== undefined)
        return this.allContentsScales_;

      var scales = {};
      for (var tileID in this.allTileHistories_) {
        var tileHistory = this.allTileHistories_[tileID];
        scales[tileHistory.contentsScale] = true;
      }
      this.allContentsScales_ = base.dictionaryKeys(scales);
      return this.allContentsScales_;
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

    // These fields are affected by the rebuildIfNeeded_() flow.
    this.pendingTree = new ccfv.model.LayerTreeImpl(
        this, constants.PENDING_TREE);
    this.activeTree = new ccfv.model.LayerTreeImpl(
        this, constants.ACTIVE_TREE);

    this.tilesForTree_ = undefined;
    this.inactiveTiles_ = [];
  }

  LayerTreeHostImpl.prototype = {
    get id() {
      return this.history.id;
    },

    getTree: function(whichTree) {
      if (whichTree === constants.ACTIVE_TREE)
        return this.activeTree;
      else if (whichTree === constants.PENDING_TREE)
        return this.pendingTree;
      else
        throw new Error('Active or pending only.');
    },

    getOrCreateTile: function(tileID) {
      var tileHistory = this.history.getOrCreateTileHistory(tileID);
      var tile = tileHistory.getOrCreateTileForLTHI(this);
      this.allTiles.push(tile);

      this.activeTree.setTilesDirty();
      this.pendingTree.setTilesDirty();
      this.tilesForTree_ = undefined;
      this.inactiveTiles_ = undefined;

      return tile;
    },

    get inactiveTiles() {
      if (this.inactiveTiles_ === undefined)
        this.rebuildTiles_();
      return this.inactiveTiles_;
    },

    getTilesForTree: function(which_tree) {
      if (this.tilesForTree_ === undefined)
        this.rebuildTiles_();
      return this.tilesForTree_[which_tree];
    },

    rebuildTiles_: function()  {
      this.tilesForTree_ = [[], []];
      this.inactiveTiles_ = [];
      this.allTiles.forEach(function(tile) {
        if (tile.priority[constants.ACTIVE_TREE].is_live)
          this.tilesForTree_[constants.ACTIVE_TREE].push(tile);
        if (tile.priority[constants.PENDING_TREE].is_live)
          this.tilesForTree_[constants.PENDING_TREE].push(tile);
        if (tile.priority[constants.PENDING_TREE].is_live == false &&
            tile.priority[constants.ACTIVE_TREE].is_live == false)
          this.inactiveTiles_.push(tile);
      }, this);
    },
  };

  return {
    LayerTreeHostImpl: LayerTreeHostImpl,
    LayerTreeHostImplHistory: LayerTreeHostImplHistory
  };
});
