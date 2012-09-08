// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Times how long the nexe took to load, then smoke tests it.

function LoadingTester(plugin) {
  // Workaround how JS binds 'this'
  var this_ = this;

  this.rpc_ = new RPCWrapper();

  this.setupLoad = function() {
    var loadBegin = (new Date).getTime();
    this_.rpc_.startup();
    var waiter = new NaClWaiter();

    waiter.waitFor(plugin);
    waiter.run(
        function(loaded, waiting) {
          var loadEnd = (new Date).getTime();
          logLoadStatus(this_.rpc_, true, loaded, waiting);
          // Log the total in-browser load time (for first load).
          this_.rpc_.logTimeData('Total',
                                (loadEnd - loadBegin));
          this_.doSmokeTests_(plugin);
        },
        function() {
          this_.rpc_.ping();
        }
      );
  }

  this.doSmokeTests_ = function(plugin) {
    var tester = new Tester();
    tester.addAsyncTest('TestHello', function(status) {
        plugin.addEventListener('message', function(message_event) {
            this.removeEventListener('message', arguments.callee, false);
            status.assertEqual(message_event.data, 'hello from NaCl');
            status.pass();
          }, false);
        plugin.postMessage('hello');
      });
    tester.run();
  }
}
