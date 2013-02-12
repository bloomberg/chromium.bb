/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('base.bbox2');
base.require('model.constants');

base.exportTo('ccfv.model', function() {

  /**
   * Represents a cc::LayerTreeImpl
   *
   * @constructor
   */
  function LayerTreeImpl(which_tree) {
    this.which_tree = which_tree;
    this.tiles = [];
    this.tileBBox_ = undefined;
  };

  LayerTreeImpl.prototype = {
    resetTiles: function() {
      this.tilse = [];
      this.tileBBox_ = undefined;
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

