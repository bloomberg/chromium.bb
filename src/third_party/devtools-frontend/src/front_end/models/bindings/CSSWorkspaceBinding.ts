// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import type * as Workspace from '../workspace/workspace.js';

import type {LiveLocation as LiveLocationInterface, LiveLocationPool} from './LiveLocation.js';
import {LiveLocationWithPool} from './LiveLocation.js';
import {ResourceMapping} from './ResourceMapping.js';
import {SASSSourceMapping} from './SASSSourceMapping.js';
import {StylesSourceMapping} from './StylesSourceMapping.js';

let cssWorkspaceBindingInstance: CSSWorkspaceBinding|undefined;

export class CSSWorkspaceBinding implements SDK.TargetManager.SDKModelObserver<SDK.CSSModel.CSSModel> {
  private readonly workspace: Workspace.Workspace.WorkspaceImpl;
  private readonly modelToInfo: Map<SDK.CSSModel.CSSModel, ModelInfo>;
  private readonly sourceMappings: SourceMapping[];
  private readonly liveLocationPromises: Set<Promise<unknown>>;

  private constructor(targetManager: SDK.TargetManager.TargetManager, workspace: Workspace.Workspace.WorkspaceImpl) {
    this.workspace = workspace;

    this.modelToInfo = new Map();
    this.sourceMappings = [];
    targetManager.observeModels(SDK.CSSModel.CSSModel, this);

    this.liveLocationPromises = new Set();
  }

  static instance(opts: {
    forceNew: boolean|null,
    targetManager: SDK.TargetManager.TargetManager|null,
    workspace: Workspace.Workspace.WorkspaceImpl|null,
  } = {forceNew: null, targetManager: null, workspace: null}): CSSWorkspaceBinding {
    const {forceNew, targetManager, workspace} = opts;
    if (!cssWorkspaceBindingInstance || forceNew) {
      if (!targetManager || !workspace) {
        throw new Error(
            `Unable to create CSSWorkspaceBinding: targetManager and workspace must be provided: ${new Error().stack}`);
      }

      cssWorkspaceBindingInstance = new CSSWorkspaceBinding(targetManager, workspace);
    }

    return cssWorkspaceBindingInstance;
  }

  static removeInstance(): void {
    cssWorkspaceBindingInstance = undefined;
  }

  private getCSSModelInfo(cssModel: SDK.CSSModel.CSSModel): ModelInfo {
    return this.modelToInfo.get(cssModel) as ModelInfo;
  }

  modelAdded(cssModel: SDK.CSSModel.CSSModel): void {
    this.modelToInfo.set(cssModel, new ModelInfo(cssModel, this.workspace));
  }

  modelRemoved(cssModel: SDK.CSSModel.CSSModel): void {
    this.getCSSModelInfo(cssModel).dispose();
    this.modelToInfo.delete(cssModel);
  }

  /**
   * The promise returned by this function is resolved once all *currently*
   * pending LiveLocations are processed.
   */
  async pendingLiveLocationChangesPromise(): Promise<void> {
    await Promise.all(this.liveLocationPromises);
  }

  private recordLiveLocationChange(promise: Promise<unknown>): void {
    promise.then(() => {
      this.liveLocationPromises.delete(promise);
    });
    this.liveLocationPromises.add(promise);
  }

  async updateLocations(header: SDK.CSSStyleSheetHeader.CSSStyleSheetHeader): Promise<void> {
    const updatePromise = this.getCSSModelInfo(header.cssModel()).updateLocations(header);
    this.recordLiveLocationChange(updatePromise);
    await updatePromise;
  }

  createLiveLocation(
      rawLocation: SDK.CSSModel.CSSLocation, updateDelegate: (arg0: LiveLocationInterface) => Promise<void>,
      locationPool: LiveLocationPool): Promise<LiveLocation> {
    const locationPromise =
        this.getCSSModelInfo(rawLocation.cssModel()).createLiveLocation(rawLocation, updateDelegate, locationPool);
    this.recordLiveLocationChange(locationPromise);
    return locationPromise;
  }

  propertyUILocation(cssProperty: SDK.CSSProperty.CSSProperty, forName: boolean): Workspace.UISourceCode.UILocation
      |null {
    const style = cssProperty.ownerStyle;
    if (!style || style.type !== SDK.CSSStyleDeclaration.Type.Regular || !style.styleSheetId) {
      return null;
    }
    const header = style.cssModel().styleSheetHeaderForId(style.styleSheetId);
    if (!header) {
      return null;
    }

    const range = forName ? cssProperty.nameRange() : cssProperty.valueRange();
    if (!range) {
      return null;
    }

    const lineNumber = range.startLine;
    const columnNumber = range.startColumn;
    const rawLocation = new SDK.CSSModel.CSSLocation(
        header, header.lineNumberInSource(lineNumber), header.columnNumberInSource(lineNumber, columnNumber));
    return this.rawLocationToUILocation(rawLocation);
  }

  rawLocationToUILocation(rawLocation: SDK.CSSModel.CSSLocation): Workspace.UISourceCode.UILocation|null {
    for (let i = this.sourceMappings.length - 1; i >= 0; --i) {
      const uiLocation = this.sourceMappings[i].rawLocationToUILocation(rawLocation);
      if (uiLocation) {
        return uiLocation;
      }
    }
    return this.getCSSModelInfo(rawLocation.cssModel()).rawLocationToUILocation(rawLocation);
  }

  uiLocationToRawLocations(uiLocation: Workspace.UISourceCode.UILocation): SDK.CSSModel.CSSLocation[] {
    for (let i = this.sourceMappings.length - 1; i >= 0; --i) {
      const rawLocations = this.sourceMappings[i].uiLocationToRawLocations(uiLocation);
      if (rawLocations.length) {
        return rawLocations;
      }
    }
    const rawLocations = [];
    for (const modelInfo of this.modelToInfo.values()) {
      rawLocations.push(...modelInfo.uiLocationToRawLocations(uiLocation));
    }
    return rawLocations;
  }

  addSourceMapping(sourceMapping: SourceMapping): void {
    this.sourceMappings.push(sourceMapping);
  }
}

/**
 * @interface
 */
export interface SourceMapping {
  rawLocationToUILocation(rawLocation: SDK.CSSModel.CSSLocation): Workspace.UISourceCode.UILocation|null;

  uiLocationToRawLocations(uiLocation: Workspace.UISourceCode.UILocation): SDK.CSSModel.CSSLocation[];
}

export class ModelInfo {
  private readonly eventListeners: Common.EventTarget.EventDescriptor[];
  private stylesSourceMapping: StylesSourceMapping;
  private sassSourceMapping: SASSSourceMapping;
  private readonly locations: Platform.MapUtilities.Multimap<SDK.CSSStyleSheetHeader.CSSStyleSheetHeader, LiveLocation>;
  private readonly unboundLocations: Platform.MapUtilities.Multimap<string, LiveLocation>;
  constructor(cssModel: SDK.CSSModel.CSSModel, workspace: Workspace.Workspace.WorkspaceImpl) {
    this.eventListeners = [
      cssModel.addEventListener(
          SDK.CSSModel.Events.StyleSheetAdded,
          event => {
            this.styleSheetAdded(event);
          },
          this),
      cssModel.addEventListener(
          SDK.CSSModel.Events.StyleSheetRemoved,
          event => {
            this.styleSheetRemoved(event);
          },
          this),
    ];

    this.stylesSourceMapping = new StylesSourceMapping(cssModel, workspace);
    const sourceMapManager = cssModel.sourceMapManager();
    this.sassSourceMapping = new SASSSourceMapping(cssModel.target(), sourceMapManager, workspace);

    this.locations = new Platform.MapUtilities.Multimap();
    this.unboundLocations = new Platform.MapUtilities.Multimap();
  }

  async createLiveLocation(
      rawLocation: SDK.CSSModel.CSSLocation, updateDelegate: (arg0: LiveLocationInterface) => Promise<void>,
      locationPool: LiveLocationPool): Promise<LiveLocation> {
    const location = new LiveLocation(rawLocation, this, updateDelegate, locationPool);
    const header = rawLocation.header();
    if (header) {
      location.setHeader(header);
      this.locations.set(header, location);
      await location.update();
    } else {
      this.unboundLocations.set(rawLocation.url, location);
    }
    return location;
  }

  disposeLocation(location: LiveLocation): void {
    const header = location.header();
    if (header) {
      this.locations.delete(header, location);
    } else {
      this.unboundLocations.delete(location.url, location);
    }
  }

  updateLocations(header: SDK.CSSStyleSheetHeader.CSSStyleSheetHeader): Promise<void[]> {
    const promises = [];
    for (const location of this.locations.get(header)) {
      promises.push(location.update());
    }
    return Promise.all(promises);
  }

  private async styleSheetAdded(
      event: Common.EventTarget.EventTargetEvent<SDK.CSSStyleSheetHeader.CSSStyleSheetHeader>): Promise<void> {
    const header = event.data;
    if (!header.sourceURL) {
      return;
    }

    const promises = [];
    for (const location of this.unboundLocations.get(header.sourceURL)) {
      location.setHeader(header);
      this.locations.set(header, location);
      promises.push(location.update());
    }
    await Promise.all(promises);
    this.unboundLocations.deleteAll(header.sourceURL);
  }

  private async styleSheetRemoved(
      event: Common.EventTarget.EventTargetEvent<SDK.CSSStyleSheetHeader.CSSStyleSheetHeader>): Promise<void> {
    const header = event.data;
    const promises = [];
    for (const location of this.locations.get(header)) {
      location.setHeader(header);
      this.unboundLocations.set(location.url, location);
      promises.push(location.update());
    }
    await Promise.all(promises);
    this.locations.deleteAll(header);
  }

  rawLocationToUILocation(rawLocation: SDK.CSSModel.CSSLocation): Workspace.UISourceCode.UILocation|null {
    let uiLocation: (Workspace.UISourceCode.UILocation|null)|null = null;
    uiLocation = uiLocation || this.sassSourceMapping.rawLocationToUILocation(rawLocation);
    uiLocation = uiLocation || this.stylesSourceMapping.rawLocationToUILocation(rawLocation);
    uiLocation = uiLocation || ResourceMapping.instance().cssLocationToUILocation(rawLocation);
    return uiLocation;
  }

  uiLocationToRawLocations(uiLocation: Workspace.UISourceCode.UILocation): SDK.CSSModel.CSSLocation[] {
    let rawLocations = this.sassSourceMapping.uiLocationToRawLocations(uiLocation);
    if (rawLocations.length) {
      return rawLocations;
    }
    rawLocations = this.stylesSourceMapping.uiLocationToRawLocations(uiLocation);
    if (rawLocations.length) {
      return rawLocations;
    }
    return ResourceMapping.instance().uiLocationToCSSLocations(uiLocation);
  }

  dispose(): void {
    Common.EventTarget.removeEventListeners(this.eventListeners);
    this.stylesSourceMapping.dispose();
    this.sassSourceMapping.dispose();
  }
}

export class LiveLocation extends LiveLocationWithPool {
  readonly url: string;
  private readonly lineNumber: number;
  private readonly columnNumber: number;
  private readonly info: ModelInfo;
  headerInternal: SDK.CSSStyleSheetHeader.CSSStyleSheetHeader|null;
  constructor(
      rawLocation: SDK.CSSModel.CSSLocation, info: ModelInfo,
      updateDelegate: (arg0: LiveLocationInterface) => Promise<void>, locationPool: LiveLocationPool) {
    super(updateDelegate, locationPool);
    this.url = rawLocation.url;
    this.lineNumber = rawLocation.lineNumber;
    this.columnNumber = rawLocation.columnNumber;
    this.info = info;
    this.headerInternal = null;
  }

  header(): SDK.CSSStyleSheetHeader.CSSStyleSheetHeader|null {
    return this.headerInternal;
  }

  setHeader(header: SDK.CSSStyleSheetHeader.CSSStyleSheetHeader|null): void {
    this.headerInternal = header;
  }

  async uiLocation(): Promise<Workspace.UISourceCode.UILocation|null> {
    if (!this.headerInternal) {
      return null;
    }
    const rawLocation = new SDK.CSSModel.CSSLocation(this.headerInternal, this.lineNumber, this.columnNumber);
    return CSSWorkspaceBinding.instance().rawLocationToUILocation(rawLocation);
  }

  dispose(): void {
    super.dispose();
    this.info.disposeLocation(this);
  }

  async isIgnoreListed(): Promise<boolean> {
    return false;
  }
}
