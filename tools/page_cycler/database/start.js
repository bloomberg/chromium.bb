// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.title = 'page cycler';

document.cookie = '__navigated_to_report=0; path=/';
document.cookie = '__pc_done=0; path=/';
document.cookie = '__pc_timings=; path=/';

var options = location.search.substring(1).split('&');

function getOption(name) {
  var r = new RegExp('^' + name + '=');
  for (var i = 0; i < options.length; i++) {
    if (options[i].match(r)) {
      return options[i].substring(name.length + 1);
    }
  }
  return null;
}

function start() {
  var iterations = document.getElementById('iterations').value;
  window.resizeTo(800, 800);
  var url = 'index.html?n=' + iterations + '&i=0&td=0';
  window.location = url;
}

function renderForm() {
  var form = document.createElement('form');
  form.onsubmit = function(e) {
    start();
    e.preventDefault();
  };

  var label = document.createTextNode('Iterations: ');
  form.appendChild(label);

  var input = document.createElement('input');
  input.id = 'iterations';
  input.type = 'number';
  var iterations = getOption('iterations');
  input.value = iterations ? iterations : '5';
  form.appendChild(input);

  input = document.createElement('input');
  input.type = 'submit';
  input.value = 'Start';
  form.appendChild(input);

  document.body.appendChild(form);
}

renderForm();

// should we start automatically?
if (location.search.match('auto=1'))
  start();
