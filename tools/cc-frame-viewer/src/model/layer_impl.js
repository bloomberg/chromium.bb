/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('model.constants');

base.exportTo('ccfv.model', function() {

  /**
   * Represents a cc::LayerImpl over the course of its lifetime.
   *
   * @constructor
   */
  function LayerImplHistory(id) {
    this.id = id;
    this.layersBySnapshotNumber = {};
    this.args = {};
    this.selected = false;
  }

  LayerImplHistory.prototype = {
    get title() {
      return 'LayerImpl ' + this.id;
    },

    getOrCreateLayerImplForLTHI: function(lthi) {
      if (!this.layersBySnapshotNumber[lthi.snapshotNumber])
        this.layersBySnapshotNumber[lthi.snapshotNumber] = new LayerImpl(this);
      return this.layersBySnapshotNumber[lthi.snapshotNumber];
    },

    dumpToSimpleObject: function(obj) {
      obj.id = this.id;
      obj.args = this.args;
    }
  };

  /**
   * Represents a cc::LayerImpl at an instant in time.
   *
   * @constructor
   */
  function LayerImpl(history) {
    this.history = history;
    this.args = {}
  }

  LayerImpl.prototype = {
    get id() {
      return this.history.id;
    },

    get title() {
      return 'LayerImpl ' + this.id;
    },

    get selected() {
      return this.history.selected;
    },

    set selected(s) {
      this.history.selected = s;
    },

    dumpToSimpleObject: function(obj) {
      obj.history = {};
      obj.args = this.args;
    },
  };

  return {
    LayerImpl: LayerImpl,
    LayerImplHistory: LayerImplHistory
  }
});

