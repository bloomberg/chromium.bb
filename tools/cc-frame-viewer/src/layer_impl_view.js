/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('ui');
base.requireStylesheet('layer_impl_view');
base.exportTo('ccfv', function() {
  var LayerImplView = ui.define('x-layer-impl-view');
  LayerImplView.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      this.layerImpl_ = undefined;
    },

    set layerImpl(layerImpl) {
      this.layerImpl_ = layerImpl;
      this.updateChildren_();
    },

    updateChildren_: function() {
      this.textContent = '';
      if (!this.layerImpl_)
        return;

      var simpleObj = {};
      this.layerImpl_.dumpToSimpleObject(simpleObj);
      this.textContent = JSON.stringify(
        simpleObj,
        null,
        2);
    }
  };

  return {
    LayerImplView: LayerImplView
  }
});

