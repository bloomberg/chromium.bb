// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//                 P L E A S E   N O T E
// This is a temporary file-loading solution for Native Client
// dynamic libraries, pending the permanent solution documented in:
//   http://code.google.com/p/nativeclient/issues/detail?id=1534
// This mechanism should go away once the permanent solution is
// in place.

// This is a helper library for launching dynamically-linked programs
// inside the browser.  It sends initial arguments to the NaCl process
// and fetches library files on behalf of the NaCl process.


// Encode a little-endian int32 or uint32.
var packInt = function(value) {
  return (String.fromCharCode(value & 0xff) +
          String.fromCharCode((value >> 8) & 0xff) +
          String.fromCharCode((value >> 16) & 0xff) +
          String.fromCharCode((value >> 24) & 0xff));
};

// Encode a list of strings as a string.
var packStringList = function(strings) {
  var temp = [];
  for (var i = 0; i < strings.length; i++) {
    temp.push(strings[i]);
    temp.push('\0');
  }
  return temp.join('');
};

// Encode a startup message containing argv and env arrays.
var packArgs = function(argv, envv) {
  return ('ARGS' +
          packInt(argv.length) +
          packInt(envv.length) +
          packStringList(argv) +
          packStringList(envv));
};

// Hex-encode a byte string.
var encodeHex = function(data) {
  var temp = [];
  for (var i = 0; i < data.length; i++) {
    var byte = data.charCodeAt(i);
    if (byte < 16) {
      temp.push('0');
    }
    temp.push(byte.toString(16));
  }
  return temp.join('');
};


var handlePluginInstance = function(plugin, libdir, log, args,
                                    onload_callback) {
  var argv = [args[0], '--library-path', libdir].concat(args);
  var envv = ['NACL_FILE_RPC=1', 'NACL_LD_ACCEPTS_PLUGIN_CONNECTION=1'];
  var startup_message = packArgs(argv, envv);
  /* As a workaround for a limitation in the NaCl plugin, hex-encode the
     message.  This is because __sendAsyncMessage*() does not support
     null bytes in messages.
     See http://code.google.com/p/nativeclient/issues/detail?id=1535
     TODO(mseaborn): Fix this limitation.  */
  startup_message = 'HEXD' + encodeHex(startup_message);
  plugin.__sendAsyncMessage0(startup_message);

  // Handles open() calls made by the NaCl process.
  var handleOpenRequest = function(message_body) {
    // Ignore the "flags" and "mode" arguments (two int32s).
    // We only handle opening files for reading.
    var filename = message_body.substring(8);
    log('open: ' + filename);
    plugin.__urlAsNaClDesc(filename, {
      onload: function(fd) {
        plugin.__sendAsyncMessage1('Okay', fd);
      },
      onfail: function(error) {
        log('fetch failed: ' + error);
        plugin.__sendAsyncMessage0('Fail');
      }
    });
  };

  var handleInitRequest = function(message_body) {
    log('starting PPAPI...');
    plugin.__startSrpcServices();
    onload_callback();
  };

  var handlers = {
    'Open': handleOpenRequest,
    'Init': handleInitRequest
  };

  var handleRequest = function(message) {
    // The first 4 bytes are a message type tag.
    var method_id = message.substring(0, 4);
    var message_body = message.substring(4);
    if (method_id in handlers) {
      handlers[method_id](message_body);
    } else {
      log('Unhandled message: ' + message);
    }
  };

  plugin.__setAsyncCallback(handleRequest);
};

var startPluginInstance = function(plugin, libdir, log, args,
                                   onload_callback) {
  log('startPluginInstance');
  try {
    var my_isa = plugin.__getSandboxISA();
    var dynamic_linker_url = libdir[my_isa] + '/runnable-ld.so';
    var continuation = function(succeeded) {
      if (succeeded)
        handlePluginInstance(plugin, libdir[my_isa], log, args[my_isa],
                             onload_callback);
    }

    plugin.__urlAsNaClDesc(dynamic_linker_url, {
     onload: function(fd) {
       plugin.__launchExecutableFromFd(fd, continuation);
     },
     onfail: function(error) {
       log('Failed to fetch dynamic linker, ' +
           dynamic_linker_url + ': ' + error);
     }
    });
  } catch (err) {
    log(err);
    log("Could not start plugin instance");
  }
};
