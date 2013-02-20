/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('base.bbox2');
base.require('model.constants');
base.require('model.layer_impl');

base.exportTo('ccfv.model', function() {

  /**
   * Represents a cc::LayerTreeImpl
   *
   * @constructor
   */
  function LayerTreeImpl(lthi, which_tree) {
    this.lthi = lthi;
    this.which_tree = which_tree;
    this.tiles_ = [];
    this.allLayers = [];
    this.tileBBox_ = undefined;
  };

  LayerTreeImpl.prototype = {
    setTilesDirty: function() {
      this.tiles_ = undefined;
      this.tileBBox_ = undefined;
    },

    get tiles() {
      if (!this.tiles_)
        this.tiles_ = this.lthi.getTilesForTree(this.which_tree);
      return this.tiles_;
    },

    getOrCreateLayerImpl: function(layerID) {
      var layerHistory = this.lthi.history.getOrCreateLayerImplHistory(layerID);
      var layer = layerHistory.getOrCreateLayerImplForLTHI(this);
      this.allLayers.push(layer);
      return layer;
    },

    get tileBBox() {
      if (!this.tileBBox_) {
        var bbox = new base.BBox2();
        for (var i = 0; i < this.tiles.length; i++) {
          var tile = this.tiles[i];
          var priority = tile.priority[this.which_tree];
          if (!priority.current_screen_quad)
            continue;
          bbox.addQuad(priority.current_screen_quad);
        }
        this.tileBBox_ = bbox;
      }
      return this.tileBBox_;
    },

  };

  return {
    LayerTreeImpl: LayerTreeImpl
  }
});

