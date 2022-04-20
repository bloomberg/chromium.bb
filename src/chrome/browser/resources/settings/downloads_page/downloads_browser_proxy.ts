// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// </if>

export interface DownloadsBrowserProxy {
  initializeDownloads(): void;

  /**
   * @param enableLink whether to link or unlink account.
   */
  setDownloadsConnectionAccountLink(enableLink: boolean): void;

  selectDownloadLocation(): void;

  resetAutoOpenFileTypes(): void;

  // <if expr="chromeos_ash">
  /**
   * @param path Path to sanitze.
   * @return string to display in UI.
   */
  getDownloadLocationText(path: string): Promise<string>;
  // </if>
}

export class DownloadsBrowserProxyImpl implements DownloadsBrowserProxy {
  initializeDownloads() {
    chrome.send('initializeDownloads');
  }

  setDownloadsConnectionAccountLink(enableLink: boolean) {
    chrome.send('setDownloadsConnectionAccountLink', [enableLink]);
  }

  selectDownloadLocation() {
    chrome.send('selectDownloadLocation');
  }

  resetAutoOpenFileTypes() {
    chrome.send('resetAutoOpenFileTypes');
  }

  // <if expr="chromeos_ash">
  getDownloadLocationText(path: string) {
    return sendWithPromise('getDownloadLocationText', path);
  }
  // </if>

  static getInstance(): DownloadsBrowserProxy {
    return instance || (instance = new DownloadsBrowserProxyImpl());
  }

  static setInstance(obj: DownloadsBrowserProxy) {
    instance = obj;
  }
}

let instance: DownloadsBrowserProxy|null = null;
