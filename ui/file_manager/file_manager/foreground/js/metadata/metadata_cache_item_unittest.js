// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testMetadataCacheItemBasic() {
  var item = new MetadataCacheItem();
  var loadRequested = item.startRequests(1, ['propertyA']);
  assertEquals(1, loadRequested.length);
  assertEquals('propertyA', loadRequested[0]);

  assertTrue(item.storeProperties(1, {propertyA: 'value'}));

  var result = item.get(['propertyA']);
  assertEquals('value', result.propertyA);
}

function testMetadataCacheItemAvoidDoubleLoad() {
  var item = new MetadataCacheItem();
  item.startRequests(1, ['propertyA']);
  var loadRequested = item.startRequests(2, ['propertyA']);
  assertEquals(0, loadRequested.length);

  assertTrue(item.storeProperties(1, {propertyA: 'value'}));

  var result = item.get(['propertyA']);
  assertEquals('value', result.propertyA);
}

function testMetadataCacheItemInvalidate() {
  var item = new MetadataCacheItem();
  item.startRequests(1, ['propertyA']);
  item.invalidate(2);
  assertFalse(item.storeProperties(1, {propertyA: 'value'}));

  var loadRequested = item.startRequests(3, ['propertyA']);
  assertEquals(1, loadRequested.length);
}

function testMetadataCacheItemStoreInReverseOrder() {
  var item = new MetadataCacheItem();
  item.startRequests(1, ['propertyA']);
  item.startRequests(2, ['propertyA']);

  assertTrue(item.storeProperties(2, {propertyA: 'value2'}));
  assertFalse(item.storeProperties(1, {propertyA: 'value1'}));

  var result = item.get(['propertyA']);
  assertEquals('value2', result.propertyA);
}
