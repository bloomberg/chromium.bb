/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
/* eslint-disable rulesdir/no_underscored_properties */
/* eslint-disable @typescript-eslint/no-unused-vars */

import * as Common from '../common/common.js';
import * as i18n from '../i18n/i18n.js';
import * as Platform from '../platform/platform.js';
import * as Root from '../root/root.js';

import type {CanShowSurveyResult, ContextMenuDescriptor, EnumeratedHistogram, ExtensionDescriptor, InspectorFrontendHostAPI, LoadNetworkResourceResult, ShowSurveyResult} from './InspectorFrontendHostAPI.js';
import {EventDescriptors, Events} from './InspectorFrontendHostAPI.js';  // eslint-disable-line no-unused-vars
import {streamWrite as resourceLoaderStreamWrite} from './ResourceLoader.js';

const UIStrings = {
  /**
  *@description Document title in Inspector Frontend Host of the DevTools window
  *@example {example.com} PH1
  */
  devtoolsS: 'DevTools - {PH1}',
};
const str_ = i18n.i18n.registerUIStrings('core/host/InspectorFrontendHost.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export class InspectorFrontendHostStub implements InspectorFrontendHostAPI {
  _urlsBeingSaved: Map<string, string[]>;
  events!: Common.EventTarget.EventTarget;
  _windowVisible?: boolean;

  constructor() {
    function stopEventPropagation(this: InspectorFrontendHostAPI, event: KeyboardEvent): void {
      // Let browser handle Ctrl+/Ctrl- shortcuts in hosted mode.
      const zoomModifier = this.platform() === 'mac' ? event.metaKey : event.ctrlKey;
      if (zoomModifier && (event.key === '+' || event.key === '-')) {
        event.stopPropagation();
      }
    }
    document.addEventListener('keydown', event => {
      stopEventPropagation.call(this, (event as KeyboardEvent));
    }, true);
    this._urlsBeingSaved = new Map();
  }

  platform(): string {
    const userAgent = navigator.userAgent;
    if (userAgent.includes('Windows NT')) {
      return 'windows';
    }
    if (userAgent.includes('Mac OS X')) {
      return 'mac';
    }
    return 'linux';
  }

  loadCompleted(): void {
  }

  bringToFront(): void {
    this._windowVisible = true;
  }

  closeWindow(): void {
    this._windowVisible = false;
  }

  setIsDocked(isDocked: boolean, callback: () => void): void {
    setTimeout(callback, 0);
  }

  showSurvey(trigger: string, callback: (arg0: ShowSurveyResult) => void): void {
    setTimeout(() => callback({surveyShown: false}), 0);
  }

  canShowSurvey(trigger: string, callback: (arg0: CanShowSurveyResult) => void): void {
    setTimeout(() => callback({canShowSurvey: false}), 0);
  }

  /**
   * Requests inspected page to be placed atop of the inspector frontend with specified bounds.
   */
  setInspectedPageBounds(bounds: {
    x: number,
    y: number,
    width: number,
    height: number,
  }): void {
  }

  inspectElementCompleted(): void {
  }

  setInjectedScriptForOrigin(origin: string, script: string): void {
  }

  inspectedURLChanged(url: string): void {
    document.title = i18nString(UIStrings.devtoolsS, {PH1: url.replace(/^https?:\/\//, '')});
  }

  copyText(text: string|null|undefined): void {
    if (text === undefined || text === null) {
      return;
    }
    navigator.clipboard.writeText(text);
  }

  openInNewTab(url: string): void {
    window.open(url, '_blank');
  }

  showItemInFolder(fileSystemPath: string): void {
    Common.Console.Console.instance().error(
        'Show item in folder is not enabled in hosted mode. Please inspect using chrome://inspect');
  }

  save(url: string, content: string, forceSaveAs: boolean): void {
    let buffer = this._urlsBeingSaved.get(url);
    if (!buffer) {
      buffer = [];
      this._urlsBeingSaved.set(url, buffer);
    }
    buffer.push(content);
    this.events.dispatchEventToListeners(Events.SavedURL, {url, fileSystemPath: url});
  }

  append(url: string, content: string): void {
    const buffer = this._urlsBeingSaved.get(url);
    if (buffer) {
      buffer.push(content);
      this.events.dispatchEventToListeners(Events.AppendedToURL, url);
    }
  }

  close(url: string): void {
    const buffer = this._urlsBeingSaved.get(url) || [];
    this._urlsBeingSaved.delete(url);
    let fileName = '';

    if (url) {
      try {
        const trimmed = Platform.StringUtilities.trimURL(url);
        fileName = Platform.StringUtilities.removeURLFragment(trimmed);
      } catch (error) {
        // If url is not a valid URL, it is probably a filename.
        fileName = url;
      }
    }

    const link = document.createElement('a');
    link.download = fileName;
    const blob = new Blob([buffer.join('')], {type: 'text/plain'});
    const blobUrl = URL.createObjectURL(blob);
    link.href = blobUrl;
    link.click();
    URL.revokeObjectURL(blobUrl);
  }

  sendMessageToBackend(message: string): void {
  }

  recordEnumeratedHistogram(actionName: EnumeratedHistogram, actionCode: number, bucketSize: number): void {
  }

  recordPerformanceHistogram(histogramName: string, duration: number): void {
  }

  recordUserMetricsAction(umaName: string): void {
  }

  requestFileSystems(): void {
    this.events.dispatchEventToListeners(Events.FileSystemsLoaded, []);
  }

  addFileSystem(type?: string): void {
  }

  removeFileSystem(fileSystemPath: string): void {
  }

  isolatedFileSystem(fileSystemId: string, registeredName: string): FileSystem|null {
    return null;
  }

  loadNetworkResource(
      url: string, headers: string, streamId: number, callback: (arg0: LoadNetworkResourceResult) => void): void {
    Root.Runtime.loadResourcePromise(url)
        .then(function(text) {
          resourceLoaderStreamWrite(streamId, text);
          callback({
            statusCode: 200,
            headers: undefined,
            messageOverride: undefined,
            netError: undefined,
            netErrorName: undefined,
            urlValid: undefined,
          });
        })
        .catch(function() {
          callback({
            statusCode: 404,
            headers: undefined,
            messageOverride: undefined,
            netError: undefined,
            netErrorName: undefined,
            urlValid: undefined,
          });
        });
  }

  getPreferences(callback: (arg0: {
                   [x: string]: string,
                 }) => void): void {
    const prefs: {
      [x: string]: string,
    } = {};
    for (const name in window.localStorage) {
      prefs[name] = window.localStorage[name];
    }
    callback(prefs);
  }

  setPreference(name: string, value: string): void {
    window.localStorage[name] = value;
  }

  removePreference(name: string): void {
    delete window.localStorage[name];
  }

  clearPreferences(): void {
    window.localStorage.clear();
  }

  upgradeDraggedFileSystemPermissions(fileSystem: FileSystem): void {
  }

  indexPath(requestId: number, fileSystemPath: string, excludedFolders: string): void {
  }

  stopIndexing(requestId: number): void {
  }

  searchInPath(requestId: number, fileSystemPath: string, query: string): void {
  }

  zoomFactor(): number {
    return 1;
  }

  zoomIn(): void {
  }

  zoomOut(): void {
  }

  resetZoom(): void {
  }

  setWhitelistedShortcuts(shortcuts: string): void {
  }

  setEyeDropperActive(active: boolean): void {
  }

  showCertificateViewer(certChain: string[]): void {
  }

  reattach(callback: () => void): void {
  }

  readyForTest(): void {
  }

  connectionReady(): void {
  }

  setOpenNewWindowForPopups(value: boolean): void {
  }

  setDevicesDiscoveryConfig(config: Adb.Config): void {
  }

  setDevicesUpdatesEnabled(enabled: boolean): void {
  }

  performActionOnRemotePage(pageId: string, action: string): void {
  }

  openRemotePage(browserId: string, url: string): void {
  }

  openNodeFrontend(): void {
  }

  showContextMenuAtPoint(x: number, y: number, items: ContextMenuDescriptor[], document: Document): void {
    throw 'Soft context menu should be used';
  }

  isHostedMode(): boolean {
    return true;
  }

  setAddExtensionCallback(callback: (arg0: ExtensionDescriptor) => void): void {
    // Extensions are not supported in hosted mode.
  }
}

// @ts-ignore Global injected by devtools-compatibility.js
// eslint-disable-next-line @typescript-eslint/naming-convention
export let InspectorFrontendHostInstance: InspectorFrontendHostStub = window.InspectorFrontendHost;

class InspectorFrontendAPIImpl {
  _debugFrontend: boolean;

  constructor() {
    this._debugFrontend = (Boolean(Root.Runtime.Runtime.queryParam('debugFrontend'))) ||
        // @ts-ignore Compatibility hacks
        (window['InspectorTest'] && window['InspectorTest']['debugTest']);

    for (const descriptor of EventDescriptors) {
      // @ts-ignore Dispatcher magic
      this[descriptor[1]] = this._dispatch.bind(this, descriptor[0], descriptor[2], descriptor[3]);
    }
  }

  _dispatch(name: symbol, signature: string[], runOnceLoaded: boolean, ...params: string[]): void {
    if (this._debugFrontend) {
      setTimeout(() => innerDispatch(), 0);
    } else {
      innerDispatch();
    }

    function innerDispatch(): void {
      // Single argument methods get dispatched with the param.
      if (signature.length < 2) {
        try {
          InspectorFrontendHostInstance.events.dispatchEventToListeners(name, params[0]);
        } catch (error) {
          console.error(error + ' ' + error.stack);
        }
        return;
      }
      const data: {
        [x: string]: string,
      } = {};
      for (let i = 0; i < signature.length; ++i) {
        data[signature[i]] = params[i];
      }
      try {
        InspectorFrontendHostInstance.events.dispatchEventToListeners(name, data);
      } catch (error) {
        console.error(error + ' ' + error.stack);
      }
    }
  }

  streamWrite(id: number, chunk: string): void {
    resourceLoaderStreamWrite(id, chunk);
  }
}

(function(): void {

function initializeInspectorFrontendHost(): void {
  let proto;
  if (!InspectorFrontendHostInstance) {
    // Instantiate stub for web-hosted mode if necessary.
    // @ts-ignore Global injected by devtools-compatibility.js
    window.InspectorFrontendHost = InspectorFrontendHostInstance = new InspectorFrontendHostStub();
  } else {
    // Otherwise add stubs for missing methods that are declared in the interface.
    proto = InspectorFrontendHostStub.prototype;
    for (const name of Object.getOwnPropertyNames(proto)) {
      // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
      // @ts-expect-error
      const stub = proto[name];
      // @ts-ignore Global injected by devtools-compatibility.js
      if (typeof stub !== 'function' || InspectorFrontendHostInstance[name]) {
        continue;
      }

      console.error(`Incompatible embedder: method Host.InspectorFrontendHost.${name} is missing. Using stub instead.`);
      // @ts-ignore Global injected by devtools-compatibility.js
      InspectorFrontendHostInstance[name] = stub;
    }
  }

  // Attach the events object.
  InspectorFrontendHostInstance.events = new Common.ObjectWrapper.ObjectWrapper();
}

// FIXME: This file is included into both apps, since the devtools_app needs the InspectorFrontendHostAPI only,
// so the host instance should not be initialized there.
initializeInspectorFrontendHost();
// @ts-ignore Global injected by devtools-compatibility.js
window.InspectorFrontendAPI = new InspectorFrontendAPIImpl();
})();

export function isUnderTest(prefs?: {
  [x: string]: string,
}): boolean {
  // Integration tests rely on test queryParam.
  if (Root.Runtime.Runtime.queryParam('test')) {
    return true;
  }
  // Browser tests rely on prefs.
  if (prefs) {
    return prefs['isUnderTest'] === 'true';
  }
  return Common.Settings.Settings.hasInstance() &&
      Common.Settings.Settings.instance().createSetting('isUnderTest', false).get();
}
