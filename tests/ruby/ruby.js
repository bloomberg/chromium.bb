/* Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file. */

var client;
var ruby;

var PostInit = function() {
  if (client == undefined) {
    alert('Unable to load, try reloading page (or missing plugins?)');
    return;
  }
  if (client.rubyEval === undefined) {
   setTimeout(PostInit, 100);
  } else {
    document.getElementById("output").innerHTML = "Ruby is loaded<br>";
  }
}

function escapeHTML(html) {
 if (html === null || typeof(html) == "undefined") {
   return "";
  }
  return html.replace(/&/g,'&amp;').replace(/>/g,'&gt;').
      replace(/</g,'&lt;').replace(/"/g,'&quot;');
}

function start() {
  client = document.getElementById('client');
  PostInit();
}

function doExecute() {
  var element = document.getElementById('rubyCode');
  var code = element.value;
  document.getElementById('output').innerHTML +=
      ("&gt; " + code + "<br>").replace('\n', '<br>');
  element.value = '';
  try {
    if (code.indexOf("puts ") != 0) {
      code = "puts " + code;
    }
    var result = client.rubyEval(code);
    document.getElementById('output').innerHTML +=
        escapeHTML(result).replace('\n', '<br>');
  } catch (e) {
    alert('error while executing Ruby code "' + code + '" err:' + e);
  }
}
