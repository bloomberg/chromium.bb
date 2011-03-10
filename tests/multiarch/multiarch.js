/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This script is invoked by  tools/selenium_tester.py, which evaluates
// these expressions:
//
//   wait for (window.nacllib.getStatus() != 'WAIT'")
//   window.nacllib.getStatus() // "" => success
//   window.nacllib.getMessage()
//
// I'm reinventing the Selenium NaclLib class here because the other
// implementation has overcomplex ordering requirements leading to race
// conditions.


// Prints a message to the JavaScript console.  It's not guaranteed to appear
// until we return control to the event loop.
//
// alert() doesn't work reliably under Selenium.  JavaScript doesn't have a
// print statement.  Someone explain to me why it should be easier to program
// in assembler than in JavaScript.  Awful.
function print(x) {
  setTimeout(function() { throw new Error(x); }, 0);
}

String.prototype.endsWith = function (s) {
  return this.length >= s.length &&
  this.substr(this.length - s.length) == s;
}

var nacllib = {};

nacllib.setStatus = function(status, message) {
  this.status = status;
  this.message = message;
  // For interactive users:
  document.getElementById('status').innerHTML = status + ": " + message;
};

nacllib.getStatus = function() { return this.status; };

nacllib.getMessage = function() { return this.message; };

nacllib.setStatus("WAIT", "");

// TODO(adonovan): onfail is not called reliably.  Fix and test.
function OnNaClFail() {
  print("Failed to load NaCl module.");
  nacllib.setStatus("ERROR", "Loading failed");
}

// This function is called when the loading of the nexe module completes
// successfully.  It must set the status to SUCCESS: or ERROR: as the last
// thing it does.
function OnNaClLoad() {
  print("Test called");

  var embed = document.getElementById("embed");

  // Check the module prints the correct string:
  var hello = embed.helloworld();
  if (hello != "hello, world.") {
    var error = "embed.helloworld() yielded wrong value: " + hello;
    nacllib.setStatus("ERROR", error);
    return error;
  }

  // Read back the __nacl property to ensure the manifest file was loaded.
  if (!embed.__nacl.endsWith("/srpc_hw.nmf")) {
    var error = "Wrong manifest: embed.__nacl doesn't end with \"srpc_hw.nmf\" "
        + "(" + embed.__nacl + ")."
    nacllib.setStatus("ERROR", error);
    return error;
  }

  // Read back the __src property to ensure correct nexe was chosen.
  if (!embed.__src.endsWith("/srpc_hw.nexe")) {
    var error = "Wrong nexe: embed.__src doesn't end with \"srpc_hw.nexe\" "
        + "(" + embed.__src + ")."
    nacllib.setStatus("ERROR", error);
    return error;
  }

  nacllib.setStatus("SUCCESS", "success");
  return "";  // Success.
}
