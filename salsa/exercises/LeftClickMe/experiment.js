// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var blocks = ['topLeft', 'topLeft', 'topLeft', 'topCenter', 'topCenter',
  'topCenter', 'topRight', 'topRight', 'topRight', 'middleLeft', 'middleLeft',
  'middleLeft', 'middleRight', 'middleRight', 'middleRight',
  'bottomLeft', 'bottomLeft', 'bottomLeft', 'bottomCenter', 'bottomCenter',
  'bottomCenter', 'bottomRight', 'bottomRight', 'bottomRight'];

// Metrics
var globals = {
  targetId: undefined,
  startPosition: undefined,
  endPosition: undefined,
  startTime: undefined,
  time: undefined,
  numberMissClickErrors: 0,
  missClickErrorPositions: '',
  numberWrongClickErrors: 0,
  wrongClickErrorPositions: '',
  mouseMoving: false,
  mouseMovePositions: '',
  data: []
};

$(document).ready(function() {

  $(document).bind('contextmenu',function(e){
    return false;
  });

  $(document).click(function(event){
    if (event.target.id === 'middleCenter') {
      globals.startPosition = event.pageX + ', ' + event.pageY;
      globals.mouseMoving = true;
      globals.startTime = new Date();
      $('#middleCenter').hide();
      var block = getRandomBlock();
      globals.targetId = block;
      $('#' + block).show();
    } else if (event.target.id != '') {
      if (event.which === 1) {
        $('#' + event.target.id).hide();
        globals.mouseMoving = false;
        globals.endPosition = event.pageX + ', ' + event.pageY;
        globals.time = new Date() - globals.startTime;
        recordData();
        resetData();

        if(blocks.length > 0) {
          $('#middleCenter').show();
        } else {
          history.back()
        }
      } else {
        globals.numberWrongClickErrors += 1;
        globals.wrongClickErrorPositions += event.pageX + ', ' +
                                            event.pageY + ' @ ' +
                                            new Date() + ' | ';
      }
    } else {
      globals.numberMissClickErrors += 1;
      globals.missClickErrorPositions += event.pageX + ', ' +
                                         event.pageY + ' @ ' +
                                         new Date() + ' | ';
    }
  });

  $(document).mousemove(function(event) {
    if (globals.mouseMoving === true) {
      globals.mouseMovePositions += event.pageX + ', ' + event.pageY + ' | ';
    }
  });
});

function getRandomBlock() {
  var i = Math.floor(Math.random() * blocks.length);
  var block = blocks[i];
  blocks.splice(i, 1);
  return block;
}

function recordData() {
  globals.data.push(new Trial(globals.targetId, globals.startPosition,
                              globals.endPosition, globals.time,
                              globals.numberWrongClickErrors,
                              globals.wrongClickErrorPositions,
                              globals.numberMissClickErrors,
                              globals.missClickErrorPositions,
                              globals.mouseMovePositions));
  localStorage['data'] = JSON.stringify(globals.data);
}

function resetData() {
  globals.numberMissClickErrors = 0;
  globals.missClickErrorPositions = '';
  globals.numberWrongClickErrors = 0;
  globals.wrongClickErrorPositions = '';
  globals.mouseMovePositions = '';
}

function Trial(targetId, startPosition, endPosition, time,
               numberWrongClickErrors, wrongClickErrorPositions,
               numberMissClickErrors, missClickErrorPositions,
               mouseMovePositions) {
  this.targetId = targetId;
  this.startPosition = startPosition;
  this.endPosition = endPosition;
  this.time = time;
  this.numberWrongClickErrors = numberWrongClickErrors;
  this.wrongClickErrorPositions = wrongClickErrorPositions;
  this.numberMissClickErrors = numberMissClickErrors;
  this.missClickErrorPositions = missClickErrorPositions;
  this.mouseMovePositions = mouseMovePositions;
}
