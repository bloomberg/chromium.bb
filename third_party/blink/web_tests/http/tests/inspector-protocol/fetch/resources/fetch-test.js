(function() {

class FetchHandler {
  constructor(testRunner, protocol) {
    this._testRunner = testRunner;
    this._protocol = protocol;
    this._callback = null;
  }

  _handle(params) {
    this._callback(params);
  }

  matched() {
    return new Promise(fulfill => this._callback = fulfill);
  }

  async continueRequest(params) {
    const request = await this.matched();
    return this._protocol.Fetch.continueRequest(
        Object.assign(params || {}, {requestId: request.requestId}))
            .then(result => this._handleError(result));
  }

  async fail(params) {
    const request = await this.matched();
    return this._protocol.Fetch.failRequest(
        Object.assign(params, {requestId: request.requestId}))
            .then(result => this._handleError(result));
  }

  async fulfill(params) {
    const request = await this.matched();
    return this._protocol.Fetch.fulfillRequest(
        Object.assign(params, {requestId: request.requestId}))
            .then(result => this._handleError(result));
  }

  _handleError(result) {
    if (result.error)
      this._testRunner.log(`Got error: ${result.error.message}`);
  }
};

class FetchHelper {
  constructor(testRunner, pageProtocol) {
    this._handlers = [];
    this._onceHandlers = [];
    this._testRunner = testRunner;
    this._protocol = testRunner.browserP();
    this._pageProtocol = pageProtocol;
    this._protocol.Fetch.onRequestPaused(event => {
      this._logRequest(event);
      const handler = this._findHandler(event);
      if (handler)
        handler._handle(event);
    });
  }

  enable(patterns) {
    this._protocol.Fetch.enable({patterns});
    this.onceRequest().continueRequest();
    return this._pageProtocol.Page.reload();
  }

  onRequest(pattern) {
    const handler = new FetchHandler(this._testRunner, this._protocol);
    this._handlers.push({pattern, handler});
    return handler;
  }

  onceRequest(pattern) {
    const handler = new FetchHandler(this._testRunner, this._protocol);
    this._onceHandlers.push({pattern, handler});
    return handler;
  }

  _logRequest(event) {
    const params = event.params;
    const response = event.responseErrorReason || event.responseStatusCode;
    const response_text = response ? 'Response' : 'Request';
    this._testRunner.log(`${response_text} to ${params.url}, type: ${params.resourceType}`);
  }

  _findHandler(event) {
    const params = event.params;
    const url = params.request.url;
    let entry;
    let index = FetchHelper._findHandlerIndex(this._onceHandlers, url);
    if (index >= 0) {
      [entry] = this._onceHandlers.splice(index, 1);
    } else {
      entry = FetchHelper._findHandlerIndex(this._handlers, url);
      if (index >= 0)
        handler = this._handlers[index];
    }
    if (entry)
      entry.handler._handle(params);
  }

  static _findHandlerIndex(arr, url) {
    return arr.findIndex(item => !item.pattern || item.pattern.test(url));
  }
};

return FetchHelper;
})()
