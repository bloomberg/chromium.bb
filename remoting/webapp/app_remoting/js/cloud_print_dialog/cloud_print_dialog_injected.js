/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * The script file to be injected into the clould print dialog
 * (https://www.google.com/cloudprint/dialog.html).
 * Also see cloud_print_dialog_container.js
 */

(function () {

'use strict';

var hostWindow = null;

/**
 * Message handler responsible for setting up the communication channel with
 * the hosting window and redirecting to it the relevant cp-dialog messages.
 */
function onMessage(event) {
  var data = event.data;

  if (typeof data != 'string') {
    console.warn('Unknown message of type:' + typeof data);
    return;
  }

  switch (data) {
    case 'app-remoting-handshake':
      if (!hostWindow) {
        hostWindow = event.source;
        console.log('Handshake received in cloud print dialog.');
      } else {
        console.warn('Received duplicate handshake message.');
      }
      break;

    case 'cp-dialog-on-init::':
    case 'cp-dialog-on-close::':
      if (hostWindow) {
        var message = {'source': 'cloud-print-dialog', 'command': data};
        hostWindow.postMessage(message, '*');
      } else {
        console.warn('Message received before handshake: ' + data);
      }
      break;

    default:
      console.warn('Unknown message: ' + data);
  }
}

window.addEventListener('message', onMessage, false);

}) ();
