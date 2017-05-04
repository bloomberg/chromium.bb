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
 * @implements {Bindings.CSSWorkspaceBinding.SourceMapping}
 */
Bindings.SASSSourceMapping = class {
  /**
   * @param {!SDK.SourceMapManager} sourceMapManager
   * @param {!Workspace.Workspace} workspace
   * @param {!Bindings.NetworkProject} networkProject
   */
  constructor(sourceMapManager, workspace, networkProject) {
    this._sourceMapManager = sourceMapManager;
    this._networkProject = networkProject;
    this._workspace = workspace;
    this._eventListeners = [
      this._sourceMapManager.addEventListener(
          SDK.SourceMapManager.Events.SourceMapAttached, this._sourceMapAttached, this),
      this._sourceMapManager.addEventListener(
          SDK.SourceMapManager.Events.SourceMapDetached, this._sourceMapDetached, this),
      this._sourceMapManager.addEventListener(
          SDK.SourceMapManager.Events.SourceMapChanged, this._sourceMapChanged, this)
    ];

    /** @type {!Multimap<string, !SDK.CSSStyleSheetHeader>} */
    this._sourceMapIdToHeaders = new Multimap();
  }

  /**
   * @param {?SDK.SourceMap} sourceMap
   */
  _sourceMapAttachedForTest(sourceMap) {
  }

  /**
   * @param {string} frameId
   * @param {string} sourceMapURL
   * @return {string}
   */
  static _sourceMapId(frameId, sourceMapURL) {
    return frameId + ':' + sourceMapURL;
  }

  /**
   * @param {!Common.Event} event
   */
  _sourceMapAttached(event) {
    var header = /** @type {!SDK.CSSStyleSheetHeader} */ (event.data.client);
    var sourceMap = /** @type {!SDK.SourceMap} */ (event.data.sourceMap);
    var sourceMapId = Bindings.SASSSourceMapping._sourceMapId(header.frameId, sourceMap.url());
    if (this._sourceMapIdToHeaders.has(sourceMapId)) {
      this._sourceMapIdToHeaders.set(sourceMapId, header);
      this._sourceMapAttachedForTest(sourceMap);
      return;
    }
    this._sourceMapIdToHeaders.set(sourceMapId, header);

    for (var sassURL of sourceMap.sourceURLs()) {
      var contentProvider = sourceMap.sourceContentProvider(sassURL, Common.resourceTypes.SourceMapStyleSheet);
      var embeddedContent = sourceMap.embeddedContentByURL(sassURL);
      var embeddedContentLength = typeof embeddedContent === 'string' ? embeddedContent.length : null;
      var uiSourceCode =
          this._networkProject.addSourceMapFile(contentProvider, header.frameId, false, embeddedContentLength);
      uiSourceCode[Bindings.SASSSourceMapping._sourceMapSymbol] = sourceMap;
    }
    Bindings.cssWorkspaceBinding.updateLocations(header);
    this._sourceMapAttachedForTest(sourceMap);
  }

  /**
   * @param {!Common.Event} event
   */
  _sourceMapDetached(event) {
    var header = /** @type {!SDK.CSSStyleSheetHeader} */ (event.data.client);
    var sourceMap = /** @type {!SDK.SourceMap} */ (event.data.sourceMap);
    var sourceMapId = Bindings.SASSSourceMapping._sourceMapId(header.frameId, sourceMap.url());
    this._sourceMapIdToHeaders.remove(sourceMapId, header);
    if (this._sourceMapIdToHeaders.has(sourceMapId))
      return;
    for (var sassURL of sourceMap.sourceURLs())
      this._networkProject.removeSourceMapFile(sassURL, header.frameId, false);
    Bindings.cssWorkspaceBinding.updateLocations(header);
  }

  /**
   * @param {!Common.Event} event
   */
  _sourceMapChanged(event) {
    var sourceMap = /** @type {!SDK.SourceMap} */ (event.data.sourceMap);
    var newSources = /** @type {!Map<string, string>} */ (event.data.newSources);
    var headers = this._sourceMapManager.clientsForSourceMap(sourceMap);
    var handledUISourceCodes = new Set();
    for (var header of headers) {
      Bindings.cssWorkspaceBinding.updateLocations(header);
      for (var sourceURL of newSources.keys()) {
        var uiSourceCode = Bindings.NetworkProject.uiSourceCodeForStyleURL(this._workspace, sourceURL, header);
        if (!uiSourceCode) {
          console.error('Failed to update source for ' + sourceURL);
          continue;
        }
        if (handledUISourceCodes.has(uiSourceCode))
          continue;
        handledUISourceCodes.add(uiSourceCode);
        uiSourceCode[Bindings.SASSSourceMapping._sourceMapSymbol] = sourceMap;
        var sassText = /** @type {string} */ (newSources.get(sourceURL));
        uiSourceCode.setWorkingCopy(sassText);
      }
    }
  }

  /**
   * @override
   * @param {!SDK.CSSLocation} rawLocation
   * @return {?Workspace.UILocation}
   */
  rawLocationToUILocation(rawLocation) {
    var header = rawLocation.header();
    if (!header)
      return null;
    var sourceMap = this._sourceMapManager.sourceMapForClient(header);
    if (!sourceMap)
      return null;
    var entry = sourceMap.findEntry(rawLocation.lineNumber, rawLocation.columnNumber);
    if (!entry || !entry.sourceURL)
      return null;
    var uiSourceCode = Bindings.NetworkProject.uiSourceCodeForStyleURL(this._workspace, entry.sourceURL, header);
    if (!uiSourceCode)
      return null;
    return uiSourceCode.uiLocation(entry.sourceLineNumber || 0, entry.sourceColumnNumber);
  }

  /**
   * @override
   * @param {!Workspace.UILocation} uiLocation
   * @return {!Array<!SDK.CSSLocation>}
   */
  uiLocationToRawLocations(uiLocation) {
    var sourceMap = uiLocation.uiSourceCode[Bindings.SASSSourceMapping._sourceMapSymbol];
    if (!sourceMap)
      return [];
    var entries =
        sourceMap.findReverseEntries(uiLocation.uiSourceCode.url(), uiLocation.lineNumber, uiLocation.columnNumber);
    var locations = [];
    for (var header of this._sourceMapManager.clientsForSourceMap(sourceMap))
      locations.pushAll(entries.map(entry => new SDK.CSSLocation(header, entry.lineNumber, entry.columnNumber)));
    return locations;
  }

  dispose() {
    Common.EventTarget.removeEventListeners(this._eventListeners);
  }
};

Bindings.SASSSourceMapping._sourceMapSymbol = Symbol('sourceMap');
