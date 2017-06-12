// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
SDK.ServiceWorkerCacheModel = class extends SDK.SDKModel {
  /**
   * Invariant: This model can only be constructed on a ServiceWorker target.
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);

    /** @type {!Map<string, !SDK.ServiceWorkerCacheModel.Cache>} */
    this._caches = new Map();

    this._agent = target.cacheStorageAgent();

    this._securityOriginManager = target.model(SDK.SecurityOriginManager);

    /** @type {boolean} */
    this._enabled = false;
  }

  enable() {
    if (this._enabled)
      return;

    this._securityOriginManager.addEventListener(
        SDK.SecurityOriginManager.Events.SecurityOriginAdded, this._securityOriginAdded, this);
    this._securityOriginManager.addEventListener(
        SDK.SecurityOriginManager.Events.SecurityOriginRemoved, this._securityOriginRemoved, this);

    for (var securityOrigin of this._securityOriginManager.securityOrigins())
      this._addOrigin(securityOrigin);
    this._enabled = true;
  }

  /**
   * @param {string} origin
   */
  clearForOrigin(origin) {
    this._removeOrigin(origin);
    this._addOrigin(origin);
  }

  refreshCacheNames() {
    for (var cache of this._caches.values())
      this._cacheRemoved(cache);
    this._caches.clear();
    var securityOrigins = this._securityOriginManager.securityOrigins();
    for (var securityOrigin of securityOrigins)
      this._loadCacheNames(securityOrigin);
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   */
  async deleteCache(cache) {
    var response = await this._agent.invoke_deleteCache({cacheId: cache.cacheId});
    if (response[Protocol.Error]) {
      console.error(`ServiceWorkerCacheAgent error deleting cache ${cache.toString()}: ${response[Protocol.Error]}`);
      return;
    }
    this._caches.delete(cache.cacheId);
    this._cacheRemoved(cache);
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   * @param {string} request
   * @return {!Promise}
   */
  async deleteCacheEntry(cache, request) {
    var response = await this._agent.invoke_deleteEntry({cacheId: cache.cacheId, request});
    if (!response[Protocol.Error])
      return;
    Common.console.error(Common.UIString(
        'ServiceWorkerCacheAgent error deleting cache entry %s in cache: %s', cache.toString(),
        response[Protocol.Error]));
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   * @param {number} skipCount
   * @param {number} pageSize
   * @param {function(!Array.<!SDK.ServiceWorkerCacheModel.Entry>, boolean)} callback
   */
  loadCacheData(cache, skipCount, pageSize, callback) {
    this._requestEntries(cache, skipCount, pageSize, callback);
  }

  /**
   * @return {!Array.<!SDK.ServiceWorkerCacheModel.Cache>}
   */
  caches() {
    var caches = new Array();
    for (var cache of this._caches.values())
      caches.push(cache);
    return caches;
  }

  /**
   * @override
   */
  dispose() {
    for (var cache of this._caches.values())
      this._cacheRemoved(cache);
    this._caches.clear();
    if (this._enabled) {
      this._securityOriginManager.removeEventListener(
          SDK.SecurityOriginManager.Events.SecurityOriginAdded, this._securityOriginAdded, this);
      this._securityOriginManager.removeEventListener(
          SDK.SecurityOriginManager.Events.SecurityOriginRemoved, this._securityOriginRemoved, this);
    }
  }

  _addOrigin(securityOrigin) {
    this._loadCacheNames(securityOrigin);
  }

  /**
   * @param {string} securityOrigin
   */
  _removeOrigin(securityOrigin) {
    for (var opaqueId of this._caches.keys()) {
      var cache = this._caches.get(opaqueId);
      if (cache.securityOrigin === securityOrigin) {
        this._caches.delete(opaqueId);
        this._cacheRemoved(cache);
      }
    }
  }

  /**
   * @param {string} securityOrigin
   */
  async _loadCacheNames(securityOrigin) {
    var caches = await this._agent.requestCacheNames(securityOrigin);
    if (!caches)
      return;
    this._updateCacheNames(securityOrigin, caches);
  }

  /**
   * @param {string} securityOrigin
   * @param {!Array} cachesJson
   */
  _updateCacheNames(securityOrigin, cachesJson) {
    /**
     * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
     * @this {SDK.ServiceWorkerCacheModel}
     */
    function deleteAndSaveOldCaches(cache) {
      if (cache.securityOrigin === securityOrigin && !updatingCachesIds.has(cache.cacheId)) {
        oldCaches.set(cache.cacheId, cache);
        this._caches.delete(cache.cacheId);
      }
    }

    /** @type {!Set<string>} */
    var updatingCachesIds = new Set();
    /** @type {!Map<string, !SDK.ServiceWorkerCacheModel.Cache>} */
    var newCaches = new Map();
    /** @type {!Map<string, !SDK.ServiceWorkerCacheModel.Cache>} */
    var oldCaches = new Map();

    for (var cacheJson of cachesJson) {
      var cache =
          new SDK.ServiceWorkerCacheModel.Cache(cacheJson.securityOrigin, cacheJson.cacheName, cacheJson.cacheId);
      updatingCachesIds.add(cache.cacheId);
      if (this._caches.has(cache.cacheId))
        continue;
      newCaches.set(cache.cacheId, cache);
      this._caches.set(cache.cacheId, cache);
    }
    this._caches.forEach(deleteAndSaveOldCaches, this);
    newCaches.forEach(this._cacheAdded, this);
    oldCaches.forEach(this._cacheRemoved, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _securityOriginAdded(event) {
    var securityOrigin = /** @type {string} */ (event.data);
    this._addOrigin(securityOrigin);
  }

  /**
   * @param {!Common.Event} event
   */
  _securityOriginRemoved(event) {
    var securityOrigin = /** @type {string} */ (event.data);
    this._removeOrigin(securityOrigin);
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   */
  _cacheAdded(cache) {
    this.dispatchEventToListeners(SDK.ServiceWorkerCacheModel.Events.CacheAdded, {model: this, cache: cache});
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   */
  _cacheRemoved(cache) {
    this.dispatchEventToListeners(SDK.ServiceWorkerCacheModel.Events.CacheRemoved, {model: this, cache: cache});
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   * @param {number} skipCount
   * @param {number} pageSize
   * @param {function(!Array<!SDK.ServiceWorkerCacheModel.Entry>, boolean)} callback
   */
  async _requestEntries(cache, skipCount, pageSize, callback) {
    var response = await this._agent.invoke_requestEntries({cacheId: cache.cacheId, skipCount, pageSize});
    if (response[Protocol.Error]) {
      console.error('ServiceWorkerCacheAgent error while requesting entries: ', response[Protocol.Error]);
      return;
    }
    var entries = response.cacheDataEntries.map(
        dataEntry => new SDK.ServiceWorkerCacheModel.Entry(
            dataEntry.request, dataEntry.response, new Date(dataEntry.responseTime * 1000).toLocaleString()));
    callback(entries, response.hasMore);
  }
};

SDK.SDKModel.register(SDK.ServiceWorkerCacheModel, SDK.Target.Capability.Browser, false);

/** @enum {symbol} */
SDK.ServiceWorkerCacheModel.Events = {
  CacheAdded: Symbol('CacheAdded'),
  CacheRemoved: Symbol('CacheRemoved')
};

/**
 * @unrestricted
 */
SDK.ServiceWorkerCacheModel.Entry = class {
  /**
   * @param {string} request
   * @param {string} response
   * @param {string} responseTime
   */
  constructor(request, response, responseTime) {
    this.request = request;
    this.response = response;
    this.responseTime = responseTime;
  }
};

/**
 * @unrestricted
 */
SDK.ServiceWorkerCacheModel.Cache = class {
  /**
   * @param {string} securityOrigin
   * @param {string} cacheName
   * @param {string} cacheId
   */
  constructor(securityOrigin, cacheName, cacheId) {
    this.securityOrigin = securityOrigin;
    this.cacheName = cacheName;
    this.cacheId = cacheId;
  }

  /**
   * @param {!SDK.ServiceWorkerCacheModel.Cache} cache
   * @return {boolean}
   */
  equals(cache) {
    return this.cacheId === cache.cacheId;
  }

  /**
   * @override
   * @return {string}
   */
  toString() {
    return this.securityOrigin + this.cacheName;
  }
};
