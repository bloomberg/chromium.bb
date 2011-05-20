// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Times how long the nexe took to load, then smoke tests it.

function LoadingTester(plugin, smokeTestSetup) {
  // Workaround how JS binds 'this'
  var this_ = this;

  this.smokeTestSetup_ = smokeTestSetup;
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
          this_.doSmokeTests_([plugin]);
        },
        function() {
          this_.rpc_.ping();
        }
      );
  }

  this.doSmokeTests_ = function(servers) {
    var tester = new Tester();
    for (var i = 0; i < servers.length; ++i) {
      this_.smokeTestSetup_(tester, servers[i]);
    }
    tester.run();
  }
}
