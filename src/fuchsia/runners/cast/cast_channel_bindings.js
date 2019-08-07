// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Implementation of the cast.__platform__.channel API which uses MessagePort
// IPC to communicate with an actual Cast Channel implementation provided by
// the content embedder. There is at most one channel which may be opened (able
// to send & receive messages) or closed.

// Don't clobber the Cast Channel API if it was previously injected.
if (!cast.__platform__.channel) {
  cast.__platform__.channel = new class {
    constructor() {
      this.master_port_ = cast.__platform__.PortConnector.bind(
          'cast.__platform__.channel');
    }

    // Signals to the peer that the Cast Channel is opened.
    // |openHandler|: A callback function which is invoked on channel open with
    //                a boolean indicating success.
    // |messageHandler|: Invoked when a message arrives from the peer.
    open(openHandler, messageHandler) {
      if (this.current_port_) {
        console.error('open() called on an open Cast Channel.');
        openHandler(false);
        return;
      }

      if (!messageHandler) {
        console.error('Null messageHandler passed to open().');
        openHandler(false);
        return;
      }

      // Create the MessageChannel for Cast Channel and distribute its ports.
      var channel = new MessageChannel();
      this.master_port_.postMessage('', [channel.port1]);

      this.current_port_ = channel.port2;
      this.current_port_.onmessage = function(message) {
        messageHandler(message.data);
      };
      this.current_port_.onerror = function() {
        console.error('Cast Channel was closed unexpectedly by peer.');
        return;
      };

      openHandler(true);
    }

    // Closes the Cast Channel.
    close(closeHandler) {
      if (this.current_port_) {
        this.current_port_.close();
        this.current_port_ = null;
      }
    }

    // Sends a message to the Cast Channel's peer.
    send(message) {
      if (!this.current_port_) {
        console.error('send() called on a closed Cast Channel.');
        return;
      }

      this.current_port_.postMessage(message);
    }

    // Used to send newly opened Cast Channel ports to C++.
    master_port_ = null;

    // The current opened Cast Channel.
    current_port_ = null;
  };
}
