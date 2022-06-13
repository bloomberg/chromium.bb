// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../common/common.js';
import * as i18n from '../i18n/i18n.js';
import * as Platform from '../platform/platform.js';

import type {FrameAssociated} from './FrameAssociated.js';
import type {Target} from './Target.js';
import {Events as TargetManagerEvents, TargetManager} from './TargetManager.js';
import type {SourceMap} from './SourceMap.js';
import {TextSourceMap} from './SourceMap.js';

const UIStrings = {
  /**
  *@description Error message when failing to load a source map text
  *@example {An error occurred} PH1
  */
  devtoolsFailedToLoadSourcemapS: 'DevTools failed to load source map: {PH1}',
};
const str_ = i18n.i18n.registerUIStrings('core/sdk/SourceMapManager.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export class SourceMapManager<T extends FrameAssociated> extends Common.ObjectWrapper.ObjectWrapper<EventTypes<T>> {
  readonly #target: Target;
  #isEnabled: boolean;
  readonly #relativeSourceURL: Map<T, string>;
  readonly #relativeSourceMapURL: Map<T, string>;
  #resolvedSourceMapId: Map<T, string>;
  readonly #sourceMapById: Map<string, SourceMap>;
  #sourceMapIdToLoadingClients: Platform.MapUtilities.Multimap<string, T>;
  #sourceMapIdToClients: Platform.MapUtilities.Multimap<string, T>;

  constructor(target: Target) {
    super();

    this.#target = target;
    this.#isEnabled = true;

    this.#relativeSourceURL = new Map();
    this.#relativeSourceMapURL = new Map();
    this.#resolvedSourceMapId = new Map();

    this.#sourceMapById = new Map();
    this.#sourceMapIdToLoadingClients = new Platform.MapUtilities.Multimap();
    this.#sourceMapIdToClients = new Platform.MapUtilities.Multimap();

    TargetManager.instance().addEventListener(TargetManagerEvents.InspectedURLChanged, this.inspectedURLChanged, this);
  }

  setEnabled(isEnabled: boolean): void {
    if (isEnabled === this.#isEnabled) {
      return;
    }
    this.#isEnabled = isEnabled;
    // We need this copy, because `this.#resolvedSourceMapId` is getting modified
    // in the loop body and trying to iterate over it at the same time leads to
    // an infinite loop.
    const clients = [...this.#resolvedSourceMapId.keys()];
    for (const client of clients) {
      const relativeSourceURL = this.#relativeSourceURL.get(client);
      const relativeSourceMapURL = this.#relativeSourceMapURL.get(client);
      this.detachSourceMap(client);
      this.attachSourceMap(client, relativeSourceURL, relativeSourceMapURL);
    }
  }

  private inspectedURLChanged(event: Common.EventTarget.EventTargetEvent<Target>): void {
    if (event.data !== this.#target) {
      return;
    }

    // We need this copy, because `this.#resolvedSourceMapId` is getting modified
    // in the loop body and trying to iterate over it at the same time leads to
    // an infinite loop.
    const prevSourceMapIds = new Map(this.#resolvedSourceMapId);
    for (const [client, prevSourceMapId] of prevSourceMapIds) {
      const relativeSourceURL = this.#relativeSourceURL.get(client);
      const relativeSourceMapURL = this.#relativeSourceMapURL.get(client);
      if (relativeSourceURL === undefined || relativeSourceMapURL === undefined) {
        continue;
      }
      const resolvedUrls = this.resolveRelativeURLs(relativeSourceURL, relativeSourceMapURL);
      if (resolvedUrls !== null && prevSourceMapId !== resolvedUrls.sourceMapId) {
        this.detachSourceMap(client);
        this.attachSourceMap(client, relativeSourceURL, relativeSourceMapURL);
      }
    }
  }

  sourceMapForClient(client: T): SourceMap|null {
    const sourceMapId = this.#resolvedSourceMapId.get(client);
    if (!sourceMapId) {
      return null;
    }
    return this.#sourceMapById.get(sourceMapId) || null;
  }

  clientsForSourceMap(sourceMap: SourceMap): T[] {
    const sourceMapId = this.getSourceMapId(sourceMap.compiledURL(), sourceMap.url());
    if (this.#sourceMapIdToClients.has(sourceMapId)) {
      return [...this.#sourceMapIdToClients.get(sourceMapId)];
    }
    return [...this.#sourceMapIdToLoadingClients.get(sourceMapId)];
  }

  private getSourceMapId(sourceURL: string, sourceMapURL: string): string {
    return `${sourceURL}:${sourceMapURL}`;
  }

  private resolveRelativeURLs(sourceURL: string, sourceMapURL: string): {
    sourceURL: string,
    sourceMapURL: string,
    sourceMapId: string,
  }|null {
    // |#sourceURL| can be a random string, but is generally an absolute path.
    // Complete it to inspected page url for relative links.
    const resolvedSourceURL = Common.ParsedURL.ParsedURL.completeURL(this.#target.inspectedURL(), sourceURL);
    if (!resolvedSourceURL) {
      return null;
    }
    const resolvedSourceMapURL = Common.ParsedURL.ParsedURL.completeURL(resolvedSourceURL, sourceMapURL);
    if (!resolvedSourceMapURL) {
      return null;
    }
    return {
      sourceURL: resolvedSourceURL,
      sourceMapURL: resolvedSourceMapURL,
      sourceMapId: this.getSourceMapId(resolvedSourceURL, resolvedSourceMapURL),
    };
  }

  attachSourceMap(client: T, relativeSourceURL: string|undefined, relativeSourceMapURL: string|undefined): void {
    // TODO(chromium:1011811): Strengthen the type to obsolte the undefined check once core/sdk/ is fully typescriptified.
    if (relativeSourceURL === undefined || !relativeSourceMapURL) {
      return;
    }
    console.assert(!this.#resolvedSourceMapId.has(client), 'SourceMap is already attached to client');
    const resolvedURLs = this.resolveRelativeURLs(relativeSourceURL, relativeSourceMapURL);
    if (!resolvedURLs) {
      return;
    }
    this.#relativeSourceURL.set(client, relativeSourceURL);
    this.#relativeSourceMapURL.set(client, relativeSourceMapURL);

    const {sourceURL, sourceMapURL, sourceMapId} = resolvedURLs;
    this.#resolvedSourceMapId.set(client, sourceMapId);

    if (!this.#isEnabled) {
      return;
    }

    this.dispatchEventToListeners(Events.SourceMapWillAttach, {client});

    if (this.#sourceMapById.has(sourceMapId)) {
      attach.call(this, sourceMapId, client);
      return;
    }
    if (!this.#sourceMapIdToLoadingClients.has(sourceMapId)) {
      TextSourceMap.load(sourceMapURL, sourceURL, client.createPageResourceLoadInitiator())
          .catch(error => {
            Common.Console.Console.instance().warn(
                i18nString(UIStrings.devtoolsFailedToLoadSourcemapS, {PH1: error.message}));
            return null;
          })
          .then(onSourceMap.bind(this, sourceMapId));
    }
    this.#sourceMapIdToLoadingClients.set(sourceMapId, client);

    function onSourceMap(this: SourceMapManager<T>, sourceMapId: string, sourceMap: SourceMap|null): void {
      this.sourceMapLoadedForTest();
      const clients = this.#sourceMapIdToLoadingClients.get(sourceMapId);
      this.#sourceMapIdToLoadingClients.deleteAll(sourceMapId);
      if (!clients.size) {
        return;
      }
      if (!sourceMap) {
        for (const client of clients) {
          this.dispatchEventToListeners(Events.SourceMapFailedToAttach, {client});
        }
        return;
      }
      this.#sourceMapById.set(sourceMapId, sourceMap);
      for (const client of clients) {
        attach.call(this, sourceMapId, client);
      }
    }

    function attach(this: SourceMapManager<T>, sourceMapId: string, client: T): void {
      this.#sourceMapIdToClients.set(sourceMapId, client);
      const sourceMap = this.#sourceMapById.get(sourceMapId) as SourceMap;
      this.dispatchEventToListeners(Events.SourceMapAttached, {client, sourceMap});
    }
  }

  detachSourceMap(client: T): void {
    const sourceMapId = this.#resolvedSourceMapId.get(client);
    this.#relativeSourceURL.delete(client);
    this.#relativeSourceMapURL.delete(client);
    this.#resolvedSourceMapId.delete(client);

    if (!sourceMapId) {
      return;
    }
    if (!this.#sourceMapIdToClients.hasValue(sourceMapId, client)) {
      if (this.#sourceMapIdToLoadingClients.delete(sourceMapId, client)) {
        this.dispatchEventToListeners(Events.SourceMapFailedToAttach, {client});
      }
      return;
    }
    this.#sourceMapIdToClients.delete(sourceMapId, client);
    const sourceMap = this.#sourceMapById.get(sourceMapId);
    if (!sourceMap) {
      return;
    }
    if (!this.#sourceMapIdToClients.has(sourceMapId)) {
      this.#sourceMapById.delete(sourceMapId);
    }
    this.dispatchEventToListeners(Events.SourceMapDetached, {client, sourceMap});
  }

  private sourceMapLoadedForTest(): void {
  }

  dispose(): void {
    TargetManager.instance().removeEventListener(
        TargetManagerEvents.InspectedURLChanged, this.inspectedURLChanged, this);
  }
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum Events {
  SourceMapWillAttach = 'SourceMapWillAttach',
  SourceMapFailedToAttach = 'SourceMapFailedToAttach',
  SourceMapAttached = 'SourceMapAttached',
  SourceMapDetached = 'SourceMapDetached',
}

export type EventTypes<T extends FrameAssociated> = {
  [Events.SourceMapWillAttach]: {client: T},
  [Events.SourceMapFailedToAttach]: {client: T},
  [Events.SourceMapAttached]: {client: T, sourceMap: SourceMap},
  [Events.SourceMapDetached]: {client: T, sourceMap: SourceMap},
};
