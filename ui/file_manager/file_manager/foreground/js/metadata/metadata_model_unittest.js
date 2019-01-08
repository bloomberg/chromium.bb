// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestMetadataProvider
 * @constructor
 * @extends {MetadataProvider}
 */
function TestMetadataProvider() {
  MetadataProvider.call(this, ['property', 'propertyA', 'propertyB']);
  this.requestCount = 0;
}

TestMetadataProvider.prototype.__proto__ = MetadataProvider.prototype;

TestMetadataProvider.prototype.get = function(requests) {
  this.requestCount++;
  return Promise.resolve(requests.map(function(request) {
    var entry = request.entry;
    var names = request.names;
    var result = {};
    for (var i = 0; i < names.length; i++) {
      result[names[i]] = entry.toURL() + ':' + names[i];
    }
    return result;
  }));
};

/**
 * TestEmptyMetadataProvider
 * @constructor
 * @extends {MetadataProvider}
 */
function TestEmptyMetadataProvider() {
  MetadataProvider.call(this, ['property']);
}

TestEmptyMetadataProvider.prototype.__proto__ = MetadataProvider.prototype;

TestEmptyMetadataProvider.prototype.get = function(requests) {
  return Promise.resolve(requests.map(function() {
    return {};
  }));
};

/**
 * ManualTestMetadataProvider
 * @constructor
 * @extends {MetadataProvider}
 */
function ManualTestMetadataProvider() {
  MetadataProvider.call(this, ['propertyA', 'propertyB', 'propertyC']);
  this.callback = [];
}

ManualTestMetadataProvider.prototype.__proto__ = MetadataProvider.prototype;

ManualTestMetadataProvider.prototype.get = function(requests) {
  return new Promise(function(fulfill) {
    this.callback.push(fulfill);
  }.bind(this));
};

/** @type {!Entry} */
var entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
});

/** @type {!Entry} */
var entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
});

/**
 * Returns a property of a Metadata result object.
 * @param {Object} result Metadata result
 * @param {string} property Property name to return.
 * @return {string}
 */
function getProperty(result, property) {
  if (!result) {
    throw new Error('Fail: Metadata result is undefined');
  }
  return result[property];
}

function testMetadataModelBasic(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['property']).then(function(results) {
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:property', getProperty(results[0], 'property'));
        assertEquals(
            'filesystem://B:property', getProperty(results[1], 'property'));
      }),
      callback);
}

function testMetadataModelRequestForCachedProperty(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['property'])
          .then(function() {
            // All the results should be cached here.
            return model.get([entryA, entryB], ['property']);
          })
          .then(function(results) {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            assertEquals(
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

function testMetadataModelRequestForCachedAndNonCachedProperty(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA, entryB], ['propertyA'])
          .then(function() {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            // propertyB has not been cached here.
            return model.get([entryA, entryB], ['propertyA', 'propertyB']);
          })
          .then(function(results) {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(2, provider.requestCount);
            assertEquals(
                'filesystem://A:propertyA',
                getProperty(results[0], 'propertyA'));
            assertEquals(
                'filesystem://A:propertyB',
                getProperty(results[0], 'propertyB'));
            assertEquals(
                'filesystem://B:propertyA',
                getProperty(results[1], 'propertyA'));
            assertEquals(
                'filesystem://B:propertyB',
                getProperty(results[1], 'propertyB'));
          }),
      callback);
}

function testMetadataModelRequestForCachedAndNonCachedEntry(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  reportPromise(
      model.get([entryA], ['property'])
          .then(function() {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(1, provider.requestCount);
            // entryB has not been cached here.
            return model.get([entryA, entryB], ['property']);
          })
          .then(function(results) {
            provider =
                /** @type {!TestMetadataProvider} */ (model.getProvider());
            assertEquals(2, provider.requestCount);
            assertEquals(
                'filesystem://A:property', getProperty(results[0], 'property'));
            assertEquals(
                'filesystem://B:property', getProperty(results[1], 'property'));
          }),
      callback);
}

function testMetadataModelRequestBeforeCompletingPreviousRequest(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  model.get([entryA], ['property']);
  provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
  assertEquals(1, provider.requestCount);

  // The result of first call has not been fetched yet.
  reportPromise(
      model.get([entryA], ['property']).then(function(results) {
        provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
        assertEquals(1, provider.requestCount);
        assertEquals(
            'filesystem://A:property', getProperty(results[0], 'property'));
      }),
      callback);
}

function testMetadataModelNotUpdateCachedResultAfterRequest(callback) {
  var provider = new ManualTestMetadataProvider();
  var model = new MetadataModel(provider);

  var promise = model.get([entryA], ['propertyA']);
  provider = /** @type {!ManualTestMetadataProvider} */ (model.getProvider());
  provider.callback[0]([{propertyA: 'valueA1'}]);

  reportPromise(promise.then(function() {
    // 'propertyA' is cached here.
    var promise1 = model.get([entryA], ['propertyA', 'propertyB']);
    var promise2 = model.get([entryA], ['propertyC']);
    // Returns propertyC.
    provider = /** @type {!ManualTestMetadataProvider} */ (model.getProvider());
    provider.callback[2]([{propertyA: 'valueA2', propertyC: 'valueC'}]);
    provider.callback[1]([{propertyB: 'valueB'}]);
    return Promise.all([promise1, promise2]);
  }).then(function(results) {
    // The result should be cached value at the time when get was called.
    assertEquals('valueA1', getProperty(results[0][0], 'propertyA'));
    assertEquals('valueB', getProperty(results[0][0], 'propertyB'));
    assertEquals('valueC', getProperty(results[1][0], 'propertyC'));
  }), callback);
}

function testMetadataModelGetCache(callback) {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  var promise = model.get([entryA], ['property']);
  var cache = model.getCache([entryA], ['property']);
  assertEquals(null, getProperty(cache[0], 'property'));

  reportPromise(promise.then(function() {
    var cache = model.getCache([entryA], ['property']);
    provider = /** @type {!TestMetadataProvider} */ (model.getProvider());
    assertEquals(1, provider.requestCount);
    assertEquals('filesystem://A:property', getProperty(cache[0], 'property'));
  }), callback);
}

function testMetadataModelUnknownProperty() {
  var provider = new TestMetadataProvider();
  var model = new MetadataModel(provider);

  assertThrows(function() {
    model.get([entryA], ['unknown']);
  });
}

function testMetadataModelEmptyResult(callback) {
  var provider = new TestEmptyMetadataProvider();
  var model = new MetadataModel(provider);

  // getImpl returns empty result.
  reportPromise(
      model.get([entryA], ['property']).then(function(results) {
        assertEquals(undefined, getProperty(results[0], 'property'));
      }),
      callback);
}
