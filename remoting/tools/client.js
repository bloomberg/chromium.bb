// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function init_params() {
  var hash;
  var hashes = window.location.href.slice(
      window.location.href.indexOf('?') + 1).split('&');
  var connect_params = document.forms[0];
  for(var i = 0; i < hashes.length; i++)
  {
    hash = hashes[i].split('=');
    if (hash[0] == "username") {
      connect_params.username.value = hash[1];
    } else if (hash[0] == "host_jid") {
      connect_params.host_jid.value = hash[1];
    } else if (hash[0] == "auth_token") {
      connect_params.auth_token.value = hash[1];
    }
  }
}

function do_connect(form) {
  debug_output("Attempt to connect with " + 
               "username='" + form.username.value + "'" +
               " host_jid='" + form.host_jid.value + "'" +
               " auth_token='" + form.auth_token.value + "'");

  document.chromoting.connect(form.username.value, form.host_jid.value, form.auth_token.value);
}

function debug_output(message) {
  var debug_div = document.getElementById('debug_div');
  var message_node = document.createElement('p');
  message_node.innerText = message;

  debug_div.appendChild(message_node);
}

