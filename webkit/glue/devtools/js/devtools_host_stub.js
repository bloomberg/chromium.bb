// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview These stubs emulate backend functionality and allows
 * DevTools frontend to function as a standalone web app.
 */

if (!window['RemoteDebuggerAgent']) {

/**
 * @constructor
 */
RemoteDebuggerAgentStub = function() {
  this.activeProfilerModules_ =
      devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_NONE;
  this.profileLogPos_ = 0;
  this.heapProfSample_ = 0;
  this.heapProfLog_ = '';
};


RemoteDebuggerAgentStub.prototype.GetContextId = function() {
  RemoteDebuggerAgent.SetContextId(3);
};


RemoteDebuggerAgentStub.prototype.StopProfiling = function(modules) {
  this.activeProfilerModules_ &= ~modules;
};


RemoteDebuggerAgentStub.prototype.StartProfiling = function(modules) {
  var profModules = devtools.DebuggerAgent.ProfilerModules;
  if (modules & profModules.PROFILER_MODULE_HEAP_SNAPSHOT) {
    if (modules & profModules.PROFILER_MODULE_HEAP_STATS) {
      this.heapProfLog_ +=
          'heap-sample-begin,"Heap","allocated",' +
              (new Date()).getTime() + '\n' +
          'heap-sample-stats,"Heap","allocated",10000,1000\n';
      this.heapProfLog_ +=
          'heap-sample-item,STRING_TYPE,100,1000\n' +
          'heap-sample-item,CODE_TYPE,10,200\n' +
          'heap-sample-item,MAP_TYPE,20,350\n';
      this.heapProfLog_ += RemoteDebuggerAgentStub.HeapSamples[this.heapProfSample_++];
      this.heapProfSample_ %= RemoteDebuggerAgentStub.HeapSamples.length;
      this.heapProfLog_ +=
          'heap-sample-end,"Heap","allocated"\n';
    }
  } else {
    this.activeProfilerModules_ |= modules;
  }
};


RemoteDebuggerAgentStub.prototype.GetActiveProfilerModules = function() {
  var self = this;
  setTimeout(function() {
      RemoteDebuggerAgent.DidGetActiveProfilerModules(
          self.activeProfilerModules_);
  }, 100);
};


RemoteDebuggerAgentStub.prototype.GetNextLogLines = function() {
  var profModules = devtools.DebuggerAgent.ProfilerModules;
  var logLines = '';
  if (this.activeProfilerModules_ & profModules.PROFILER_MODULE_CPU) {
    if (this.profileLogPos_ < RemoteDebuggerAgentStub.ProfilerLogBuffer.length) {
      this.profileLogPos_ += RemoteDebuggerAgentStub.ProfilerLogBuffer.length;
      logLines += RemoteDebuggerAgentStub.ProfilerLogBuffer;
    }
  }
  if (this.heapProfLog_) {
    logLines += this.heapProfLog_;
    this.heapProfLog_ = '';
  }
  setTimeout(function() {
    RemoteDebuggerAgent.DidGetNextLogLines(logLines);
  }, 100);
};


/**
 * @constructor
 */
RemoteToolsAgentStub = function() {
};


RemoteToolsAgentStub.prototype.DispatchOnInjectedScript = function() {
};


RemoteToolsAgentStub.prototype.DispatchOnInspectorController = function() {
};


RemoteToolsAgentStub.prototype.ExecuteVoidJavaScript = function() {
};


RemoteDebuggerAgentStub.ProfilerLogBuffer =
  'profiler,begin,1\n' +
  'profiler,resume\n' +
  'code-creation,LazyCompile,0x1000,256,"test1 http://aaa.js:1"\n' +
  'code-creation,LazyCompile,0x2000,256,"test2 http://bbb.js:2"\n' +
  'code-creation,LazyCompile,0x3000,256,"test3 http://ccc.js:3"\n' +
  'tick,0x1010,0x0,3\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x3010,0x0,3,0x2020, 0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x2030,0x0,3,0x2020, 0x1010\n' +
  'tick,0x2020,0x0,3,0x1010\n' +
  'tick,0x1010,0x0,3\n' +
  'profiler,pause\n';


RemoteDebuggerAgentStub.HeapSamples = [
  'heap-js-cons-item,foo,1,100\n' +
  'heap-js-cons-item,bar,20,2000\n' +
  'heap-js-cons-item,Object,5,100\n' +
  'heap-js-ret-item,foo,bar;3\n' +
  'heap-js-ret-item,bar,foo;5\n' +
  'heap-js-ret-item,Object:0x1234,(roots);1\n',

  'heap-js-cons-item,foo,2000,200000\n' +
  'heap-js-cons-item,bar,10,1000\n' +
  'heap-js-cons-item,Object,6,120\n' +
  'heap-js-ret-item,foo,bar;7,Object:0x1234;10\n' +
  'heap-js-ret-item,bar,foo;10,Object:0x1234;10\n' +
  'heap-js-ret-item,Object:0x1234,(roots);1\n',

  'heap-js-cons-item,foo,15,1500\n' +
  'heap-js-cons-item,bar,15,1500\n' +
  'heap-js-cons-item,Object,5,100\n' +
  'heap-js-cons-item,Array,3,1000\n' +
  'heap-js-ret-item,foo,bar;3,Array:0x5678;1\n' +
  'heap-js-ret-item,bar,foo;5,Object:0x1234;8,Object:0x5678;2\n' +
  'heap-js-ret-item,Object:0x1234,(roots);1,Object:0x5678;2\n' +
  'heap-js-ret-item,Object:0x5678,(global property);3,Object:0x1234;5\n' +
  'heap-js-ret-item,Array:0x5678,(global property);3,Array:0x5678;2\n',

  'heap-js-cons-item,bar,20,2000\n' +
  'heap-js-cons-item,Object,6,120\n' +
  'heap-js-ret-item,bar,foo;5,Object:0x1234;1,Object:0x1235;3\n' +
  'heap-js-ret-item,Object:0x1234,(global property);3\n' +
  'heap-js-ret-item,Object:0x1235,(global property);5\n',

  'heap-js-cons-item,foo,15,1500\n' +
  'heap-js-cons-item,bar,15,1500\n' +
  'heap-js-cons-item,Array,10,1000\n' +
  'heap-js-ret-item,foo,bar;1,Array:0x5678;1\n' +
  'heap-js-ret-item,bar,foo;5\n' +
  'heap-js-ret-item,Array:0x5678,(roots);3\n',

  'heap-js-cons-item,bar,20,2000\n' +
  'heap-js-cons-item,baz,15,1500\n' +
  'heap-js-ret-item,bar,baz;3\n' +
  'heap-js-ret-item,baz,bar;3\n'
];


/**
 * @constructor
 */
RemoteDebuggerCommandExecutorStub = function() {
};


RemoteDebuggerCommandExecutorStub.prototype.DebuggerCommand = function(cmd) {
  if ('{"seq":2,"type":"request","command":"scripts","arguments":{"' +
      'includeSource":false}}' == cmd) {
    var response1 =
        '{"seq":5,"request_seq":2,"type":"response","command":"scripts","' +
        'success":true,"body":[{"handle":61,"type":"script","name":"' +
        'http://www/~test/t.js","id":59,"lineOffset":0,"columnOffset":0,' +
        '"lineCount":1,"sourceStart":"function fib(n) {","sourceLength":300,' +
        '"scriptType":2,"compilationType":0,"context":{"ref":60}}],"refs":[{' +
        '"handle":60,"type":"context","data":"page,3"}],"running":false}';
    this.sendResponse_(response1);
  } else if ('{"seq":3,"type":"request","command":"scripts","arguments":{' +
             '"ids":[59],"includeSource":true}}' == cmd) {
    this.sendResponse_(
        '{"seq":8,"request_seq":3,"type":"response","command":"scripts",' +
        '"success":true,"body":[{"handle":1,"type":"script","name":' +
        '"http://www/~test/t.js","id":59,"lineOffset":0,"columnOffset":0,' +
        '"lineCount":1,"source":"function fib(n) {return n+1;}",' +
        '"sourceLength":244,"scriptType":2,"compilationType":0,"context":{' +
        '"ref":0}}],"refs":[{"handle":0,"type":"context","data":"page,3}],"' +
        '"running":false}');
  } else {
    debugPrint('Unexpected command: ' + cmd);
  }
};


RemoteDebuggerCommandExecutorStub.prototype.DebuggerPauseScript = function() {
};


RemoteDebuggerCommandExecutorStub.prototype.sendResponse_ = function(response) {
  setTimeout(function() {
    RemoteDebuggerAgent.DebuggerOutput(response);
  }, 0);
};


DevToolsHostStub = function() {
  this.isStub = true;
};
goog.inherits(DevToolsHostStub,
    WebInspector.InspectorFrontendHostStub);


DevToolsHostStub.prototype.reset = function() {
};


DevToolsHostStub.prototype.setting = function() {
};


DevToolsHostStub.prototype.setSetting = function() {
};


window['RemoteDebuggerAgent'] = new RemoteDebuggerAgentStub();
window['RemoteDebuggerCommandExecutor'] =
    new RemoteDebuggerCommandExecutorStub();
window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
InspectorFrontendHost = new DevToolsHostStub();

}
