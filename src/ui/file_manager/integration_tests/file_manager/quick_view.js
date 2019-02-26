// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Opens the Quick View dialog on a given file |name|. The file must be present
 * in the Files app file list.
 *
 * @param {string} appId Files app windowId.
 * @param {string} name File name.
 */
async function openQuickView(appId, name) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayBlock(elements) {
    const haveElements = Array.isArray(elements) && elements.length !== 0;
    if (!haveElements || elements[0].styles.display !== 'block')
      return pending(caller, 'Waiting for Quick View to open.');
    return;
  }

  // Select file |name| in the file list.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, [name]),
      'selectFile failed');

  // Press the space key.
  const space = ['#file-list', ' ', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space),
      'fakeKeyDown failed');

  // Check: the Quick View element should be shown.
  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog[open]'];
    return checkQuickViewElementsDisplayBlock(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Assuming that Quick View is currently open per openQuickView above, closes
 * the Quick View dialog.
 *
 * @param {string} appId Files app windowId.
 */
async function closeQuickView(appId) {
  let caller = getCaller();

  function checkQuickViewElementsDisplayNone(elements) {
    chrome.test.assertTrue(Array.isArray(elements));
    if (elements.length > 0 && elements[0].styles.display !== 'none')
      return pending(caller, 'Waiting for Quick View to close.');
    return;
  }

  // Click on Quick View to close it.
  const panelElements = ['#quick-view', '#contentPanel'];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [panelElements]),
      'fakeMouseClick failed');

  // Check: the Quick View element should not be shown.
  await repeatUntil(async () => {
    const elements = ['#quick-view', '#dialog:not([open])'];
    return checkQuickViewElementsDisplayNone(
        await remoteCall.callRemoteTestUtil(
            'deepQueryAllElements', appId, [elements, ['display']]));
  });
}

/**
 * Tests opening Quick View on a local downloads file.
 */
testcase.openQuickView = async function() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening then closing Quick View on a local downloads file.
 */
testcase.closeQuickView = async function() {
  // Open Files app on Downloads containing ENTRIES.hello.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Close Quick View.
  await closeQuickView(appId);
};

/**
 * Tests opening Quick View on a Drive file.
 */
testcase.openQuickViewDrive = async function() {
  // Open Files app on Drive containing ENTRIES.hello.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], [ENTRIES.hello]);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Check that the correct mime type is displayed.
  const mimeTypeSelector = [
    '#quick-view',
    '#metadata-box',
    // TODO(crbug.com/677338): Replace the attribute selector with key="Type"
    // once the key is populated with polymer2 enabled.
    'files-metadata-entry[i18n-values="key:METADATA_BOX_MEDIA_MIME_TYPE"]',
    '#value div',
  ];
  chrome.test.assertEq(
      'text/plain',
      (await remoteCall.waitForElement(appId, mimeTypeSelector)).text);
};

/**
 * Tests opening Quick View on a USB file.
 */
testcase.openQuickViewUsb = async function() {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.photos], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the USB files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a USB file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on an MTP file.
 */
testcase.openQuickViewMtp = async function() {
  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.photos], []);

  // Mount a non-empty MTP volume.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP volume to mount.
  await remoteCall.waitForElement(appId, MTP_VOLUME_QUERY);

  // Click to open the MTP volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY]),
      'fakeMouseClick failed');

  // Check: the MTP files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open an MTP file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on a Crostini file.
 */
testcase.openQuickViewCrostini = async function() {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinuxFiles = '#directory-tree [volume-type-icon="crostini"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.photos], []);

  // Check: the fake Linux files icon should be shown.
  await remoteCall.waitForElement(appId, fakeLinuxFiles);

  // Add files to the Crostini volume.
  await addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET);

  // Click the fake Linux files icon to mount the Crostini volume.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [fakeLinuxFiles]),
      'fakeMouseClick failed');

  // Check: the Crostini volume icon should appear.
  await remoteCall.waitForElement(appId, realLinuxFiles);

  // Check: the Crostini files should appear in the file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open a Crostini file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);
};

/**
 * Tests opening Quick View on an Android file.
 */
testcase.openQuickViewAndroid = async function() {
  // Open Files app on Android files.
  const appId = await openNewWindow(null, RootPath.ANDROID_FILES);

  // Add files to the Android files volume.
  const entrySet = BASIC_ANDROID_ENTRY_SET.concat([ENTRIES.documentsText]);
  await addEntries(['android_files'], entrySet);

  // Wait for the file list to appear.
  await remoteCall.waitForElement(appId, '#file-list');

  // Check: the basic Android file set should appear in the file list.
  let files = TestEntryInfo.getExpectedRows(BASIC_ANDROID_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Navigate to the Android files '/Documents' directory.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Documents', 'My files/Play files', 'android_files');

  // Check: the 'android.txt' file should appear in the file list.
  files = [ENTRIES.documentsText.getExpectedRow()];
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

  // Open the Android file in Quick View.
  const documentsFileName = ENTRIES.documentsText.nameText;
  await openQuickView(appId, documentsFileName);
};

/**
 * Tests opening Quick View and scrolling its <webview> which contains a tall
 * text document.
 */
testcase.openQuickViewScrollText = async function() {
  const caller = getCaller();

  /**
   * The text <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.text-content'];

  function scrollQuickViewTextBy(y) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [webView, doScrollBy]);
  }

  async function checkQuickViewTextScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      console.log('checkQuickViewTextScrollY: scrollY '.concat(scrollY));
      await scrollQuickViewTextBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
  }

  // Open Files app on Downloads containing ENTRIES.tallText.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || !elements[0].attributes.src)
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the Quick View <webview> scrollY.
  const getScrollY = 'window.scrollY';
  await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getScrollY]);

  // Check: the initial <webview> scrollY should be 0.
  chrome.test.assertEq('0', scrollY.toString());

  // Scroll the <webview> and verify that it scrolled.
  await repeatUntil(async () => {
    const getScrollY = 'window.scrollY';
    return checkQuickViewTextScrollY(await remoteCall.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [webView, getScrollY]));
  });
};

/**
 * Tests opening Quick View on a text document to verify that the background
 * color of the <webview> root (html) element is solid white.
 */
testcase.openQuickViewBackgroundColorText = async function() {
  const caller = getCaller();

  /**
   * The text <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.text-content'];

  // Open Files app on Downloads containing ENTRIES.tallText.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.tallText], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallText.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewTextLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || !elements[0].attributes.src)
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewTextLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the <webview> root (html) element backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.documentElement).backgroundColor';
  const backgroundColor = await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle]);

  // Check: the <webview> root backgroundColor should be solid white.
  chrome.test.assertEq('rgb(255, 255, 255)', backgroundColor[0]);
};

/**
 * Tests opening Quick View containing a PDF document.
 */
testcase.openQuickViewPdf = async function() {
  const caller = getCaller();

  /**
   * The PDF <webview> resides in the #quick-view shadow DOM, as a child of
   * the #dialog element.
   */
  const webView = ['#quick-view', '#dialog[open] webview.content'];

  // Open Files app on Downloads containing ENTRIES.tallPdf.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.tallPdf], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallPdf.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewPdfLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || !elements[0].attributes.src)
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewPdfLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the <webview> embed type attribute.
  function checkPdfEmbedType(type) {
    let haveElements = Array.isArray(type) && type.length === 1;
    if (!haveElements || !type[0].toString().includes('pdf'))
      return pending(caller, 'Waiting for plugin <embed> type.');
    return type[0];
  }
  const type = await repeatUntil(async () => {
    const getType = 'window.document.querySelector("embed").type';
    return checkPdfEmbedType(await remoteCall.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [webView, getType]));
  });

  // Check: the <webview> embed type should be PDF mime type.
  chrome.test.assertEq('application/pdf', type);
};

/**
 * Tests opening Quick View and scrolling its <webview> which contains a tall
 * html document.
 */
testcase.openQuickViewScrollHtml = async function() {
  const caller = getCaller();

  /**
   * The <webview> resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="html"]', 'webview'];

  function scrollQuickViewHtmlBy(y) {
    const doScrollBy = `window.scrollBy(0,${y})`;
    return remoteCall.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [webView, doScrollBy]);
  }

  async function checkQuickViewHtmlScrollY(scrollY) {
    if (!scrollY || Number(scrollY.toString()) <= 200) {
      console.log('checkQuickViewHtmlScrollY: scrollY '.concat(scrollY));
      await scrollQuickViewHtmlBy(100);
      return pending(caller, 'Waiting for Quick View to scroll.');
    }
  }

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallHtml.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewHtmlLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || elements[0].attributes.loaded !== '')
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewHtmlLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the Quick View <webview> scrollY.
  const getScrollY = 'window.scrollY';
  await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getScrollY]);

  // Check: the initial <webview> scrollY should be 0.
  chrome.test.assertEq('0', scrollY.toString());

  // Scroll the <webview> and verify that it scrolled.
  await repeatUntil(async () => {
    const getScrollY = 'window.scrollY';
    return checkQuickViewHtmlScrollY(await remoteCall.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [webView, getScrollY]));
  });
};

/**
 * Tests opening Quick View on an html document to verify that the background
 * color of the <files-safe-media type="html"> that contains the <webview> is
 * solid white.
 */
testcase.openQuickViewBackgroundColorHtml = async function() {
  const caller = getCaller();

  /**
   * The <webview> resides in the <files-safe-media type="html"> shadow DOM,
   * which is a child of the #quick-view shadow DOM. This test only needs to
   * examine the <files-safe-media> element.
   */
  const fileSafeMedia = ['#quick-view', 'files-safe-media[type="html"]'];

  // Open Files app on Downloads containing ENTRIES.tallHtml.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.tallHtml], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.tallHtml.nameText);

  // Get the <files-safe-media type='html'> backgroundColor style.
  function getFileSafeMediaBackgroundColor(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || !elements[0].styles.backgroundColor)
      return pending(caller, 'Waiting for <file-safe-media> element.');
    return elements[0].styles.backgroundColor;
  }
  const backgroundColor = await repeatUntil(async () => {
    const styles = ['display', 'backgroundColor'];
    return getFileSafeMediaBackgroundColor(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [fileSafeMedia, styles]));
  });

  // Check: the <files-safe-media> backgroundColor should be solid white.
  chrome.test.assertEq('rgb(255, 255, 255)', backgroundColor);
};

/**
 * Tests opening Quick View containing an audio file.
 */
testcase.openQuickViewAudio = async function() {
  const caller = getCaller();

  /**
   * The <webview> resides in the <files-safe-media type="audio"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="audio"]', 'webview'];

  // Open Files app on Downloads containing ENTRIES.beautiful song.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.beautiful], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.beautiful.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewAudioLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || elements[0].attributes.loaded !== '')
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewAudioLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the <webview> document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle]);

  // Check: the <webview> body backgroundColor should be transparent black.
  chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
};

/**
 * Tests opening Quick View containing an image.
 */
testcase.openQuickViewImage = async function() {
  const caller = getCaller();

  /**
   * The <webview> resides in the <files-safe-media type="image"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="image"]', 'webview'];

  // Open Files app on Downloads containing ENTRIES.smallJpeg.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.smallJpeg], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.smallJpeg.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewImageLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || elements[0].attributes.loaded !== '')
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewImageLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the <webview> document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle]);

  // Check: the <webview> body backgroundColor should be transparent black.
  chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
};

/**
 * Tests opening Quick View containing a video.
 */
testcase.openQuickViewVideo = async function() {
  const caller = getCaller();

  /**
   * The <webview> resides in the <files-safe-media type="video"> shadow DOM,
   * which is a child of the #quick-view shadow DOM.
   */
  const webView = ['#quick-view', 'files-safe-media[type="video"]', 'webview'];

  // Open Files app on Downloads containing ENTRIES.world video.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.world], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.world.nameText);

  // Wait for the Quick View <webview> to load and display its content.
  function checkWebViewVideoLoaded(elements) {
    let haveElements = Array.isArray(elements) && elements.length === 1;
    if (haveElements)
      haveElements = elements[0].styles.display.includes('block');
    if (!haveElements || elements[0].attributes.loaded !== '')
      return pending(caller, 'Waiting for <webview> to load.');
    return;
  }
  await repeatUntil(async () => {
    return checkWebViewVideoLoaded(await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [webView, ['display']]));
  });

  // Get the <webview> document.body backgroundColor style.
  const getBackgroundStyle =
      'window.getComputedStyle(document.body).backgroundColor';
  const backgroundColor = await remoteCall.callRemoteTestUtil(
      'deepExecuteScriptInWebView', appId, [webView, getBackgroundStyle]);

  // Check: the <webview> body backgroundColor should be transparent black.
  chrome.test.assertEq('rgba(0, 0, 0, 0)', backgroundColor[0]);
};

/**
 * Tests close/open metadata info via Enter key.
 */
testcase.pressEnterOnInfoBoxToOpenClose = async function() {
  const infoButton = ['#quick-view', '#metadata-button'];
  const key = [infoButton, 'Enter', false, false, false];
  const infoShown = ['#quick-view', '#contentPanel[metadata-box-active]'];
  const infoHidden =
      ['#quick-view', '#contentPanel:not([metadata-box-active])'];


  // Open Files app on Downloads containing ENTRIES.hello.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.hello], []);

  // Open the file in Quick View.
  await openQuickView(appId, ENTRIES.hello.nameText);

  // Press Enter on info button to close metadata box.
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Info should be hidden.
  await remoteCall.waitForElement(appId, infoHidden);

  // Press Enter on info button to open metadata box.
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key);

  // Info should be shown.
  await remoteCall.waitForElement(appId, infoShown);

  // Close Quick View.
  await closeQuickView(appId);
};

/**
 * Tests that Quick View doesn't open with multiple files selected.
 */
testcase.cantOpenQuickViewWithMultipleFiles = async function() {
  // Open Files app on Downloads containing ENTRIES.hello and ENTRIES.world.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.hello, ENTRIES.world], []);

  // Select all 2 files.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Wait for the files to be selected.
  await remoteCall.waitForElement(
      appId, '#cancel-selection-button-wrapper:not([hidden]):not([disabled])');

  // Attempt to open Quick View via its keyboard shortcut.
  const space = ['#file-list', ' ', false, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, space);

  // Wait for it to possibly open.
  await new Promise((resolve) => {
    window.setTimeout(resolve, 500);
  });

  // Check Quick View hasn't opened.
  chrome.test.assertEq(
      [],
      await remoteCall.callRemoteTestUtil(
          'deepQueryAllElements', appId, [['#quick-view', '#dialog[open]']]));
};
