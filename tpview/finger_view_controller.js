// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FingerViewController(inGraph, outGraph) {
  this.entries = [];
  this.begin = -1;
  this.end = -1;

  this.gs_graph = outGraph;
  this.inGraph = inGraph;
};

FingerViewController.prototype = {
  setEntriesLog: function(log) {
    this.entries = log.entries;
    this.log = log
    if (this.entries.length > 0)
      this.begin = this.end = 0;
    else
      this.begin = this.end = -1;

    // Update input default zoom
    var hwprops = this.log.hardwareProperties;
    var xRes = hwprops.xResolution;
    var yRes = hwprops.yResolution;
    var left = hwprops.left / xRes;
    var right = hwprops.right / xRes;
    var top = hwprops.top / yRes;
    var bottom = hwprops.bottom / yRes;

    var height = bottom - top;
    var width = right - left;
    var borderHeight = height / 20;
    var borderWidth = width / 20;
    this.inGraph.setDefaultZoom(left - borderWidth,
                                top - borderHeight,
                                right + borderWidth,
                                bottom + borderHeight);
  },
  resetZooms: function() {
    this.inGraph.resetCoordinatesAndZoom();
    this.gs_graph.resetCoordinatesAndZoom();
  },
  prevHardwareState: function() {
    for (var i = this.end - 2; i >= 0; i--)
      if (this.entries[i].type == 'hardwareState')
        return i;
    return -1;
  },
  nextHardwareState: function() {
    for (var i = this.end; i < this.entries.length; i++)
      if (this.entries[i].type == 'hardwareState')
        return i;
    return -1;
  },
  setBeginPoint: function(index) {
    if (this.begin != index) {
      this.begin = index;
      turedraw();
    }
  },
  setEndPoint: function(index) {
    redraw();
  },
  setRange: function(begin, end) {
    this.begin = begin;
    this.end = end;
    this.redraw();
  },
  updateInput: function() {
    var lastPosDict = {};
    var segs = [];
    var lastPoints = [];
    var lastLines = [];
    // draw border
    var hp = this.log.hardwareProperties;
    var xRes = hp.xResolution;
    var yRes = hp.yResolution;
    var upLeft = {'xPos': hp.left / xRes,
                  'yPos': hp.top / yRes};
    var upRight = {'xPos': hp.right / xRes,
                   'yPos': hp.top / yRes};
    var botLeft = {'xPos': hp.left / xRes,
                   'yPos': hp.bottom / yRes};
    var botRight = {'xPos': hp.right / xRes,
                    'yPos': hp.bottom / yRes};
    segs.push({'start': upLeft,
               'end': upRight,
               'type': GraphController.LINE,
               'color': '#ccc'});
    segs.push({'start': upRight,
              'end': botRight,
              'type': GraphController.LINE,
              'color': '#ccc'});
    segs.push({'start': botRight,
               'end': botLeft,
               'type': GraphController.LINE,
               'color': '#ccc'});
    segs.push({'start': botLeft,
              'end': upLeft,
              'type': GraphController.LINE,
              'color': '#ccc'});
    var lastEntry = null;
    for (var i = this.begin; i < Math.min(this.end, this.entries.length); i++) {
      var entry = this.entries[i];
      if (entry.type != 'hardwareState') {
        continue;
      }
      var buttonDown = entry.buttonsDown != 0;
      var newLast = {};
      var lines = [];
      var points = [];
      for (var f = 0; f < entry.fingers.length; f++) {
        var fingerState = entry.fingers[f];
        var lastFingerState = null;
        if (lastEntry) {
          for (var lastf = 0; lastf < lastEntry.fingers.length; lastf++) {
            if (lastEntry.fingers[lastf].trackingId == fingerState.trackingId) {
              lastFingerState = lastEntry.fingers[lastf];
              break;
            }
          }
        }
        var trId = fingerState.trackingId;
        var pt = {'xPos': fingerState.positionX / xRes,
                  'yPos': fingerState.positionY / yRes};
        if (trId in lastPosDict) {
          // Draw line from previous point to here
          var line = {'start': lastPosDict[trId],
                      'end': pt,
                      'type': GraphController.LINE,
                      'color': '#000'};
          segs.push(line);
          lines.push(line);
        }
        newLast[trId] = pt;
        var color = '#ccc';
        var outPressure = fingerState.pressure *
            this.log.properties['Pressure Calibration Slope'] +
            this.log.properties['Pressure Calibration Offset'];
        var circle = {'type': GraphController.CIRCLE,
                      'center': pt,
                      'color': color,
                      'radius': outPressure / 4,
                      'label': fingerState.trackingId + ';' +
                               fingerState.pressure};
        segs.push(circle);
        points.push(circle);
      }
      lastPosDict = newLast;
      lastPoints = points;
      lastLines = lines;
      lastEntry = entry;
    }
    for (var i = 0; i < lastPoints.length; i++) {
      lastPoints[i].color = '#f99';
    }
    for (var i = 0; i < lastLines.length; i++) {
      lastLines[i].color = '#f99';
    }
    this.inGraph.setLineSegments(segs);
  },
  updateGs: function() {
    var xPos = 0;
    var yPos = 0;
    var buttonsDx = 5;
    var buttonsDy = 10;
    var xMin = Number.POSITIVE_INFINITY;
    var yMin = Number.POSITIVE_INFINITY;
    var xMax = Number.NEGATIVE_INFINITY;
    var yMax = Number.NEGATIVE_INFINITY;
    var segs = [];
    for (var i = this.begin; i < Math.min(this.end, this.entries.length); i++) {
      var entry = this.entries[i];
      if (entry.type == 'gesture') {
        if (entry.gestureType == 'scroll' || entry.gestureType == 'move') {
          var colors = {'scroll': '#00f', 'move': '#f00'};
          var endPt = {'xPos': (xPos + entry.dx), 'yPos': (yPos + entry.dy)};
          segs.push({'start': {'xPos': xPos, 'yPos': yPos},
                     'end': endPt,
                     'type': GraphController.LINE,
                     'color': colors[entry.gestureType]});
          xPos += entry.dx;
          yPos += entry.dy;
        } else if (entry.gestureType == 'buttonsChange') {
          var colors = ['#0f0', '#0a0', '#050'];
          for (var bt = 0; bt < 3; bt++) {
            var mask = 1 << bt;
            var color = colors[bt];
            if (entry.down & mask) {
              segs.push({'type': GraphController.LINE,
                         'start': {'xPos': xPos, 'yPos': yPos},
                         'end': {'xPos': (xPos + buttonsDx),
                                 'yPos': (yPos + buttonsDy)},
                         'color': color});
              xPos += buttonsDx;
              yPos += buttonsDy;
              xMin = Math.min(xMin, xPos);
              xMax = Math.max(xMax, xPos);
              yMin = Math.min(yMin, yPos);
              yMax = Math.max(yMax, yPos);
            }
            if (entry.up & mask) {
              segs.push({'type': GraphController.LINE,
                         'start': {'xPos': xPos, 'yPos': yPos},
                         'end': {'xPos': (xPos + buttonsDx),
                                 'yPos': (yPos - buttonsDy)},
                         'color': color});
              xPos += buttonsDx;
              yPos -= buttonsDy;
            }
            xMin = Math.min(xMin, xPos);
            xMax = Math.max(xMax, xPos);
            yMin = Math.min(yMin, yPos);
            yMax = Math.max(yMax, yPos);
          }
        }
        segs.push({'type': GraphController.CIRCLE,
                   'center': {'xPos': xPos, 'yPos': yPos},
                   'radius': 3,
                   'color': '#ccc'});
      }
      xMin = Math.min(xMin, xPos);
      xMax = Math.max(xMax, xPos);
      yMin = Math.min(yMin, yPos);
      yMax = Math.max(yMax, yPos);
    }
    if (xMin < Number.POSITIVE_INFINITY) {
      yBorder = (yMax - yMin) / 20;
      xBorder = (xMax - xMin) / 20;
      this.gs_graph.setDefaultZoom(xMin - xBorder,
                                   yMin - yBorder,
                                   xMax + xBorder,
                                   yMax + yBorder);
    }
    this.gs_graph.setLineSegments(segs);
  },
  redraw: function() {
    this.updateInput();
    this.updateGs();
  },
  colorForGesture: function(gs) {
    if (gs.gestureType == 'scroll')
      return 'rgb(255, 0, 0)';
    if (gs.gestureType == 'move')
      return 'rgb(0, 0, 255)';
    if (gs.gestureType == 'buttons')
      return 'rgb(0, 255, 0)';
  }
};
