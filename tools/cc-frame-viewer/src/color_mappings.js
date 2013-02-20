/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.require('base.color');
base.require('model.constants');

base.exportTo('ccfv', function() {
  var Color = base.Color;
  var constants = ccfv.model.constants;


  var colorInf = new Color(192, 192, 192, 0.5);
  var colorYes = new Color(0, 255, 0, 0.5);

  var colorInfAsString = colorInf.toString();
  var colorYesAsString = colorYes.toString();

  var allTileColorMaps = [
    {
      title: 'None',
      getValueForTile: function(tile, priority) {
        return undefined;
      },
      getBackgroundColorForValue: function(value) {
      },
    }
  ];

  // time_to_visible_in_seconds.
  var colorRedLo = new Color(255, 0, 0, 1);
  var colorRedHi = new Color(0, 0, 255, 1);
  allTileColorMaps.push({
    title: 'time_to_visible_in_seconds',
    getValueForTile: function(tile, priority) {
      return priority.time_to_visible_in_seconds;
    },
    getBackgroundColorForValue: function(value) {
      if (value > 1e9)
        return colorInfAsString;
      var percentage = Math.max(0, value / 10);
      var res = Color.lerpRGBA(colorRedLo, colorRedHi,
                               1 - percentage)
      return res.toString();
    },
  });

  // Distance.
  var colorGreenLo = new Color(0, 0, 255, 1);
  var colorGreenHi = new Color(0, 255, 0, 1);
  allTileColorMaps.push({
    title: 'distance_to_visible_in_pixels',
    getValueForTile: function(tile, priority) {
      return priority.distance_to_visible_in_pixels;
    },
    getBackgroundColorForValue: function(value) {
      if (value > 1e9)
        return colorInfAsString;
      var percentage = Math.max(0, value / 2000);
      var res = Color.lerpRGBA(colorGreenLo, colorGreenHi,
                               1 - percentage)
      return res.toString();
    },
  });

  // can_use_gpu_memory
  allTileColorMaps.push({
    title: 'can_use_gpu_memory',
    getValueForTile: function(tile, priority) {
      return tile.managedState.can_use_gpu_memory;
    },
    getBackgroundColorForValue: function(value) {
      return value ? colorYesAsString : undefined;
    },
  });

  // has_resource
  allTileColorMaps.push({
    title: 'has_resource (consuming gpu memory)',
    getValueForTile: function(tile, priority) {
      return tile.managedState.has_resource;
    },
    getBackgroundColorForValue: function(value) {
      return value ? colorYesAsString : undefined;
    },
  });

  // bins of various types
  var binToColorAsString = {
    'NOW_BIN': new Color(0, 0, 255, 1).toString(),
    'SOON_BIN': new Color(255, 255, 0, 1).toString(),
    'EVENTUALLY_BIN': new Color(0, 255, 255, 1).toString(),
    'NEVER_BIN': new Color(128, 128, 128, 1).toString(),
    '<unknown TileManagerBin value>': new Color(255, 0, 0, 1).toString()
  };

  allTileColorMaps.push({
    title: 'bin[HIGH_PRIORITY]',
    getValueForTile: function(tile, priority) {
      var bin = tile.managedState.bin[constants.HIGH_PRIORITY_BIN];
      if (binToColorAsString[bin] === undefined)
        throw new Error('Something has gone very wrong');
      return bin;
    },
    getBackgroundColorForValue: function(value) {
      return binToColorAsString[value];
    },
  });

  allTileColorMaps.push({
    title: 'bin[LOW_PRIORITY]',
    getValueForTile: function(tile, priority) {
      var bin = tile.managedState.bin[constants.LOW_PRIORITY_BIN];
      if (binToColorAsString[bin] === undefined)
        throw new Error('Something has gone very wrong');
      return bin;
    },
    getBackgroundColorForValue: function(value) {
      return binToColorAsString[value];
    },
  });

  allTileColorMaps.push({
    title: 'gpu_memmgr_stats_bin',
    getValueForTile: function(tile, priority) {
      var bin = tile.managedState.gpu_memmgr_stats_bin;
      if (binToColorAsString[bin] === undefined)
        throw new Error('Something has gone very wrong');
      return bin;
    },
    getBackgroundColorForValue: function(value) {
      return binToColorAsString[value];
    },
  });

  return {
    ALL_TILE_COLOR_MAPS: allTileColorMaps
  };
})
