// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!MetadataItem}
 */
let metadataA = new MetadataItem();
metadataA.contentMimeType = 'value';


function testMetadataCacheItemBasic() {
  var item = new MetadataCacheItem();
  var loadRequested = item.createRequests(['contentMimeType']);
  assertEquals(1, loadRequested.length);
  assertEquals('contentMimeType', loadRequested[0]);

  item.startRequests(1, loadRequested);
  assertTrue(item.storeProperties(1, metadataA));

  var result = item.get(['contentMimeType']);
  assertEquals('value', result.contentMimeType);
}

function testMetadataCacheItemAvoidDoubleLoad() {
  var item = new MetadataCacheItem();
  item.startRequests(1, ['contentMimeType']);
  var loadRequested = item.createRequests(['contentMimeType']);
  assertEquals(0, loadRequested.length);

  item.startRequests(2, loadRequested);
  assertTrue(item.storeProperties(1, metadataA));

  var result = item.get(['contentMimeType']);
  assertEquals('value', result.contentMimeType);
}

function testMetadataCacheItemInvalidate() {
  var item = new MetadataCacheItem();
  item.startRequests(1, item.createRequests(['contentMimeType']));
  item.invalidate(2);
  assertFalse(item.storeProperties(1, metadataA));

  var loadRequested = item.createRequests(['contentMimeType']);
  assertEquals(1, loadRequested.length);
}

function testMetadataCacheItemStoreInReverseOrder() {
  var item = new MetadataCacheItem();
  item.startRequests(1, item.createRequests(['contentMimeType']));
  item.startRequests(2, item.createRequests(['contentMimeType']));

  let metadataB = new MetadataItem();
  metadataB.contentMimeType = 'value2';

  assertTrue(item.storeProperties(2, metadataB));
  assertFalse(item.storeProperties(1, metadataA));

  var result = item.get(['contentMimeType']);
  assertEquals('value2', result.contentMimeType);
}

function testMetadataCacheItemClone() {
  var itemA = new MetadataCacheItem();
  itemA.startRequests(1, itemA.createRequests(['contentMimeType']));
  var itemB = itemA.clone();
  itemA.storeProperties(1, metadataA);
  assertFalse(itemB.hasFreshCache(['contentMimeType']));

  itemB.storeProperties(1, metadataA);
  assertTrue(itemB.hasFreshCache(['contentMimeType']));

  itemA.invalidate(2);
  assertTrue(itemB.hasFreshCache(['contentMimeType']));
}

function testMetadataCacheItemHasFreshCache() {
  var item = new MetadataCacheItem();
  assertFalse(item.hasFreshCache(['contentMimeType', 'externalFileUrl']));

  item.startRequests(
      1, item.createRequests(['contentMimeType', 'externalFileUrl']));

  let metadata = new MetadataItem();
  metadata.contentMimeType = 'mime';
  metadata.externalFileUrl = 'url';

  item.storeProperties(1, metadata);
  assertTrue(item.hasFreshCache(['contentMimeType', 'externalFileUrl']));

  item.invalidate(2);
  assertFalse(item.hasFreshCache(['contentMimeType', 'externalFileUrl']));

  item.startRequests(1, item.createRequests(['contentMimeType']));
  item.storeProperties(1, metadataA);
  assertFalse(item.hasFreshCache(['contentMimeType', 'externalFileUrl']));
  assertTrue(item.hasFreshCache(['contentMimeType']));
}

function testMetadataCacheItemShouldNotUpdateBeforeInvalidation() {
  var item = new MetadataCacheItem();
  item.startRequests(1, item.createRequests(['contentMimeType']));
  item.storeProperties(1, metadataA);

  let metadataB = new MetadataItem();
  metadataB.contentMimeType = 'value2';

  item.storeProperties(2, metadataB);
  assertEquals('value', item.get(['contentMimeType']).contentMimeType);
}

function testMetadataCacheItemError() {
  var item = new MetadataCacheItem();
  item.startRequests(1, item.createRequests(['contentThumbnailUrl']));

  let metadataWithError = new MetadataItem();
  metadataWithError.contentThumbnailUrlError = new Error('Error');

  item.storeProperties(1, metadataWithError);
  let property = item.get(['contentThumbnailUrl']);
  assertEquals(undefined, property.contentThumbnailUrl);
  assertEquals('Error', property.contentThumbnailUrlError.message);
}

function testMetadataCacheItemErrorShouldNotFetchedDirectly() {
  var item = new MetadataCacheItem();
  item.startRequests(1, item.createRequests(['contentThumbnailUrl']));

  let metadataWithError = new MetadataItem();
  metadataWithError.contentThumbnailUrlError = new Error('Error');

  item.storeProperties(1, metadataWithError);
  assertThrows(function() {
    item.get(['contentThumbnailUrlError']);
  });
}
