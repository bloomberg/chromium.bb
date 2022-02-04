// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Protocol from '../../generated/protocol.js';
import * as Common from '../common/common.js';

import {OverlayColorGenerator} from './OverlayColorGenerator.js';

export class OverlayPersistentHighlighter {
  readonly #model: OverlayModel;
  readonly #gridHighlights: Map<Protocol.DOM.NodeId, Protocol.Overlay.GridHighlightConfig>;
  readonly #scrollSnapHighlights: Map<Protocol.DOM.NodeId, Protocol.Overlay.ScrollSnapContainerHighlightConfig>;
  readonly #flexHighlights: Map<Protocol.DOM.NodeId, Protocol.Overlay.FlexContainerHighlightConfig>;
  readonly #containerQueryHighlights: Map<Protocol.DOM.NodeId, Protocol.Overlay.ContainerQueryContainerHighlightConfig>;
  readonly #isolatedElementHighlights: Map<Protocol.DOM.NodeId, Protocol.Overlay.IsolationModeHighlightConfig>;
  readonly #colors: Map<Protocol.DOM.NodeId, Common.Color.Color>;
  #gridColorGenerator: OverlayColorGenerator;
  #flexColorGenerator: OverlayColorGenerator;
  #flexEnabled: boolean;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  readonly #showGridLineLabelsSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  readonly #extendGridLinesSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  readonly #showGridAreasSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  readonly #showGridTrackSizesSetting: Common.Settings.Setting<any>;
  constructor(model: OverlayModel, flexEnabled: boolean = true) {
    this.#model = model;

    this.#gridHighlights = new Map();

    this.#scrollSnapHighlights = new Map();

    this.#flexHighlights = new Map();

    this.#containerQueryHighlights = new Map();

    this.#isolatedElementHighlights = new Map();

    this.#colors = new Map();

    this.#gridColorGenerator = new OverlayColorGenerator();
    this.#flexColorGenerator = new OverlayColorGenerator();
    this.#flexEnabled = flexEnabled;

    this.#showGridLineLabelsSetting = Common.Settings.Settings.instance().moduleSetting('showGridLineLabels');
    this.#showGridLineLabelsSetting.addChangeListener(this.onSettingChange, this);
    this.#extendGridLinesSetting = Common.Settings.Settings.instance().moduleSetting('extendGridLines');
    this.#extendGridLinesSetting.addChangeListener(this.onSettingChange, this);
    this.#showGridAreasSetting = Common.Settings.Settings.instance().moduleSetting('showGridAreas');
    this.#showGridAreasSetting.addChangeListener(this.onSettingChange, this);
    this.#showGridTrackSizesSetting = Common.Settings.Settings.instance().moduleSetting('showGridTrackSizes');
    this.#showGridTrackSizesSetting.addChangeListener(this.onSettingChange, this);
  }

  private onSettingChange(): void {
    this.resetOverlay();
  }

  private buildGridHighlightConfig(nodeId: Protocol.DOM.NodeId): Protocol.Overlay.GridHighlightConfig {
    const mainColor = this.colorOfGrid(nodeId);
    const background = mainColor.setAlpha(0.1);
    const gapBackground = mainColor.setAlpha(0.3);
    const gapHatch = mainColor.setAlpha(0.8);

    const showGridExtensionLines = (this.#extendGridLinesSetting.get() as boolean);
    const showPositiveLineNumbers = this.#showGridLineLabelsSetting.get() === 'lineNumbers';
    const showNegativeLineNumbers = showPositiveLineNumbers;
    const showLineNames = this.#showGridLineLabelsSetting.get() === 'lineNames';
    return {
      rowGapColor: gapBackground.toProtocolRGBA(),
      rowHatchColor: gapHatch.toProtocolRGBA(),
      columnGapColor: gapBackground.toProtocolRGBA(),
      columnHatchColor: gapHatch.toProtocolRGBA(),
      gridBorderColor: mainColor.toProtocolRGBA(),
      gridBorderDash: false,
      rowLineColor: mainColor.toProtocolRGBA(),
      columnLineColor: mainColor.toProtocolRGBA(),
      rowLineDash: true,
      columnLineDash: true,
      showGridExtensionLines,
      showPositiveLineNumbers,
      showNegativeLineNumbers,
      showLineNames,
      showAreaNames: (this.#showGridAreasSetting.get() as boolean),
      showTrackSizes: (this.#showGridTrackSizesSetting.get() as boolean),
      areaBorderColor: mainColor.toProtocolRGBA(),
      gridBackgroundColor: background.toProtocolRGBA(),
    };
  }

  private buildFlexContainerHighlightConfig(nodeId: Protocol.DOM.NodeId):
      Protocol.Overlay.FlexContainerHighlightConfig {
    const mainColor = this.colorOfFlex(nodeId);
    return {
      containerBorder: {color: mainColor.toProtocolRGBA(), pattern: Protocol.Overlay.LineStylePattern.Dashed},
      itemSeparator: {color: mainColor.toProtocolRGBA(), pattern: Protocol.Overlay.LineStylePattern.Dotted},
      lineSeparator: {color: mainColor.toProtocolRGBA(), pattern: Protocol.Overlay.LineStylePattern.Dashed},
      mainDistributedSpace: {hatchColor: mainColor.toProtocolRGBA()},
      crossDistributedSpace: {hatchColor: mainColor.toProtocolRGBA()},
    };
  }

  private buildScrollSnapContainerHighlightConfig(_nodeId: number):
      Protocol.Overlay.ScrollSnapContainerHighlightConfig {
    return {
      snapAreaBorder: {
        color: Common.Color.PageHighlight.GridBorder.toProtocolRGBA(),
        pattern: Protocol.Overlay.LineStylePattern.Dashed,
      },
      snapportBorder: {color: Common.Color.PageHighlight.GridBorder.toProtocolRGBA()},
      scrollMarginColor: Common.Color.PageHighlight.Margin.toProtocolRGBA(),
      scrollPaddingColor: Common.Color.PageHighlight.Padding.toProtocolRGBA(),
    };
  }

  highlightGridInOverlay(nodeId: Protocol.DOM.NodeId): void {
    this.#gridHighlights.set(nodeId, this.buildGridHighlightConfig(nodeId));
    this.updateHighlightsInOverlay();
  }

  isGridHighlighted(nodeId: Protocol.DOM.NodeId): boolean {
    return this.#gridHighlights.has(nodeId);
  }

  colorOfGrid(nodeId: Protocol.DOM.NodeId): Common.Color.Color {
    let color = this.#colors.get(nodeId);
    if (!color) {
      color = this.#gridColorGenerator.next();
      this.#colors.set(nodeId, color);
    }

    return color;
  }

  setColorOfGrid(nodeId: Protocol.DOM.NodeId, color: Common.Color.Color): void {
    this.#colors.set(nodeId, color);
  }

  hideGridInOverlay(nodeId: Protocol.DOM.NodeId): void {
    if (this.#gridHighlights.has(nodeId)) {
      this.#gridHighlights.delete(nodeId);
      this.updateHighlightsInOverlay();
    }
  }

  highlightScrollSnapInOverlay(nodeId: Protocol.DOM.NodeId): void {
    this.#scrollSnapHighlights.set(nodeId, this.buildScrollSnapContainerHighlightConfig(nodeId));
    this.updateHighlightsInOverlay();
  }

  isScrollSnapHighlighted(nodeId: Protocol.DOM.NodeId): boolean {
    return this.#scrollSnapHighlights.has(nodeId);
  }

  hideScrollSnapInOverlay(nodeId: Protocol.DOM.NodeId): void {
    if (this.#scrollSnapHighlights.has(nodeId)) {
      this.#scrollSnapHighlights.delete(nodeId);
      this.updateHighlightsInOverlay();
    }
  }

  highlightFlexInOverlay(nodeId: Protocol.DOM.NodeId): void {
    this.#flexHighlights.set(nodeId, this.buildFlexContainerHighlightConfig(nodeId));
    this.updateHighlightsInOverlay();
  }

  isFlexHighlighted(nodeId: Protocol.DOM.NodeId): boolean {
    return this.#flexHighlights.has(nodeId);
  }

  colorOfFlex(nodeId: Protocol.DOM.NodeId): Common.Color.Color {
    let color = this.#colors.get(nodeId);
    if (!color) {
      color = this.#flexColorGenerator.next();
      this.#colors.set(nodeId, color);
    }

    return color;
  }

  setColorOfFlex(nodeId: Protocol.DOM.NodeId, color: Common.Color.Color): void {
    this.#colors.set(nodeId, color);
  }

  hideFlexInOverlay(nodeId: Protocol.DOM.NodeId): void {
    if (this.#flexHighlights.has(nodeId)) {
      this.#flexHighlights.delete(nodeId);
      this.updateHighlightsInOverlay();
    }
  }

  highlightContainerQueryInOverlay(nodeId: Protocol.DOM.NodeId): void {
    this.#containerQueryHighlights.set(nodeId, this.buildContainerQueryContainerHighlightConfig());
    this.updateHighlightsInOverlay();
  }

  hideContainerQueryInOverlay(nodeId: Protocol.DOM.NodeId): void {
    if (this.#containerQueryHighlights.has(nodeId)) {
      this.#containerQueryHighlights.delete(nodeId);
      this.updateHighlightsInOverlay();
    }
  }

  isContainerQueryHighlighted(nodeId: Protocol.DOM.NodeId): boolean {
    return this.#containerQueryHighlights.has(nodeId);
  }

  private buildContainerQueryContainerHighlightConfig(): Protocol.Overlay.ContainerQueryContainerHighlightConfig {
    return {
      containerBorder: {
        color: Common.Color.PageHighlight.LayoutLine.toProtocolRGBA(),
        pattern: Protocol.Overlay.LineStylePattern.Dashed,
      },
      descendantBorder: {
        color: Common.Color.PageHighlight.LayoutLine.toProtocolRGBA(),
        pattern: Protocol.Overlay.LineStylePattern.Dashed,
      },
    };
  }

  highlightIsolatedElementInOverlay(nodeId: Protocol.DOM.NodeId): void {
    this.#isolatedElementHighlights.set(nodeId, this.buildIsolationModeHighlightConfig());
    this.updateHighlightsInOverlay();
  }

  hideIsolatedElementInOverlay(nodeId: Protocol.DOM.NodeId): void {
    if (this.#isolatedElementHighlights.has(nodeId)) {
      this.#isolatedElementHighlights.delete(nodeId);
      this.updateHighlightsInOverlay();
    }
  }

  isIsolatedElementHighlighted(nodeId: Protocol.DOM.NodeId): boolean {
    return this.#isolatedElementHighlights.has(nodeId);
  }

  private buildIsolationModeHighlightConfig(): Protocol.Overlay.IsolationModeHighlightConfig {
    return {
      resizerColor: Common.Color.IsolationModeHighlight.Resizer.toProtocolRGBA(),
      resizerHandleColor: Common.Color.IsolationModeHighlight.ResizerHandle.toProtocolRGBA(),
      maskColor: Common.Color.IsolationModeHighlight.Mask.toProtocolRGBA(),
    };
  }

  hideAllInOverlay(): void {
    this.#flexHighlights.clear();
    this.#gridHighlights.clear();
    this.#scrollSnapHighlights.clear();
    this.#containerQueryHighlights.clear();
    this.#isolatedElementHighlights.clear();
    this.updateHighlightsInOverlay();
  }

  refreshHighlights(): void {
    const gridsNeedUpdate = this.updateHighlightsForDeletedNodes(this.#gridHighlights);
    const flexboxesNeedUpdate = this.updateHighlightsForDeletedNodes(this.#flexHighlights);
    const scrollSnapsNeedUpdate = this.updateHighlightsForDeletedNodes(this.#scrollSnapHighlights);
    const containerQueriesNeedUpdate = this.updateHighlightsForDeletedNodes(this.#containerQueryHighlights);
    const isolatedElementsNeedUpdate = this.updateHighlightsForDeletedNodes(this.#isolatedElementHighlights);
    if (flexboxesNeedUpdate || gridsNeedUpdate || scrollSnapsNeedUpdate || containerQueriesNeedUpdate ||
        isolatedElementsNeedUpdate) {
      this.updateHighlightsInOverlay();
    }
  }

  private updateHighlightsForDeletedNodes(highlights: Map<Protocol.DOM.NodeId, unknown>): boolean {
    let needsUpdate = false;
    for (const nodeId of highlights.keys()) {
      if (this.#model.getDOMModel().nodeForId(nodeId) === null) {
        highlights.delete(nodeId);
        needsUpdate = true;
      }
    }
    return needsUpdate;
  }

  resetOverlay(): void {
    for (const nodeId of this.#gridHighlights.keys()) {
      this.#gridHighlights.set(nodeId, this.buildGridHighlightConfig(nodeId));
    }
    for (const nodeId of this.#flexHighlights.keys()) {
      this.#flexHighlights.set(nodeId, this.buildFlexContainerHighlightConfig(nodeId));
    }
    for (const nodeId of this.#scrollSnapHighlights.keys()) {
      this.#scrollSnapHighlights.set(nodeId, this.buildScrollSnapContainerHighlightConfig(nodeId));
    }
    for (const nodeId of this.#containerQueryHighlights.keys()) {
      this.#containerQueryHighlights.set(nodeId, this.buildContainerQueryContainerHighlightConfig());
    }
    for (const nodeId of this.#isolatedElementHighlights.keys()) {
      this.#isolatedElementHighlights.set(nodeId, this.buildIsolationModeHighlightConfig());
    }
    this.updateHighlightsInOverlay();
  }

  private updateHighlightsInOverlay(): void {
    const hasNodesToHighlight = this.#gridHighlights.size > 0 || this.#flexHighlights.size > 0 ||
        this.#containerQueryHighlights.size > 0 || this.#isolatedElementHighlights.size > 0;
    this.#model.setShowViewportSizeOnResize(!hasNodesToHighlight);
    this.updateGridHighlightsInOverlay();
    this.updateFlexHighlightsInOverlay();
    this.updateScrollSnapHighlightsInOverlay();
    this.updateContainerQueryHighlightsInOverlay();
    this.updateIsolatedElementHighlightsInOverlay();
  }

  private updateGridHighlightsInOverlay(): void {
    const overlayModel = this.#model;
    const gridNodeHighlightConfigs = [];
    for (const [nodeId, gridHighlightConfig] of this.#gridHighlights.entries()) {
      gridNodeHighlightConfigs.push({nodeId, gridHighlightConfig});
    }
    overlayModel.target().overlayAgent().invoke_setShowGridOverlays({gridNodeHighlightConfigs});
  }

  private updateFlexHighlightsInOverlay(): void {
    if (!this.#flexEnabled) {
      return;
    }
    const overlayModel = this.#model;
    const flexNodeHighlightConfigs = [];
    for (const [nodeId, flexContainerHighlightConfig] of this.#flexHighlights.entries()) {
      flexNodeHighlightConfigs.push({nodeId, flexContainerHighlightConfig});
    }
    overlayModel.target().overlayAgent().invoke_setShowFlexOverlays({flexNodeHighlightConfigs});
  }

  private updateScrollSnapHighlightsInOverlay(): void {
    const overlayModel = this.#model;
    const scrollSnapHighlightConfigs = [];
    for (const [nodeId, scrollSnapContainerHighlightConfig] of this.#scrollSnapHighlights.entries()) {
      scrollSnapHighlightConfigs.push({nodeId, scrollSnapContainerHighlightConfig});
    }
    overlayModel.target().overlayAgent().invoke_setShowScrollSnapOverlays({scrollSnapHighlightConfigs});
  }

  updateContainerQueryHighlightsInOverlay(): void {
    const overlayModel = this.#model;
    const containerQueryHighlightConfigs = [];
    for (const [nodeId, containerQueryContainerHighlightConfig] of this.#containerQueryHighlights.entries()) {
      containerQueryHighlightConfigs.push({nodeId, containerQueryContainerHighlightConfig});
    }
    overlayModel.target().overlayAgent().invoke_setShowContainerQueryOverlays({containerQueryHighlightConfigs});
  }

  updateIsolatedElementHighlightsInOverlay(): void {
    const overlayModel = this.#model;
    const isolatedElementHighlightConfigs = [];
    for (const [nodeId, isolationModeHighlightConfig] of this.#isolatedElementHighlights.entries()) {
      isolatedElementHighlightConfigs.push({nodeId, isolationModeHighlightConfig});
    }
    overlayModel.target().overlayAgent().invoke_setShowIsolatedElements({isolatedElementHighlightConfigs});
  }
}

export interface DOMModel {
  nodeForId(nodeId: Protocol.DOM.NodeId): void;
}
export interface OverlayAgent {
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  invoke_setShowGridOverlays(param: {
    gridNodeHighlightConfigs: Array<{
      nodeId: number,
      gridHighlightConfig: Protocol.Overlay.GridHighlightConfig,
    }>,
  }): void;

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  invoke_setShowFlexOverlays(param: {
    flexNodeHighlightConfigs: Array<{
      nodeId: number,
      flexContainerHighlightConfig: Protocol.Overlay.FlexContainerHighlightConfig,
    }>,
  }): void;

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  invoke_setShowScrollSnapOverlays(param: {
    scrollSnapHighlightConfigs: Array<{
      nodeId: number,
    }>,
  }): void;

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  invoke_setShowContainerQueryOverlays(param: {
    containerQueryHighlightConfigs: Array<{
      nodeId: number,
      containerQueryContainerHighlightConfig: Protocol.Overlay.ContainerQueryContainerHighlightConfig,
    }>,
  }): void;

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  invoke_setShowIsolatedElements(param: {
    isolatedElementHighlightConfigs: Array<{
      nodeId: number,
      isolationModeHighlightConfig: Protocol.Overlay.IsolationModeHighlightConfig,
    }>,
  }): void;
}

export interface Target {
  overlayAgent(): OverlayAgent;
}

export interface OverlayModel {
  getDOMModel(): DOMModel;

  target(): Target;

  setShowViewportSizeOnResize(value: boolean): void;
}
