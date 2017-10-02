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

/**
 * @unrestricted
 */
Workspace.FileManager = class extends Common.Object {
  constructor() {
    super();
    this._savedURLsSetting = Common.settings.createLocalSetting('savedURLs', {});
    /** @type {!Map<string, function(?{fileSystemPath: (string|undefined)})>} */
    this._saveCallbacks = new Map();
    InspectorFrontendHost.events.addEventListener(InspectorFrontendHostAPI.Events.SavedURL, this._savedURL, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.CanceledSaveURL, this._canceledSavedURL, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.AppendedToURL, this._appendedToURL, this);
  }

  /**
   * @param {string} url
   * @param {string} content
   * @param {boolean} forceSaveAs
   * @return {!Promise<?{fileSystemPath: (string|undefined)}>}
   */
  save(url, content, forceSaveAs) {
    // Remove this url from the saved URLs while it is being saved.
    var savedURLs = this._savedURLsSetting.get();
    delete savedURLs[url];
    this._savedURLsSetting.set(savedURLs);
    InspectorFrontendHost.save(url, content, forceSaveAs);
    return new Promise(resolve => this._saveCallbacks.set(url, resolve));
  }

  /**
   * @param {!Common.Event} event
   */
  _savedURL(event) {
    var url = /** @type {string} */ (event.data.url);
    var callback = this._saveCallbacks.get(url);
    this._saveCallbacks.delete(url);
    if (callback)
      callback({fileSystemPath: /** @type {string} */ (event.data.fileSystemPath)});
    var savedURLs = this._savedURLsSetting.get();
    savedURLs[url] = true;
    this._savedURLsSetting.set(savedURLs);
  }

  /**
   * @param {!Common.Event} event
   */
  _canceledSavedURL(event) {
    var url = /** @type {string} */ (event.data);
    var callback = this._saveCallbacks.get(url);
    this._saveCallbacks.delete(url);
    if (callback)
      callback(null);
  }

  /**
   * @param {string} url
   * @return {boolean}
   */
  isURLSaved(url) {
    var savedURLs = this._savedURLsSetting.get();
    return savedURLs[url];
  }

  /**
   * @param {string} url
   * @param {string} content
   */
  append(url, content) {
    InspectorFrontendHost.append(url, content);
  }

  /**
   * @param {string} url
   */
  close(url) {
    // Currently a no-op.
  }

  /**
   * @param {!Common.Event} event
   */
  _appendedToURL(event) {
    var url = /** @type {string} */ (event.data);
    this.dispatchEventToListeners(Workspace.FileManager.Events.AppendedToURL, url);
  }
};

/** @enum {symbol} */
Workspace.FileManager.Events = {
  AppendedToURL: Symbol('AppendedToURL')
};

/**
 * @type {?Workspace.FileManager}
 */
Workspace.fileManager;
