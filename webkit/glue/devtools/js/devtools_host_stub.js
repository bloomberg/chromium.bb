// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview These stubs emulate backend functionality and allows
 * DevTools frontend to function as a standalone web app.
 */

/**
 * @constructor
 */
RemoteDebuggerAgentStub = function() {
  this.activeProfilerModules_ =
      devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_NONE;
  this.profileLogPos_ = 0;
  this.heapProfLog_ = '';
};


RemoteDebuggerAgentStub.prototype.DebugBreak = function() {
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
          'heap-sample-stats,"Heap","allocated",10000,1000\n' +
          'heap-js-cons-item,"foo",10,1000\n' +
          'heap-js-cons-item,"bar",20,2000\n' +
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


RemoteToolsAgentStub.prototype.HideDOMNodeHighlight = function() {
};


RemoteToolsAgentStub.prototype.HighlightDOMNode = function() {
};


RemoteToolsAgentStub.prototype.evaluate = function(expr) {
  window.eval(expr);
};

RemoteToolsAgentStub.prototype.EvaluateJavaScript = function(callId, script) {
  setTimeout(function() {
    var result = eval(script);
    RemoteToolsAgent.DidEvaluateJavaScript(callId, result);
  }, 0);
};


RemoteToolsAgentStub.prototype.ExecuteUtilityFunction = function(callId,
    functionName, args) {
  setTimeout(function() {
    var result = [];
    if (functionName == 'getProperties') {
      result = [
        'undefined', 'undefined_key', undefined,
        'string', 'string_key', 'value',
        'function', 'func', undefined,
        'array', 'array_key', [10],
        'object', 'object_key', undefined,
        'boolean', 'boolean_key', true,
        'number', 'num_key', 911,
        'date', 'date_key', new Date() ];
    } else if (functionName == 'getPrototypes') {
      result = ['Proto1', 'Proto2', 'Proto3'];
    } else if (functionName == 'getStyles') {
      result = {
        'computedStyle' : [0, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
        'inlineStyle' : [1, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
        'styleAttributes' : {
           attr: [2, '0px', '0px', null, null, null, ['display', false, false, '', 'none']]
        },
        'matchedCSSRules' : [
          { 'selector' : 'S',
            'style' : [3, '0px', '0px', null, null, null, ['display', false, false, '', 'none']],
            'parentStyleSheet' : { 'href' : 'http://localhost',
                                   'ownerNodeName' : 'DIV' }
          }
        ]
      };
    } else if (functionName == 'toggleNodeStyle' ||
        functionName == 'applyStyleText' ||
        functionName == 'setStyleProperty') {
      alert(functionName + '(' + args + ')');
    } else if (functionName == 'evaluate') {
      try {
        result = [ window.eval(JSON.parse(args)[0]), false ];
      } catch (e) {
        result = [ e.toString(), true ];
      }
    } else if (functionName == 'InspectorController' ||
        functionName == 'InjectedScript') {
      // do nothing;
    } else {
      alert('Unexpected utility function:' + functionName);
    }
    RemoteToolsAgent.DidExecuteUtilityFunction(callId,
        JSON.stringify(result), '');
  }, 0);
};


RemoteToolsAgentStub.prototype.SetResourceTrackingEnabled = function(enabled, always) {
  RemoteToolsAgent.SetResourcesPanelEnabled(enabled);
  if (enabled) {
    WebInspector.resourceTrackingWasEnabled();
  } else {
    WebInspector.resourceTrackingWasDisabled();
  }
  addDummyResource();
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
        '"handle":60,"type":"context","data":{"type":"page","value":3}}],' +
        '"running":false}';
    this.sendResponse_(response1);
  } else if ('{"seq":3,"type":"request","command":"scripts","arguments":{' +
             '"ids":[59],"includeSource":true}}' == cmd) {
    this.sendResponse_(
        '{"seq":8,"request_seq":3,"type":"response","command":"scripts",' +
        '"success":true,"body":[{"handle":1,"type":"script","name":' +
        '"http://www/~test/t.js","id":59,"lineOffset":0,"columnOffset":0,' +
        '"lineCount":1,"source":"function fib(n) {return n+1;}",' +
        '"sourceLength":244,"scriptType":2,"compilationType":0,"context":{' +
        '"ref":0}}],"refs":[{"handle":0,"type":"context","data":{"type":' +
        '"page","value":3}}],"running":false}');
  } else {
    debugPrint('Unexpected command: ' + cmd);
  }
};


RemoteDebuggerCommandExecutorStub.prototype.sendResponse_ = function(response) {
  setTimeout(function() {
    RemoteDebuggerAgent.DebuggerOutput(response);
  }, 0);
};


/**
 * @constructor
 */
DevToolsHostStub = function() {
  this.isStub = true;
  window.domAutomationController = {
    send: function(text) {
        debugPrint(text);
    }
  };
};


function addDummyResource() {
  var payload = {
    requestHeaders : {},
    requestURL: 'http://google.com/simple_page.html',
    host: 'google.com',
    path: 'simple_page.html',
    lastPathComponent: 'simple_page.html',
    isMainResource: true,
    cached: false,
    mimeType: 'text/html',
    suggestedFilename: 'simple_page.html',
    expectedContentLength: 10000,
    statusCode: 200,
    contentLength: 10000,
    responseHeaders: {},
    type: WebInspector.Resource.Type.Document,
    finished: true,
    startTime: new Date(),

    didResponseChange: true,
    didCompletionChange: true,
    didTypeChange: true
  };

  WebInspector.addResource(1, payload);
  WebInspector.updateResource(1, payload);
}


DevToolsHostStub.prototype.loaded = function() {
  addDummyResource();
  uiTests.runAllTests();
};


DevToolsHostStub.prototype.reset = function() {
};


DevToolsHostStub.prototype.getPlatform = function() {
  return "windows";
};


DevToolsHostStub.prototype.addResourceSourceToFrame = function(
    identifier, mimeType, element) {
};


DevToolsHostStub.prototype.addSourceToFrame = function(mimeType, source,
    element) {
};


if (!window['DevToolsHost']) {
  window['RemoteDebuggerAgent'] = new RemoteDebuggerAgentStub();
  window['RemoteDebuggerCommandExecutor'] =
      new RemoteDebuggerCommandExecutorStub();
  window['RemoteToolsAgent'] = new RemoteToolsAgentStub();
  window['DevToolsHost'] = new DevToolsHostStub();
}
