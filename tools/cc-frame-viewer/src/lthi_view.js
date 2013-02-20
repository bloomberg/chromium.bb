/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.require('ui.list_view');
base.require('quad_view_viewport');
base.require('tree_quad_view');
base.require('layer_tree_impl_view');
base.requireStylesheet('lthi_view');

base.exportTo('ccfv', function() {

  var TreeQuadView = ccfv.TreeQuadView;

  var LTHIView = ui.define('x-lthi-view');

  LTHIView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.lthi_ = undefined;
      this.quadViews_ = document.createElement('x-quad-views');

      this.viewport_ = undefined;

      var optionsEl = document.createElement('x-lthi-view-options');

      this.colorMapSelector_ = document.createElement('select');
      this.colorMapSelector_.addEventListener(
        'change', this.onColorMapChanged_.bind(this));
      ccfv.ALL_TILE_COLOR_MAPS.forEach(function(colorMap) {
        var mapOption = document.createElement('option');
        mapOption.textContent = colorMap.title;
        mapOption.colorMap = colorMap;
        this.colorMapSelector_.appendChild(mapOption);
      }.bind(this));
      optionsEl.appendChild(ui.createSpan('Color map:'));
      optionsEl.appendChild(this.colorMapSelector_);

      this.scaleSelector_ = document.createElement('select');
      this.scaleSelector_.addEventListener(
        'change', this.onScaleChanged_.bind(this));
      [1/32., 1/16., 1/8., 1/4, 1/2, 1].forEach(function(scale) {
        var mapOption = document.createElement('option');
        mapOption.textContent = Math.floor(scale * 100) + '%';
        mapOption.scale = scale;
        this.scaleSelector_.appendChild(mapOption);
      }.bind(this));
      this.scaleSelector_.selectedIndex = 3;
      optionsEl.appendChild(ui.createSpan('Scale:'));
      optionsEl.appendChild(this.scaleSelector_);

      this.treePerContentScale_ = document.createElement('input');
      this.treePerContentScale_.type = 'checkbox';
      this.treePerContentScale_.textContent = 'bar';
      this.treePerContentScale_.addEventListener(
          'change', this.onTreePerContentScaleChanged_.bind(this));
      optionsEl.appendChild(
          ui.createLabel('Split apart scale:', this.treePerContentScale_));

      this.headerTextEl_ = document.createElement('span');
      optionsEl.appendChild(ui.createSpan('Details:'));
      optionsEl.appendChild(this.headerTextEl_);

      this.inactiveTileView_ = new InactiveTileView();

      this.onSelectionChanged_ = this.onSelectionChanged_.bind(this);

      this.inactiveTileView_.addEventListener('selection-changed',
                                              this.onSelectionChanged_);

      this.activeLayerTreeImplView_ = new ccfv.LayerTreeImplView();
      this.activeLayerTreeImplView_.header = 'Active layers';

      this.appendChild(optionsEl);
      this.appendChild(this.quadViews_);
      this.appendChild(this.inactiveTileView_);
      this.appendChild(this.activeLayerTreeImplView_);
    },

    get lthi() {
      return this.lthi_;
    },

    set lthi(lthi) {
      this.lthi_ = lthi;
      this.updateChildren_();
    },

    onColorMapChanged_: function() {
      for (var i = 0; i < this.quadViews_.children.length; i++) {
        this.quadViews_.children[i].colorMap =
          this.colorMapSelector_.selectedOptions[0].colorMap;
      }
    },

    onScaleChanged_: function() {
      if (this.viewport_.scale)
        this.viewport_.scale = this.scaleSelector_.selectedOptions[0].scale;
    },

    onTreePerContentScaleChanged_: function() {
      this.updateChildren_();
    },

    updateChildren_: function() {
      var lthi = this.lthi_;
      if (!lthi) {
        for (var i = 0; i < this.quadViews_.children.length; i++) {
          this.quadViews_.children[i].tree = undefined;
          this.quadViews_.children[i].removeEventListener('selection-changed',
                                                      this.onSelectionChanged_);
        }
        this.quadViews_.textContent = '';
        this.viewport_ = undefined;
        this.activeLayerTreeImplView_.layerTreeImpl = undefined;
        return;
      }

      this.quadViews_.textContent = '';

      var bbox = lthi.history.allTilesBBox;
      if (bbox.isEmpty)
        return;

      this.viewport_ = new ccfv.QuadViewViewport(
        bbox);
      this.viewport_.scale = this.scaleSelector_.selectedOptions[0].scale;


      // Make the trees.
      var quadViews;
      function makeForTree(tree, treePrefix) {
        var tilesByContentsScale = {};
        tree.tiles.forEach(function(tile) {
          var cs = tile.history.contentsScale;
          if (tilesByContentsScale[cs] === undefined)
            tilesByContentsScale[cs] = [];
          tilesByContentsScale[cs].push(tile);
        }, this);
        base.dictionaryKeys(tilesByContentsScale).forEach(function(cs) {
          var qv = new TreeQuadView();
          var csRounded = Math.floor(cs * 10) / 10;
          qv.headerPrefix = treePrefix + ' @ cs=' + csRounded;
          qv.which_tree = tree.which_tree;
          qv.tiles = tilesByContentsScale[cs];
          quadViews.push(qv);
        }, this);
      }

      if (this.treePerContentScale_.checked) {

        quadViews = [];
        makeForTree(lthi.activeTree, 'Active Tree Tiles');
        makeForTree(lthi.pendingTree, 'Pending Tree Tiles');
      } else {
        var aQuadView = new TreeQuadView();
        aQuadView.headerPrefix = 'Active Tree Tiles';
        aQuadView.which_tree = lthi.activeTree.which_tree;
        aQuadView.tiles = lthi.activeTree.tiles;

        var pQuadView = new TreeQuadView();
        pQuadView.headerPrefix = 'Pending Tree Tiles';
        pQuadView.which_tree = lthi.pendingTree.which_tree;
        pQuadView.tiles = lthi.pendingTree.tiles;

        quadViews = [aQuadView, pQuadView];
      }

      quadViews.forEach(function(qv) {
        qv.viewport = this.viewport_;
        qv.addEventListener('selection-changed',
                              this.onSelectionChanged_);
        this.quadViews_.appendChild(qv);
        qv.deviceViewportSizeForFrame = lthi.deviceViewportSize;
        qv.headerText =
          qv.headerPrefix + ': ' + qv.tiles.length + ' tiles';
        qv.colorMap = this.colorMapSelector_.selectedOptions[0].colorMap;
      }, this);



      this.headerTextEl_.textContent = lthi.inactiveTiles.length +
        ' inactive tiles';
      this.inactiveTileView_.tiles = lthi.inactiveTiles;


      this.activeLayerTreeImplView_.layerTreeImpl = this.lthi_.activeTree;
    },

    onSelectionChanged_: function(e) {
      var e2 = new base.Event('selection-changed');
      e2.selection = e.selection;
      this.dispatchEvent(e2);
      return true;
    }
  };

  function InactiveTileViewSelection(view, tiles) {
    this.view = view;
    this.tiles = tiles;
  }

  InactiveTileViewSelection.prototype = {
    activate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = true;
      });
      this.quad_view.viewport.didTileSelectednessChange();
      try {
        this.view.ignoreChangeEvents_ = true;
        this.view.selectedTile = this.tiles[0];
      } finally {
        this.view.ignoreChangeEvents_ = false;
      }
    },

    deactivate: function() {
      this.tiles.forEach(function(tile) {
        tile.selected = false;
      });
      this.quad_view.viewport.didTileSelectednessChange();
      try {
        this.view.ignoreChangeEvents_ = true;
        this.view.selectedElement = undefined;
      } finally {
        this.view.ignoreChangeEvents_ = false;
      }
    }
  }

  var InactiveTileView = ui.define('x-inactive-tile-view');

  InactiveTileView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.viewport_ = undefined;
      this.tiles_ = undefined;
      this.header_ = document.createElement('div');
      this.tileList_ = new ui.ListView();
      this.ignoreChangeEvents_ = false;
      this.tileList_.addEventListener(
        'selection-changed',
        this.onSelectionChanged_.bind(this));
      this.appendChild(this.header_);
      this.appendChild(this.tileList_);
    },

    set viewport(viewport) {
      this.viewport_ = viewport;
      this.updateChildren_();
    },

    get viewport() {
      return this.viewport_;
    },

    set tiles(tiles) {
      this.tiles_ = tiles;
      this.updateChildren_();
    },

    get tiles() {
      return this.tiles_;
    },

    set selectedTile(tile) {
      for (var i = 0; i < this.tileList.children.length; i++) {
        if(this.tileList.children[i].tile == tile) {
          this.tileList.children[i].selected = true;
          return;
        }
      }
      throw new Error('Tile not found');
    },

    updateChildren_: function() {
      this.header_.textContent = '';
      this.tileList_.textContent = '';
      if (!this.tiles_)
        return;
      this.header_.textContent = this.tiles_.length + ' inactive tiles';
      this.tileList_.textContent = '';
      this.tiles_.forEach(function(tile) {
        var tileEl = document.createElement('div');
        tileEl.tile = tile;
        tileEl.textContent = tile.id;
        tileEl.className = 'tile';
        this.tileList_.appendChild(tileEl);
      }.bind(this));
    },

    onSelectionChanged_: function() {
      if (this.ignoreChangeEvents_)
        return;
      var tiles = [];
      if (this.tileList_.selectedElement)
        tiles.push(this.tileList_.selectedElement.tile);
      var selection = new InactiveTileViewSelection(this, tiles);
      var e = new base.Event('inactive-tile-selection-changed', true);
      e.selection = selection;
      this.dispatchEvent(e);
    }
  }

  return {
    LTHIView: LTHIView,
    InactiveTileView: InactiveTileView,
  }
});

