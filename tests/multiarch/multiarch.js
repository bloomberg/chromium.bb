/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

String.prototype.endsWith = function (s) {
  return this.length >= s.length &&
  this.substr(this.length - s.length) == s;
}

// This function is called when the loading of the nexe module completes
// successfully.
function OnNaClLoad() {
  var tester = new Tester();
  var nacl_module = $("nacl_module");

  // Check that the correct manifest URL string is set on the module.
  tester.addTest('__nacl_property', function() {
    assert(nacl_module.__nacl.endsWith("multiarch.nmf"));
  });

  // Check that the correct nexe URL is set on the module.
  tester.addTest('__src_property', function() {
    assert(nacl_module.__src.search("srpc_hw"));
  });

  // Check that the module prints the correct string.
  tester.addTest('hello_world', function() {
    var hello = nacl_module.helloworld();
    assertEqual(hello, "hello, world.");
  });

  tester.waitFor(nacl_module);
  tester.run();
}
