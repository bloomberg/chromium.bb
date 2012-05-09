// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FingerViewController(inGraph, outGraph, inText) {
  this.entries = [];
  this.begin = -1;
  this.end = -1;

  this.gs_graph = outGraph;
  this.inGraph = inGraph;
  this.inText = inText;
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
  prevHardwareState: function(start) {
    for (var i = start - 1; i >= 0; i--)
      if (this.entries[i].type == 'hardwareState')
        return i;
    return -1;
  },
  nextHardwareState: function(start) {
    for (var i = start + 1; i < this.entries.length; i++)
      if (this.entries[i].type == 'hardwareState')
        return i;
    return -1;
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
    var prevLastEntry = null;
    var end = Math.min(this.end + 1, this.entries.length);
    for (var i = this.begin; i < end; i++) {
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
                      outPressure.toFixed(2)};
        segs.push(circle);
        points.push(circle);
      }
      lastPosDict = newLast;
      lastPoints = points;
      lastLines = lines;
      prevLastEntry = lastEntry;
      lastEntry = entry;
    }
    for (var i = 0; i < lastPoints.length; i++) {
      lastPoints[i].color = '#f99';
    }
    for (var i = 0; i < lastLines.length; i++) {
      lastLines[i].color = '#f99';
    }
    this.inGraph.setLineSegments(segs);
    if (lastEntry) {
      var dist = "N/A";
      if (lastEntry.fingers.length == 2) {
        var dx = (lastEntry.fingers[1].positionX -
                  lastEntry.fingers[0].positionX) / xRes;
        var dy = (lastEntry.fingers[1].positionY -
                  lastEntry.fingers[0].positionY) / yRes;
        dist = Math.sqrt(dx * dx + dy * dy) + "mm";
      }
      var fingerStrings = [];
      for (var i = 0; i < lastEntry.fingers.length; i++) {
        var stringEntry = "" + lastEntry.fingers[i].trackingId;
        var fingerState = lastEntry.fingers[i];
        var outPressure = fingerState.pressure *
            this.log.properties['Pressure Calibration Slope'] +
            this.log.properties['Pressure Calibration Offset'];
        var xPos = fingerState.positionX / xRes;
        var yPos = fingerState.positionY / yRes;
        stringEntry += " (" + xPos.toFixed(2) + ", " + yPos.toFixed(2) + ")";
        stringEntry += " pr: " + outPressure.toFixed(2);
        if (prevLastEntry) {
          var dt = prevLastEntry.timestamp - lastEntry.timestamp;
          var angles = [];
          for (var j = 0; j < prevLastEntry.fingers.length; j++) {
            if (prevLastEntry.fingers[j].trackingId ==
                lastEntry.fingers[i].trackingId) {
              var dx = lastEntry.fingers[i].positionX -
                  prevLastEntry.fingers[j].positionX;
              var dy = lastEntry.fingers[i].positionY -
                  prevLastEntry.fingers[j].positionY;
              dx /= xRes;
              dy /= yRes;
              stringEntry += " (dx/dt: " + (dx/dt).toFixed(2) + ", dy/dt: " + (dy/dt).toFixed(2) + " flags: " + lastEntry.fingers[i].flags + ")";
            }
          }
        }
        fingerStrings.push(stringEntry);
      }
      var fingerString = ": " + fingerStrings.join(", ");
      this.inText[0].innerHTML = "timestamp: " + lastEntry.timestamp +
      "<br/>" + "fingerCount: " + lastEntry.fingers.length + fingerString +
      "<br/>" + "touchCount: " + lastEntry.touchCount +
      "<br/>" + "button: " + (lastEntry.buttonsDown ? "DOWN" : "UP") +
      "<br/>" + "dist: " + dist;
    }
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
    var end = Math.min(this.end + 1, this.entries.length);
    for (var i = this.begin; i < end; i++) {
      var entry = this.entries[i];
      if (entry.type == 'gesture') {
        if (entry.gestureType == 'scroll' || entry.gestureType == 'move' ||
            entry.gestureType == 'fling') {
          if (entry.gestureType == 'scroll' || entry.gestureType == 'move') {
            var dx = entry.dx;
            var dy = entry.dy;
          } else {
            var dt = entry.endTime - entry.startTime;
            var dx = entry.vx * dt;
            var dy = entry.vy * dt;
          }
          if (entry.gestureType == 'scroll') {
            if (dy != 0 && dx == 0)
              dx = 1;
          }
          var colors = {'scroll': '#00f', 'move': '#f00', 'fling': '#ff832c'};
          var endPt = {'xPos': (xPos + dx), 'yPos': (yPos + dy)};
          segs.push({'start': {'xPos': xPos, 'yPos': yPos},
                     'end': endPt,
                     'type': GraphController.LINE,
                     'color': colors[entry.gestureType]});
          xPos += dx;
          yPos += dy;
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
