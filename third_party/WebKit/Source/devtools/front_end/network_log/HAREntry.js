/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// See http://www.softwareishard.com/blog/har-12-spec/
// for HAR specification.

// FIXME: Some fields are not yet supported due to back-end limitations.
// See https://bugs.webkit.org/show_bug.cgi?id=58127 for details.

/**
 * @unrestricted
 */
NetworkLog.HAREntry = class {
  /**
   * @param {!SDK.NetworkRequest} request
   */
  constructor(request) {
    this._request = request;
  }

  /**
   * @param {number} time
   * @return {number}
   */
  static _toMilliseconds(time) {
    return time === -1 ? -1 : time * 1000;
  }

  /**
   * @return {!Object}
   */
  build() {
    var ipAddress = this._request.remoteAddress();
    var portPositionInString = ipAddress.lastIndexOf(':');
    if (portPositionInString !== -1)
      ipAddress = ipAddress.substr(0, portPositionInString);

    var timings = this._buildTimings();
    var time = 0;
    // "ssl" is included in the connect field, so do not double count it.
    for (var t of [timings.blocked, timings.dns, timings.connect, timings.send, timings.wait, timings.receive])
      time += Math.max(t, 0);

    var entry = {
      startedDateTime: NetworkLog.HARLog.pseudoWallTime(this._request, this._request.issueTime()),
      time: time,
      request: this._buildRequest(),
      response: this._buildResponse(),
      cache: {},  // Not supported yet.
      timings: timings,
      // IPv6 address should not have square brackets per (https://tools.ietf.org/html/rfc2373#section-2.2).
      serverIPAddress: ipAddress.replace(/\[\]/g, '')
    };

    // Chrome specific.
    if (this._request.cached())
      entry._fromCache = this._request.cachedInMemory() ? 'memory' : 'disk';

    if (this._request.connectionId !== '0')
      entry.connection = this._request.connectionId;
    var page = NetworkLog.PageLoad.forRequest(this._request);
    if (page)
      entry.pageref = 'page_' + page.id;
    return entry;
  }

  /**
   * @return {!Object}
   */
  _buildRequest() {
    var headersText = this._request.requestHeadersText();
    var res = {
      method: this._request.requestMethod,
      url: this._buildRequestURL(this._request.url()),
      httpVersion: this._request.requestHttpVersion(),
      headers: this._request.requestHeaders(),
      queryString: this._buildParameters(this._request.queryParameters || []),
      cookies: this._buildCookies(this._request.requestCookies || []),
      headersSize: headersText ? headersText.length : -1,
      bodySize: this.requestBodySize
    };
    if (this._request.requestFormData)
      res.postData = this._buildPostData();

    return res;
  }

  /**
   * @return {!Object}
   */
  _buildResponse() {
    var headersText = this._request.responseHeadersText;
    return {
      status: this._request.statusCode,
      statusText: this._request.statusText,
      httpVersion: this._request.responseHttpVersion(),
      headers: this._request.responseHeaders,
      cookies: this._buildCookies(this._request.responseCookies || []),
      content: this._buildContent(),
      redirectURL: this._request.responseHeaderValue('Location') || '',
      headersSize: headersText ? headersText.length : -1,
      bodySize: this.responseBodySize,
      _transferSize: this._request.transferSize,
      _error: this._request.localizedFailDescription
    };
  }

  /**
   * @return {!Object}
   */
  _buildContent() {
    var content = {
      size: this._request.resourceSize,
      mimeType: this._request.mimeType || 'x-unknown',
      // text: this._request.content // TODO: pull out into a boolean flag, as content can be huge (and needs to be requested with an async call)
    };
    var compression = this.responseCompression;
    if (typeof compression === 'number')
      content.compression = compression;
    return content;
  }

  /**
   * @return {!NetworkLog.HAREntry.Timing}
   */
  _buildTimings() {
    // Order of events: request_start = 0, [proxy], [dns], [connect [ssl]], [send], duration
    var timing = this._request.timing;
    var issueTime = this._request.issueTime();
    var startTime = this._request.startTime;

    var result = {blocked: -1, dns: -1, ssl: -1, connect: -1, send: 0, wait: 0, receive: 0, _blocked_queueing: -1};

    var queuedTime = (issueTime < startTime) ? startTime - issueTime : -1;
    result.blocked = queuedTime;
    result._blocked_queueing = NetworkLog.HAREntry._toMilliseconds(queuedTime);

    var highestTime = 0;
    if (timing) {
      // "blocked" here represents both queued + blocked/stalled + proxy (ie: anything before request was started).
      // We pick the better of when the network request start was reported and pref timing.
      var blockedStart = leastNonNegative([timing.dnsStart, timing.connectStart, timing.sendStart]);
      if (blockedStart !== Infinity)
        result.blocked += blockedStart;

      // Proxy is part of blocked but sometimes (like quic) blocked is -1 but has proxy timings.
      if (timing.proxyEnd !== -1)
        result._blocked_proxy = timing.proxyEnd - timing.proxyStart;
      if (result._blocked_proxy && result._blocked_proxy > result.blocked)
        result.blocked = result._blocked_proxy;

      var dnsStart = timing.dnsEnd >= 0 ? blockedStart : 0;
      var dnsEnd = timing.dnsEnd >= 0 ? timing.dnsEnd : -1;
      result.dns = dnsEnd - dnsStart;

      // SSL timing is included in connection timing.
      var sslStart = timing.sslEnd > 0 ? timing.sslStart : 0;
      var sslEnd = timing.sslEnd > 0 ? timing.sslEnd : -1;
      result.ssl = sslEnd - sslStart;

      var connectStart = timing.connectEnd >= 0 ? leastNonNegative([dnsEnd, blockedStart]) : 0;
      var connectEnd = timing.connectEnd >= 0 ? timing.connectEnd : -1;
      result.connect = connectEnd - connectStart;

      // Send should not be -1 for legacy reasons even if it is served from cache.
      var sendStart = timing.sendEnd >= 0 ? Math.max(connectEnd, dnsEnd, blockedStart) : 0;
      var sendEnd = timing.sendEnd >= 0 ? timing.sendEnd : 0;
      result.send = sendEnd - sendStart;
      // Quic sometimes says that sendStart is before connectionEnd (see: crbug.com/740792)
      if (result.send < 0)
        result.send = 0;
      highestTime = Math.max(sendEnd, connectEnd, sslEnd, dnsEnd, blockedStart, 0);
    } else if (this._request.responseReceivedTime === -1) {
      // Means that we don't have any more details after blocked, so attribute all to blocked.
      result.blocked = this._request.endTime - issueTime;
      return result;
    }

    var requestTime = timing ? timing.requestTime : startTime;
    var waitStart = highestTime;
    var waitEnd = NetworkLog.HAREntry._toMilliseconds(this._request.responseReceivedTime - requestTime);
    result.wait = waitEnd - waitStart;

    var receiveStart = waitEnd;
    var receiveEnd = NetworkLog.HAREntry._toMilliseconds(this._request.endTime - issueTime);
    result.receive = Math.max(receiveEnd - receiveStart, 0);

    return result;

    /**
     * @param {!Array<number>} values
     * @return {number}
     */
    function leastNonNegative(values) {
      return values.reduce((best, value) => (value >= 0 && value < best) ? value : best, Infinity);
    }
  }

  /**
   * @return {!Object}
   */
  _buildPostData() {
    var res = {mimeType: this._request.requestContentType(), text: this._request.requestFormData};
    if (this._request.formParameters)
      res.params = this._buildParameters(this._request.formParameters);
    return res;
  }

  /**
   * @param {!Array.<!Object>} parameters
   * @return {!Array.<!Object>}
   */
  _buildParameters(parameters) {
    return parameters.slice();
  }

  /**
   * @param {string} url
   * @return {string}
   */
  _buildRequestURL(url) {
    return url.split('#', 2)[0];
  }

  /**
   * @param {!Array.<!SDK.Cookie>} cookies
   * @return {!Array.<!Object>}
   */
  _buildCookies(cookies) {
    return cookies.map(this._buildCookie.bind(this));
  }

  /**
   * @param {!SDK.Cookie} cookie
   * @return {!Object}
   */
  _buildCookie(cookie) {
    var c = {
      name: cookie.name(),
      value: cookie.value(),
      path: cookie.path(),
      domain: cookie.domain(),
      expires: cookie.expiresDate(NetworkLog.HARLog.pseudoWallTime(this._request, this._request.startTime)),
      httpOnly: cookie.httpOnly(),
      secure: cookie.secure()
    };
    if (cookie.sameSite())
      c.sameSite = cookie.sameSite();
    return c;
  }

  /**
   * @return {number}
   */
  get requestBodySize() {
    return !this._request.requestFormData ? 0 : this._request.requestFormData.length;
  }

  /**
   * @return {number}
   */
  get responseBodySize() {
    if (this._request.cached() || this._request.statusCode === 304)
      return 0;
    if (!this._request.responseHeadersText)
      return -1;
    return this._request.transferSize - this._request.responseHeadersText.length;
  }

  /**
   * @return {number|undefined}
   */
  get responseCompression() {
    if (this._request.cached() || this._request.statusCode === 304 || this._request.statusCode === 206)
      return;
    if (!this._request.responseHeadersText)
      return;
    return this._request.resourceSize - this.responseBodySize;
  }
};

/** @typedef {!{
  blocked: number,
  dns: number,
  ssl: number,
  connect: number,
  send: number,
  wait: number,
  receive: number,
  _blocked_queueing: number,
  _blocked_proxy: (number|undefined)
}} */
NetworkLog.HAREntry.Timing;


/**
 * @unrestricted
 */
NetworkLog.HARLog = class {
  /**
   * @param {!Array.<!SDK.NetworkRequest>} requests
   */
  constructor(requests) {
    this._requests = requests;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @param {number} monotonicTime
   * @return {string}
   */
  static pseudoWallTime(request, monotonicTime) {
    return new Date(request.pseudoWallTime(monotonicTime) * 1000).toJSON();
  }

  /**
   * @return {!Object}
   */
  build() {
    return {
      version: '1.2',
      creator: this._creator(),
      pages: this._buildPages(),
      entries: this._requests.map(this._convertResource.bind(this))
    };
  }

  _creator() {
    var webKitVersion = /AppleWebKit\/([^ ]+)/.exec(window.navigator.userAgent);

    return {name: 'WebInspector', version: webKitVersion ? webKitVersion[1] : 'n/a'};
  }

  /**
   * @return {!Array.<!Object>}
   */
  _buildPages() {
    var seenIdentifiers = {};
    var pages = [];
    for (var i = 0; i < this._requests.length; ++i) {
      var request = this._requests[i];
      var page = NetworkLog.PageLoad.forRequest(request);
      if (!page || seenIdentifiers[page.id])
        continue;
      seenIdentifiers[page.id] = true;
      pages.push(this._convertPage(page, request));
    }
    return pages;
  }

  /**
   * @param {!NetworkLog.PageLoad} page
   * @param {!SDK.NetworkRequest} request
   * @return {!Object}
   */
  _convertPage(page, request) {
    return {
      startedDateTime: NetworkLog.HARLog.pseudoWallTime(request, page.startTime),
      id: 'page_' + page.id,
      title: page.url,  // We don't have actual page title here. URL is probably better than nothing.
      pageTimings: {
        onContentLoad: this._pageEventTime(page, page.contentLoadTime),
        onLoad: this._pageEventTime(page, page.loadTime)
      }
    };
  }

  /**
   * @param {!SDK.NetworkRequest} request
   * @return {!Object}
   */
  _convertResource(request) {
    return (new NetworkLog.HAREntry(request)).build();
  }

  /**
   * @param {!NetworkLog.PageLoad} page
   * @param {number} time
   * @return {number}
   */
  _pageEventTime(page, time) {
    var startTime = page.startTime;
    if (time === -1 || startTime === -1)
      return -1;
    return NetworkLog.HAREntry._toMilliseconds(time - startTime);
  }
};
