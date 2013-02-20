/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.require('ui.list_and_associated_view');
base.require('layer_impl_view');
base.requireStylesheet('layer_tree_impl_view');
base.exportTo('ccfv', function() {
  var LayerTreeImplView = ui.define('x-layer-tree-impl-view');
  LayerTreeImplView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.layer_tree_impl_ = undefined;
      this.layerListEl_ = new ui.ListAndAssociatedView();
      this.layerListEl_.view = new ccfv.LayerImplView();
      this.layerListEl_.viewProperty = 'layerImpl';
      this.layerListEl_.listProperty = 'title';

      this.headerEl_ = ui.createSpan('')
      this.appendChild(this.headerEl_);
      this.appendChild(this.layerListEl_);
    },

    set header(text) {
      this.headerEl_.textContent = text;
    },

    set layerTreeImpl(layerTreeImpl) {
      this.layerTreeImpl_ = layerTreeImpl;
      this.updateChildren_();
    },

    updateChildren_: function() {
      this.layerListEl_.list = this.layerTreeImpl_.allLayers;
    }
  };

  return {
    LayerTreeImplView: LayerTreeImplView,
  }
});

