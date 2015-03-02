// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestMetadataProvider() {
  NewMetadataProvider.call(this, ['property', 'propertyA', 'propertyB']);
  this.requestCount = 0;
}

TestMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

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

function TestEmptyMetadataProvider() {
  NewMetadataProvider.call(this, ['property']);
}

TestEmptyMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

TestEmptyMetadataProvider.prototype.get = function(requests) {
  return Promise.resolve(requests.map(function() {
    return {};
  }));
};

function ManualTestMetadataProvider() {
  NewMetadataProvider.call(
      this, ['propertyA', 'propertyB', 'propertyC']);
  this.callback = [];
}

ManualTestMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

ManualTestMetadataProvider.prototype.get = function(requests) {
  return new Promise(function(fulfill) {
    this.callback.push(fulfill);
  }.bind(this));
};

var entryA = {
  toURL: function() { return "filesystem://A"; }
};

var entryB = {
  toURL: function() { return "filesystem://B"; }
};

function testNewMetadataProviderBasic(callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  reportPromise(provider.get([entryA, entryB], ['property']).then(
      function(results) {
        assertEquals(1, provider.getRawProvider().requestCount);
        assertEquals('filesystem://A:property', results[0].property);
        assertEquals('filesystem://B:property', results[1].property);
      }), callback);
}

function testNewMetadataProviderRequestForCachedProperty(callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  reportPromise(provider.get([entryA, entryB], ['property']).then(
      function() {
        // All the result should be cached here.
        return provider.get([entryA, entryB], ['property']);
      }).then(function(results) {
        assertEquals(1, provider.getRawProvider().requestCount);
        assertEquals('filesystem://A:property', results[0].property);
        assertEquals('filesystem://B:property', results[1].property);
      }), callback);
}

function testNewMetadataProviderRequestForCachedAndNonCachedProperty(callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  reportPromise(provider.get([entryA, entryB], ['propertyA']).then(
      function() {
        assertEquals(1, provider.getRawProvider().requestCount);
        // propertyB has not been cached here.
        return provider.get([entryA, entryB], ['propertyA', 'propertyB']);
      }).then(function(results) {
        assertEquals(2, provider.getRawProvider().requestCount);
        assertEquals('filesystem://A:propertyA', results[0].propertyA);
        assertEquals('filesystem://A:propertyB', results[0].propertyB);
        assertEquals('filesystem://B:propertyA', results[1].propertyA);
        assertEquals('filesystem://B:propertyB', results[1].propertyB);
      }), callback);
}

function testNewMetadataProviderRequestForCachedAndNonCachedEntry(callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  reportPromise(provider.get([entryA], ['property']).then(
      function() {
        assertEquals(1, provider.getRawProvider().requestCount);
        // entryB has not been cached here.
        return provider.get([entryA, entryB], ['property']);
      }).then(function(results) {
        assertEquals(2, provider.getRawProvider().requestCount);
        assertEquals('filesystem://A:property', results[0].property);
        assertEquals('filesystem://B:property', results[1].property);
      }), callback);
}

function testNewMetadataProviderRequestBeforeCompletingPreviousRequest(
    callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  provider.get([entryA], ['property']);
  assertEquals(1, provider.getRawProvider().requestCount);
  // The result of first call has not been fetched yet.
  reportPromise(provider.get([entryA], ['property']).then(
      function(results) {
        assertEquals(1, provider.getRawProvider().requestCount);
        assertEquals('filesystem://A:property', results[0].property);
      }), callback);
}

function testNewMetadataProviderNotUpdateCachedResultAfterRequest(
    callback) {
  var provider = new CachedMetadataProvider(new ManualTestMetadataProvider());
  var promise = provider.get([entryA], ['propertyA']);
  provider.getRawProvider().callback[0]([{propertyA: 'valueA1'}]);
  reportPromise(promise.then(function() {
    // 'propertyA' is cached here.
    var promise1 = provider.get([entryA], ['propertyA', 'propertyB']);
    var promise2 = provider.get([entryA], ['propertyC']);
    // Returns propertyC.
    provider.getRawProvider().callback[2](
        [{propertyA: 'valueA2', propertyC: 'valueC'}]);
    provider.getRawProvider().callback[1]([{propertyB: 'valueB'}]);
    return Promise.all([promise1, promise2]);
  }).then(function(results) {
    // The result should be cached value at the time when get was called.
    assertEquals('valueA1', results[0][0].propertyA);
    assertEquals('valueB', results[0][0].propertyB);
    assertEquals('valueC', results[1][0].propertyC);
  }), callback);
}

function testNewMetadataProviderGetCache(callback) {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  var promise = provider.get([entryA], ['property']);
  var cache = provider.getCache([entryA], ['property']);
  assertEquals(null, cache[0].property);
  reportPromise(promise.then(function() {
    var cache = provider.getCache([entryA], ['property']);
    assertEquals(1, provider.getRawProvider().requestCount);
    assertEquals('filesystem://A:property', cache[0].property);
  }), callback);
}

function testNewMetadataProviderUnknownProperty() {
  var provider = new CachedMetadataProvider(new TestMetadataProvider());
  assertThrows(function() {
    provider.get([entryA], ['unknown']);
  });
}

function testNewMetadataProviderEmptyResult(callback) {
  var provider = new CachedMetadataProvider(new TestEmptyMetadataProvider());
  // getImpl returns empty result.
  reportPromise(provider.get([entryA], ['property']).then(function(results) {
    assertEquals(undefined, results[0].property);
  }), callback);
}
