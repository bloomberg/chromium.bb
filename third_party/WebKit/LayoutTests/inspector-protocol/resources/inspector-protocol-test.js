// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TestRunner = class {
  constructor(baseURL, log, completeTest, fetch) {
    this._dumpInspectorProtocolMessages = false;
    this._baseURL = baseURL;
    this._log = log;
    this._completeTest = completeTest;
    this._fetch = fetch;
  }

  startDumpingProtocolMessages() {
    this._dumpInspectorProtocolMessages = true;
  };

  completeTest() {
    this._completeTest.call(null);
  }

  log(text) {
    this._log.call(null, text);
  }

  logMessage(originalMessage) {
    var message = JSON.parse(JSON.stringify(originalMessage));
    if (message.id)
      message.id = '<messageId>';
    const nonStableFields = new Set(['nodeId', 'objectId', 'scriptId', 'timestamp']);
    var objects = [message];
    while (objects.length) {
      var object = objects.shift();
      for (var key in object) {
        if (nonStableFields.has(key))
          object[key] = `<${key}>`;
        else if (typeof object[key] === 'string' && object[key].match(/\d+:\d+:\d+:debug/))
          object[key] = object[key].replace(/\d+/, '<scriptId>');
        else if (typeof object[key] === 'object')
          objects.push(object[key]);
      }
    }
    this.logObject(message);
    return originalMessage;
  }

  logObject(object, title) {
    var lines = [];

    function dumpValue(value, prefix, prefixWithName) {
      if (typeof value === 'object' && value !== null) {
        if (value instanceof Array)
          dumpItems(value, prefix, prefixWithName);
        else
          dumpProperties(value, prefix, prefixWithName);
      } else {
        lines.push(prefixWithName + String(value).replace(/\n/g, ' '));
      }
    }

    function dumpProperties(object, prefix, firstLinePrefix) {
      prefix = prefix || '';
      firstLinePrefix = firstLinePrefix || prefix;
      lines.push(firstLinePrefix + '{');

      var propertyNames = Object.keys(object);
      propertyNames.sort();
      for (var i = 0; i < propertyNames.length; ++i) {
        var name = propertyNames[i];
        if (!object.hasOwnProperty(name))
          continue;
        var prefixWithName = '    ' + prefix + name + ' : ';
        dumpValue(object[name], '    ' + prefix, prefixWithName);
      }
      lines.push(prefix + '}');
    }

    function dumpItems(object, prefix, firstLinePrefix) {
      prefix = prefix || '';
      firstLinePrefix = firstLinePrefix || prefix;
      lines.push(firstLinePrefix + '[');
      for (var i = 0; i < object.length; ++i)
        dumpValue(object[i], '    ' + prefix, '    ' + prefix + '[' + i + '] : ');
      lines.push(prefix + ']');
    }

    dumpValue(object, '', title || '');
    this.log(lines.join('\n'));
  }

  url(relative) {
    return this._baseURL + relative;
  }

  _checkExpectation(fail, name, messageObject) {
    if (fail === !!messageObject.error) {
      this.log('PASS: ' + name);
      return true;
    }

    this.log('FAIL: ' + name + ': ' + JSON.stringify(messageObject));
    this.completeTest();
    return false;
  }

  expectedSuccess(name, messageObject) {
    return this._checkExpectation(false, name, messageObject);
  }

  expectedError(name, messageObject) {
    return this._checkExpectation(true, name, messageObject);
  }

  die(message, error) {
    this.log(`${message}: ${error}\n${error.stack}`);
    this.completeTest();
    throw new Error(message);
  }

  async loadScript(url) {
    var source = await this._fetch(this.url(url));
    return eval(`${source}\n//# sourceURL=${url}`);
  };

  async createPage() {
    var targetId = (await DevToolsAPI._sendCommandOrDie('Target.createTarget', {url: 'about:blank'})).targetId;
    await DevToolsAPI._sendCommandOrDie('Target.activateTarget', {targetId});
    var page = new TestRunner.Page(this, targetId);
    var dummyURL = window.location.href;
    dummyURL = dummyURL.substring(0, dummyURL.indexOf('inspector-protocol-test.html')) + 'inspector-protocol-page.html';
    await page._navigate(dummyURL);
    return page;
  }

  async _start(description, html, url) {
    try {
      this.log(description);
      var page = await this.createPage();
      if (url)
        await page.navigate(url);
      if (html)
        await page.loadHTML(html);
      var session = await page.createSession();
      return { page: page, session: session, dp: session.protocol };
    } catch (e) {
      this.die('Error starting the test', e);
    }
  };

  startBlank(description) {
    return this._start(description, null, null);
  }

  startHTML(html, description) {
    return this._start(description, html, null);
  }

  startURL(url, description) {
    return this._start(description, null, url);
  }
};

TestRunner.Page = class {
  constructor(testRunner, targetId) {
    this._testRunner = testRunner;
    this._targetId = targetId;
  }

  async createSession() {
    await DevToolsAPI._sendCommandOrDie('Target.attachToTarget', {targetId: this._targetId});
    var session = new TestRunner.Session(this);
    DevToolsAPI._sessions.set(this._targetId, session);
    return session;
  }

  navigate(url) {
    return this._navigate(this._testRunner.url(url));
  }

  async _navigate(url) {
    if (DevToolsAPI._sessions.get(this._targetId))
      this._testRunner.die(`Cannot navigate to ${url} with active session`, new Error());

    var session = await this.createSession();
    session.protocol.Page.enable();
    session.protocol.Page.navigate({url: url});

    var callback;
    var promise = new Promise(f => callback = f);
    session.protocol.Page.onFrameNavigated(message => {
      if (!message.params.frame.parentId)
        callback();
    });
    await promise;

    await session.disconnect();
  }

  async loadHTML(html) {
    if (DevToolsAPI._sessions.get(this._targetId))
      this._testRunner.die('Cannot loadHTML with active session', new Error());

    html = html.replace(/'/g, "\\'").replace(/\n/g, '\\n');
    var session = await this.createSession();
    await session.protocol.Runtime.evaluate({expression: `document.body.innerHTML='${html}'`});
    await session.disconnect();
  }
};

TestRunner.Session = class {
  constructor(page) {
    this._testRunner = page._testRunner;
    this._page = page;
    this._requestId = 0;
    this._dispatchTable = new Map();
    this._eventHandlers = new Map();
    this.protocol = this._setupProtocol();
  }

  async disconnect() {
    await DevToolsAPI._sendCommandOrDie('Target.detachFromTarget', {targetId: this._page._targetId});
    DevToolsAPI._sessions.delete(this._page._targetId);
  }

  sendRawCommand(requestId, message) {
    DevToolsAPI._sendCommandOrDie('Target.sendMessageToTarget', {targetId: this._page._targetId, message: message});
    return new Promise(f => this._dispatchTable.set(requestId, f));
  }

  sendCommand(method, params) {
    var requestId = ++this._requestId;
    var messageObject = {'id': requestId, 'method': method, 'params': params};
    if (this._testRunner._dumpInspectorProtocolMessages)
      this._testRunner.log(`frontend => backend: ${JSON.stringify(messageObject)}`);
    return this.sendRawCommand(requestId, JSON.stringify(messageObject));
  }

  async evaluate(code) {
    var response = await this.protocol.Runtime.evaluate({expression: code, returnByValue: true});
    if (response.error) {
      this._testRunner.log(`Error while evaluating '${code}': ${response.error}`);
      this._testRunner.completeTest();
    } else {
      return response.result.result.value;
    }
  }

  async evaluateAsync(code) {
    var response = await this.protocol.Runtime.evaluate({expression: code, returnByValue: true, awaitPromise: true});
    if (response.error) {
      this._testRunner.log(`Error while evaluating async '${code}': ${response.error}`);
      this._testRunner.completeTest();
    } else {
      return response.result.result.value;
    }
  }

  _dispatchMessage(message) {
    if (this._testRunner._dumpInspectorProtocolMessages)
      this._testRunner.log(`backend => frontend: ${JSON.stringify(message)}`);
    if (typeof message.id === 'number') {
      var handler = this._dispatchTable.get(message.id);
      if (handler) {
        this._dispatchTable.delete(message.id);
        handler(message);
      }
    } else {
      var eventName = message.method;
      for (var handler of (this._eventHandlers.get(eventName) || []))
        handler(message);
    }
  }

  _setupProtocol() {
    return new Proxy({}, { get: (target, agentName, receiver) => new Proxy({}, {
      get: (target, methodName, receiver) => {
        const eventPattern = /^(on(ce)?|off)([A-Z][A-Za-z0-9]+)/;
        var match = eventPattern.exec(methodName);
        if (!match)
          return args => this.sendCommand(`${agentName}.${methodName}`, args || {});
        var eventName = match[3];
        eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
        if (match[1] === 'once')
          return () => this._waitForEvent(`${agentName}.${eventName}`);
        if (match[1] === 'off')
          return listener => this._removeEventHandler(`${agentName}.${eventName}`, listener);
        return listener => this._addEventHandler(`${agentName}.${eventName}`, listener);
      }
    })});
  }

  _addEventHandler(eventName, handler) {
    var handlers = this._eventHandlers.get(eventName) || [];
    handlers.push(handler);
    this._eventHandlers.set(eventName, handlers);
  }

  _removeEventHandler(eventName, handler) {
    var handlers = this._eventHandlers.get(eventName) || [];
    var index = handlers.indexOf(handler);
    if (index === -1)
      return;
    handlers.splice(index, 1);
    this._eventHandlers.set(eventName, handlers);
  }

  _waitForEvent(eventName) {
    return new Promise(callback => {
      var handler = result => {
        this._removeEventHandler(eventName, handler);
        callback(result);
      };
      this._addEventHandler(eventName, handler);
    });
  }
};

var DevToolsAPI = {};
DevToolsAPI._requestId = 0;
DevToolsAPI._embedderMessageId = 0;
DevToolsAPI._dispatchTable = new Map();
DevToolsAPI._sessions = new Map();
DevToolsAPI._outputElement = null;

DevToolsAPI._log = function(text) {
  if (!DevToolsAPI._outputElement) {
    var intermediate = document.createElement('div');
    document.body.appendChild(intermediate);
    var intermediate2 = document.createElement('div');
    intermediate.appendChild(intermediate2);
    DevToolsAPI._outputElement = document.createElement('div');
    DevToolsAPI._outputElement.className = 'output';
    DevToolsAPI._outputElement.id = 'output';
    DevToolsAPI._outputElement.style.whiteSpace = 'pre';
    intermediate2.appendChild(DevToolsAPI._outputElement);
  }
  DevToolsAPI._outputElement.appendChild(document.createTextNode(text));
  DevToolsAPI._outputElement.appendChild(document.createElement('br'));
};

DevToolsAPI._completeTest = function() {
  window.testRunner.notifyDone();
};

DevToolsAPI._die = function(message, error) {
  DevToolsAPI._log(`${message}: ${error}\n${error.stack}`);
  DevToolsAPI._completeTest();
  throw new Error();
};

DevToolsAPI.dispatchMessage = function(messageOrObject) {
  var messageObject = (typeof messageOrObject === 'string' ? JSON.parse(messageOrObject) : messageOrObject);
  var messageId = messageObject.id;
  try {
    if (typeof messageId === 'number') {
      var handler = DevToolsAPI._dispatchTable.get(messageId);
      if (handler) {
        DevToolsAPI._dispatchTable.delete(messageId);
        handler(messageObject);
      }
    } else {
      var eventName = messageObject.method;
      if (eventName === 'Target.receivedMessageFromTarget') {
        var targetId = messageObject.params.targetId;
        var message = messageObject.params.message;
        var session = DevToolsAPI._sessions.get(targetId);
        if (session)
          session._dispatchMessage(JSON.parse(message));
      }
    }
  } catch(e) {
    DevToolsAPI._die(`Exception when dispatching message\n${JSON.stringify(messageObject)}`, e);
  }
};

DevToolsAPI._sendCommand = function(method, params) {
  var requestId = ++DevToolsAPI._requestId;
  var messageObject = {'id': requestId, 'method': method, 'params': params};
  var embedderMessage = {'id': ++DevToolsAPI._embedderMessageId, 'method': 'dispatchProtocolMessage', 'params': [JSON.stringify(messageObject)]};
  DevToolsHost.sendMessageToEmbedder(JSON.stringify(embedderMessage));
  return new Promise(f => DevToolsAPI._dispatchTable.set(requestId, f));
};

DevToolsAPI._sendCommandOrDie = function(method, params) {
  return DevToolsAPI._sendCommand(method, params).then(message => {
    if (message.error)
      DevToolsAPI._die('Error communicating with harness', new Error(message.error));
    return message.result;
  });
};

function debugTest(testFunction) {
  var dispatch = DevToolsAPI.dispatchMessage;
  var messages = [];
  DevToolsAPI.dispatchMessage = message => {
    if (!messages.length) {
      setTimeout(() => {
        for (var message of messages.splice(0))
          dispatch(message);
      }, 0);
    }
    messages.push(message);
  };
  return testRunner => {
    testRunner.log = console.log;
    testRunner.completeTest = () => console.log('Test completed');
    window.test = () => testFunction(testRunner);
  };
};

DevToolsAPI._fetch = function(url) {
  return new Promise(fulfill => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onreadystatechange = e => {
      if (xhr.readyState !== XMLHttpRequest.DONE)
        return;
      if ([0, 200, 304].indexOf(xhr.status) === -1)  // Testing harness file:/// results in 0.
        DevToolsAPI._die(`${xhr.status} while fetching ${url}`, new Error());
      else
        fulfill(e.target.response);
    };
    xhr.send(null);
  });
};

window.testRunner.dumpAsText();
window.testRunner.waitUntilDone();
window.testRunner.setCanOpenWindows(true);

window.addEventListener('load', () => {
  var testScriptURL = window.location.search.substring(1);
  var baseURL = testScriptURL.substring(0, testScriptURL.lastIndexOf('/') + 1);
  DevToolsAPI._fetch(testScriptURL).then(testScript => {
    var testRunner = new TestRunner(baseURL, DevToolsAPI._log, DevToolsAPI._completeTest, DevToolsAPI._fetch);
    var testFunction = eval(`${testScript}\n//# sourceURL=${testScriptURL}`);
    return testFunction(testRunner);
  }).catch(reason => {
    DevToolsAPI._log(`Error while executing test script: ${reason}\n${reason.stack}`);
    DevToolsAPI._completeTest();
  });
}, false);

window['onerror'] = (message, source, lineno, colno, error) => {
  DevToolsAPI._log(`${error}\n${error.stack}`);
  DevToolsAPI._completeTest();
};

window.addEventListener('unhandledrejection', e => {
  DevToolsAPI._log(`Promise rejection: ${e.reason}\n${e.reason ? e.reason.stack : ''}`);
  DevToolsAPI._completeTest();
}, false);
