// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('item tests', function() {
  /** @type {!downloads.Item} */
  let item;

  /** @type {!TestIconLoader} */
  let testIconLoader;

  setup(function() {
    PolymerTest.clearBody();

    // This isn't strictly necessary, but is a probably good idea.
    downloads.BrowserProxy.instance_ = new TestDownloadsProxy;

    testIconLoader = new TestIconLoader;
    downloads.IconLoader.instance_ = testIconLoader;

    item = document.createElement('downloads-item');
    document.body.appendChild(item);
  });

  test('dangerous downloads aren\'t linkable', () => {
    item.set('data', createDownload({
               dangerType: downloads.DangerType.DANGEROUS_FILE,
               fileExternallyRemoved: false,
               hideDate: true,
               state: downloads.States.DANGEROUS,
               url: 'http://evil.com'
             }));
    Polymer.dom.flush();

    assertTrue(item.$['file-link'].hidden);
    assertFalse(item.$.url.hasAttribute('href'));
  });

  test('icon loads successfully', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.set('data', createDownload({filePath: 'unique1', hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique1');
    Polymer.dom.flush();
    assertFalse(item.getFileIcon().hidden);
  });

  test('icon fails to load', async () => {
    testIconLoader.setShouldIconsLoad(false);
    item.set('data', createDownload({filePath: 'unique2', hideDate: false}));
    item.set('data', createDownload({hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique2');
    Polymer.dom.flush();
    assertTrue(item.getFileIcon().hidden);
  });
});
