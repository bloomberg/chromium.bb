// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DangerType, PageCallbackRouter, PageHandlerInterface, PageInterface, PageRemote, States} from 'chrome://downloads/downloads.js';

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

export class TestDownloadsProxy {
  constructor() {
    /** @type {PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @type {PageHandlerInterface} */
    this.handler = new FakePageHandler(this.callbackRouterRemote);
  }
}

/** @implements {PageHandlerInterface} */
class FakePageHandler {
  /** @param {PageInterface} */
  constructor(callbackRouterRemote) {
    /** @private {PageInterface} */
    this.callbackRouterRemote_ = callbackRouterRemote;

    /** @private {TestBrowserProxy} */
    this.callTracker_ = new TestBrowserProxy(['remove']);
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.callTracker_.whenCalled(methodName);
  }

  /** @override */
  async remove(id) {
    this.callbackRouterRemote_.removeItem(id);
    await this.callbackRouterRemote_.$.flushForTesting();
    this.callTracker_.methodCalled('remove', id);
  }

  /** @override */
  getDownloads(searchTerms) {}

  /** @override */
  openFileRequiringGesture(id) {}

  /** @override */
  drag(id) {}

  /** @override */
  saveDangerousRequiringGesture(id) {}

  /** @override */
  discardDangerous(id) {}

  /** @override */
  retryDownload(id) {}

  /** @override */
  show(id) {}

  /** @override */
  pause(id) {}

  /** @override */
  resume(id) {}

  /** @override */
  undo() {}

  /** @override */
  cancel(id) {}

  /** @override */
  clearAll() {}

  /** @override */
  openDownloadsFolderRequiringGesture() {}
}

export class TestIconLoader extends TestBrowserProxy {
  constructor() {
    super(['loadIcon']);

    /** @private */
    this.shouldIconsLoad_ = true;
  }

  /** @param {boolean} shouldIconsLoad */
  setShouldIconsLoad(shouldIconsLoad) {
    this.shouldIconsLoad_ = shouldIconsLoad;
  }

  /**
   * @param {!HTMLImageElement} imageEl
   * @param {string} filePath
   */
  loadIcon(imageEl, filePath) {
    this.methodCalled('loadIcon', filePath);
    return Promise.resolve(this.shouldIconsLoad_);
  }
}

/**
 * @param {Object=} config
 * @return {!downloads.Data}
 */
export function createDownload(config) {
  return Object.assign(
      {
        byExtId: '',
        byExtName: '',
        dangerType: DangerType.NOT_DANGEROUS,
        dateString: '',
        fileExternallyRemoved: false,
        filePath: '/some/file/path',
        fileName: 'download 1',
        fileUrl: 'file:///some/file/path',
        id: '',
        isDangerous: false,
        isMixedContent: false,
        lastReasonText: '',
        otr: false,
        percent: 100,
        progressStatusText: '',
        showInFolderText: '',
        resume: false,
        retry: false,
        return: false,
        sinceString: 'Today',
        started: Date.now() - 10000,
        state: States.COMPLETE,
        total: -1,
        url: 'http://permission.site',
      },
      config || {});
}
