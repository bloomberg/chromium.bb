/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

import * as Common from '../../../../core/common/common.js';
import type * as SDK from '../../../../core/sdk/sdk.js';
import * as UI from '../../legacy.js';
import * as i18n from '../../../../core/i18n/i18n.js';

import type {WindowChangedWithPositionEvent} from './OverviewGrid.js';
import {Events as OverviewGridEvents, OverviewGrid} from './OverviewGrid.js';
import type {Calculator} from './TimelineGrid.js';
import timelineOverviewInfoStyles from './timelineOverviewInfo.css.js';

export class TimelineOverviewPane extends Common.ObjectWrapper.eventMixin<EventTypes, typeof UI.Widget.VBox>(
    UI.Widget.VBox) {
  private readonly overviewCalculator: TimelineOverviewCalculator;
  private readonly overviewGrid: OverviewGrid;
  private readonly cursorArea: HTMLElement;
  private cursorElement: HTMLElement;
  private overviewControls: TimelineOverview[];
  private markers: Map<number, Element>;
  private readonly overviewInfo: OverviewInfo;
  private readonly updateThrottler: Common.Throttler.Throttler;
  private cursorEnabled: boolean;
  private cursorPosition: number;
  private lastWidth: number;
  private windowStartTime: number;
  private windowEndTime: number;
  private muteOnWindowChanged: boolean;

  constructor(prefix: string) {
    super();
    this.element.id = prefix + '-overview-pane';

    this.overviewCalculator = new TimelineOverviewCalculator();
    this.overviewGrid = new OverviewGrid(prefix, this.overviewCalculator);
    this.element.appendChild(this.overviewGrid.element);
    this.cursorArea = this.overviewGrid.element.createChild('div', 'overview-grid-cursor-area');
    this.cursorElement = this.overviewGrid.element.createChild('div', 'overview-grid-cursor-position');
    this.cursorArea.addEventListener('mousemove', this.onMouseMove.bind(this), true);
    this.cursorArea.addEventListener('mouseleave', this.hideCursor.bind(this), true);

    this.overviewGrid.setResizeEnabled(false);
    this.overviewGrid.addEventListener(OverviewGridEvents.WindowChangedWithPosition, this.onWindowChanged, this);
    this.overviewGrid.setClickHandler(this.onClick.bind(this));
    this.overviewControls = [];
    this.markers = new Map();

    this.overviewInfo = new OverviewInfo(this.cursorElement);
    this.updateThrottler = new Common.Throttler.Throttler(100);

    this.cursorEnabled = false;
    this.cursorPosition = 0;
    this.lastWidth = 0;

    this.windowStartTime = 0;
    this.windowEndTime = Infinity;
    this.muteOnWindowChanged = false;
  }

  private onMouseMove(event: Event): void {
    if (!this.cursorEnabled) {
      return;
    }
    const mouseEvent = (event as MouseEvent);
    const target = (event.target as HTMLElement);
    this.cursorPosition = mouseEvent.offsetX + target.offsetLeft;
    this.cursorElement.style.left = this.cursorPosition + 'px';
    this.cursorElement.style.visibility = 'visible';
    void this.overviewInfo.setContent(this.buildOverviewInfo());
  }

  private async buildOverviewInfo(): Promise<DocumentFragment> {
    const document = this.element.ownerDocument;
    const x = this.cursorPosition;
    const elements = await Promise.all(this.overviewControls.map(control => control.overviewInfoPromise(x)));
    const fragment = document.createDocumentFragment();
    const nonNullElements = (elements.filter(element => element !== null) as Element[]);
    fragment.append(...nonNullElements);
    return fragment;
  }

  private hideCursor(): void {
    this.cursorElement.style.visibility = 'hidden';
    this.overviewInfo.hide();
  }

  wasShown(): void {
    this.update();
  }

  willHide(): void {
    this.overviewInfo.hide();
  }

  onResize(): void {
    const width = this.element.offsetWidth;
    if (width === this.lastWidth) {
      return;
    }
    this.lastWidth = width;
    this.scheduleUpdate();
  }

  setOverviewControls(overviewControls: TimelineOverview[]): void {
    for (let i = 0; i < this.overviewControls.length; ++i) {
      this.overviewControls[i].dispose();
    }

    for (let i = 0; i < overviewControls.length; ++i) {
      overviewControls[i].setCalculator(this.overviewCalculator);
      overviewControls[i].show(this.overviewGrid.element);
    }
    this.overviewControls = overviewControls;
    this.update();
  }

  setBounds(minimumBoundary: number, maximumBoundary: number): void {
    this.overviewCalculator.setBounds(minimumBoundary, maximumBoundary);
    this.overviewGrid.setResizeEnabled(true);
    this.cursorEnabled = true;
  }

  setNavStartTimes(navStartTimes: Map<string, SDK.TracingModel.Event>): void {
    this.overviewCalculator.setNavStartTimes(navStartTimes);
  }

  scheduleUpdate(): void {
    void this.updateThrottler.schedule(async () => {
      this.update();
    });
  }

  private update(): void {
    if (!this.isShowing()) {
      return;
    }
    this.overviewCalculator.setDisplayWidth(this.overviewGrid.clientWidth());
    for (let i = 0; i < this.overviewControls.length; ++i) {
      this.overviewControls[i].update();
    }
    this.overviewGrid.updateDividers(this.overviewCalculator);
    this.updateMarkers();
    this.updateWindow();
  }

  setMarkers(markers: Map<number, Element>): void {
    this.markers = markers;
  }

  private updateMarkers(): void {
    const filteredMarkers = new Map<number, Element>();
    for (const time of this.markers.keys()) {
      const marker = this.markers.get(time) as HTMLElement;
      const position = Math.round(this.overviewCalculator.computePosition(time));
      // Limit the number of markers to one per pixel.
      if (filteredMarkers.has(position)) {
        continue;
      }
      filteredMarkers.set(position, marker);
      marker.style.left = position + 'px';
    }
    this.overviewGrid.removeEventDividers();
    this.overviewGrid.addEventDividers([...filteredMarkers.values()]);
  }

  reset(): void {
    this.windowStartTime = 0;
    this.windowEndTime = Infinity;
    this.overviewCalculator.reset();
    this.overviewGrid.reset();
    this.overviewGrid.setResizeEnabled(false);
    this.cursorEnabled = false;
    this.hideCursor();
    this.markers = new Map();
    for (const control of this.overviewControls) {
      control.reset();
    }
    this.overviewInfo.hide();
    this.scheduleUpdate();
  }

  private onClick(event: Event): boolean {
    return this.overviewControls.some(control => control.onClick(event));
  }

  private onWindowChanged(event: Common.EventTarget.EventTargetEvent<WindowChangedWithPositionEvent>): void {
    if (this.muteOnWindowChanged) {
      return;
    }
    // Always use first control as a time converter.
    if (!this.overviewControls.length) {
      return;
    }

    this.windowStartTime = event.data.rawStartValue;
    this.windowEndTime = event.data.rawEndValue;
    const windowTimes = {startTime: this.windowStartTime, endTime: this.windowEndTime};

    this.dispatchEventToListeners(Events.WindowChanged, windowTimes);
  }

  setWindowTimes(startTime: number, endTime: number): void {
    if (startTime === this.windowStartTime && endTime === this.windowEndTime) {
      return;
    }
    this.windowStartTime = startTime;
    this.windowEndTime = endTime;
    this.updateWindow();
    this.dispatchEventToListeners(Events.WindowChanged, {startTime: startTime, endTime: endTime});
  }

  private updateWindow(): void {
    if (!this.overviewControls.length) {
      return;
    }
    const absoluteMin = this.overviewCalculator.minimumBoundary();
    const timeSpan = this.overviewCalculator.maximumBoundary() - absoluteMin;
    const haveRecords = absoluteMin > 0;
    const left = haveRecords && this.windowStartTime ? Math.min((this.windowStartTime - absoluteMin) / timeSpan, 1) : 0;
    const right = haveRecords && this.windowEndTime < Infinity ? (this.windowEndTime - absoluteMin) / timeSpan : 1;
    this.muteOnWindowChanged = true;
    this.overviewGrid.setWindow(left, right);
    this.muteOnWindowChanged = false;
  }
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum Events {
  WindowChanged = 'WindowChanged',
}

export interface WindowChangedEvent {
  startTime: number;
  endTime: number;
}

export type EventTypes = {
  [Events.WindowChanged]: WindowChangedEvent,
};

export class TimelineOverviewCalculator implements Calculator {
  private minimumBoundaryInternal!: number;
  private maximumBoundaryInternal!: number;
  private workingArea!: number;
  private navStartTimes?: Map<string, SDK.TracingModel.Event>;

  constructor() {
    this.reset();
  }

  computePosition(time: number): number {
    return (time - this.minimumBoundaryInternal) / this.boundarySpan() * this.workingArea;
  }

  positionToTime(position: number): number {
    return position / this.workingArea * this.boundarySpan() + this.minimumBoundaryInternal;
  }

  setBounds(minimumBoundary: number, maximumBoundary: number): void {
    this.minimumBoundaryInternal = minimumBoundary;
    this.maximumBoundaryInternal = maximumBoundary;
  }

  setNavStartTimes(navStartTimes: Map<string, SDK.TracingModel.Event>): void {
    this.navStartTimes = navStartTimes;
  }

  setDisplayWidth(clientWidth: number): void {
    this.workingArea = clientWidth;
  }

  reset(): void {
    this.setBounds(0, 100);
  }

  formatValue(value: number, precision?: number): string {
    // If there are nav start times the value needs to be remapped.
    if (this.navStartTimes) {
      // Find the latest possible nav start time which is considered earlier
      // than the value passed through.
      const navStartTimes = Array.from(this.navStartTimes.values());
      for (let i = navStartTimes.length - 1; i >= 0; i--) {
        if (value > navStartTimes[i].startTime) {
          value -= (navStartTimes[i].startTime - this.zeroTime());
          break;
        }
      }
    }

    return i18n.TimeUtilities.preciseMillisToString(value - this.zeroTime(), precision);
  }

  maximumBoundary(): number {
    return this.maximumBoundaryInternal;
  }

  minimumBoundary(): number {
    return this.minimumBoundaryInternal;
  }

  zeroTime(): number {
    return this.minimumBoundaryInternal;
  }

  boundarySpan(): number {
    return this.maximumBoundaryInternal - this.minimumBoundaryInternal;
  }
}

export interface TimelineOverview {
  show(parentElement: Element, insertBefore?: Element|null): void;
  update(): void;
  dispose(): void;
  reset(): void;
  overviewInfoPromise(x: number): Promise<Element|null>;
  onClick(event: Event): boolean;
  setCalculator(calculator: TimelineOverviewCalculator): void;
}

export class TimelineOverviewBase extends UI.Widget.VBox implements TimelineOverview {
  private calculatorInternal: TimelineOverviewCalculator|null;
  private canvas: HTMLCanvasElement;
  private contextInternal: CanvasRenderingContext2D|null;

  constructor() {
    super();
    this.calculatorInternal = null;
    this.canvas = (this.element.createChild('canvas', 'fill') as HTMLCanvasElement);
    this.contextInternal = this.canvas.getContext('2d');
  }

  width(): number {
    return this.canvas.width;
  }

  height(): number {
    return this.canvas.height;
  }

  context(): CanvasRenderingContext2D {
    if (!this.contextInternal) {
      throw new Error('Unable to retrieve canvas context');
    }
    return this.contextInternal as CanvasRenderingContext2D;
  }

  calculator(): TimelineOverviewCalculator|null {
    return this.calculatorInternal;
  }

  update(): void {
    this.resetCanvas();
  }

  dispose(): void {
    this.detach();
  }

  reset(): void {
  }

  async overviewInfoPromise(_x: number): Promise<Element|null> {
    return null;
  }

  setCalculator(calculator: TimelineOverviewCalculator): void {
    this.calculatorInternal = calculator;
  }

  onClick(_event: Event): boolean {
    return false;
  }

  resetCanvas(): void {
    if (this.element.clientWidth) {
      this.setCanvasSize(this.element.clientWidth, this.element.clientHeight);
    }
  }

  setCanvasSize(width: number, height: number): void {
    this.canvas.width = width * window.devicePixelRatio;
    this.canvas.height = height * window.devicePixelRatio;
  }
}

export class OverviewInfo {
  private readonly anchorElement: Element;
  private glassPane: UI.GlassPane.GlassPane;
  private visible: boolean;
  private readonly element: Element;

  constructor(anchor: Element) {
    this.anchorElement = anchor;
    this.glassPane = new UI.GlassPane.GlassPane();
    this.glassPane.setPointerEventsBehavior(UI.GlassPane.PointerEventsBehavior.PierceContents);
    this.glassPane.setMarginBehavior(UI.GlassPane.MarginBehavior.Arrow);
    this.glassPane.setSizeBehavior(UI.GlassPane.SizeBehavior.MeasureContent);
    this.visible = false;
    this.element = UI.Utils
                       .createShadowRootWithCoreStyles(this.glassPane.contentElement, {
                         cssFile: [timelineOverviewInfoStyles],
                         delegatesFocus: undefined,
                       })
                       .createChild('div', 'overview-info');
  }

  async setContent(contentPromise: Promise<DocumentFragment>): Promise<void> {
    this.visible = true;
    const content = await contentPromise;
    if (!this.visible) {
      return;
    }
    this.element.removeChildren();
    this.element.appendChild(content);
    this.glassPane.setContentAnchorBox(this.anchorElement.boxInWindow());
    if (!this.glassPane.isShowing()) {
      this.glassPane.show((this.anchorElement.ownerDocument as Document));
    }
  }

  hide(): void {
    this.visible = false;
    this.glassPane.hide();
  }
}
