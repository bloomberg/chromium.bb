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

import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import type * as SDK from '../../core/sdk/sdk.js';
import * as TimelineModel from '../../models/timeline_model/timeline_model.js';
import * as PerfUI from '../../ui/legacy/components/perf_ui/perf_ui.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Coverage from '../coverage/coverage.js';
import * as Protocol from '../../generated/protocol.js';

import type {PerformanceModel} from './PerformanceModel.js';
import type {EventDispatchTypeDescriptor, TimelineCategory} from './TimelineUIUtils.js';
import {TimelineUIUtils} from './TimelineUIUtils.js';

const UIStrings = {
  /**
  *@description Short for Network. Label for the network requests section of the Performance panel.
  */
  net: 'NET',
  /**
  *@description Text in Timeline Event Overview of the Performance panel
  */
  cpu: 'CPU',
  /**
  *@description Text in Timeline Event Overview of the Performance panel
  */
  fps: 'FPS',
  /**
  *@description Text in Timeline Event Overview of the Performance panel
  */
  heap: 'HEAP',
  /**
  *@description Heap size label text content in Timeline Event Overview of the Performance panel
  *@example {10 MB} PH1
  *@example {30 MB} PH2
  */
  sSDash: '{PH1} – {PH2}',
  /**
  *@description Text in Timeline Event Overview of the Performance panel
  */
  coverage: 'COVERAGE',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/TimelineEventOverview.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export class TimelineEventOverview extends PerfUI.TimelineOverviewPane.TimelineOverviewBase {
  protected model: PerformanceModel|null;
  constructor(id: string, title: string|null) {
    super();
    this.element.id = 'timeline-overview-' + id;
    this.element.classList.add('overview-strip');
    this.model = null;
    if (title) {
      this.element.createChild('div', 'timeline-overview-strip-title').textContent = title;
    }
  }

  setModel(model: PerformanceModel|null): void {
    this.model = model;
  }

  renderBar(begin: number, end: number, position: number, height: number, color: string): void {
    const x = begin;
    const width = end - begin;
    const ctx = this.context();
    ctx.fillStyle = color;
    ctx.fillRect(x, position, width, height);
  }
}

export class TimelineEventOverviewInput extends TimelineEventOverview {
  constructor() {
    super('input', null);
  }

  update(): void {
    super.update();
    if (!this.model) {
      return;
    }
    const height = this.height();
    const descriptors = TimelineUIUtils.eventDispatchDesciptors();
    const descriptorsByType = new Map<string, EventDispatchTypeDescriptor>();
    let maxPriority = -1;
    for (const descriptor of descriptors) {
      for (const type of descriptor.eventTypes) {
        descriptorsByType.set(type, descriptor);
      }
      maxPriority = Math.max(maxPriority, descriptor.priority);
    }

    const minWidth = 2 * window.devicePixelRatio;
    const timeOffset = this.model.timelineModel().minimumRecordTime();
    const timeSpan = this.model.timelineModel().maximumRecordTime() - timeOffset;
    const canvasWidth = this.width();
    const scale = canvasWidth / timeSpan;

    for (let priority = 0; priority <= maxPriority; ++priority) {
      for (const track of this.model.timelineModel().tracks()) {
        for (let i = 0; i < track.events.length; ++i) {
          const event = track.events[i];
          if (event.name !== TimelineModel.TimelineModel.RecordType.EventDispatch) {
            continue;
          }
          const descriptor = descriptorsByType.get(event.args['data']['type']);
          if (!descriptor || descriptor.priority !== priority) {
            continue;
          }
          if (event.endTime === undefined) {
            continue;
          }
          const start =
              Platform.NumberUtilities.clamp(Math.floor((event.startTime - timeOffset) * scale), 0, canvasWidth);
          const end = Platform.NumberUtilities.clamp(Math.ceil((event.endTime - timeOffset) * scale), 0, canvasWidth);
          const width = Math.max(end - start, minWidth);
          this.renderBar(start, start + width, 0, height, descriptor.color);
        }
      }
    }
  }
}

export class TimelineEventOverviewNetwork extends TimelineEventOverview {
  constructor() {
    super('network', i18nString(UIStrings.net));
  }

  update(): void {
    super.update();
    if (!this.model) {
      return;
    }
    const timelineModel = this.model.timelineModel();
    const bandHeight = this.height() / 2;
    const timeOffset = timelineModel.minimumRecordTime();
    const timeSpan = timelineModel.maximumRecordTime() - timeOffset;
    const canvasWidth = this.width();
    const scale = canvasWidth / timeSpan;
    const highPath = new Path2D();
    const lowPath = new Path2D();
    const highPrioritySet = new Set([
      Protocol.Network.ResourcePriority.VeryHigh,
      Protocol.Network.ResourcePriority.High,
      Protocol.Network.ResourcePriority.Medium,
    ]);
    for (const request of timelineModel.networkRequests()) {
      const path = highPrioritySet.has(request.priority) ? highPath : lowPath;
      const s = Math.max(Math.floor((request.startTime - timeOffset) * scale), 0);
      const e = Math.min(Math.ceil((request.endTime - timeOffset) * scale + 1), canvasWidth);
      path.rect(s, 0, e - s, bandHeight - 1);
    }
    const ctx = this.context();
    ctx.save();
    ctx.fillStyle = 'hsl(214, 60%, 60%)';
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    ctx.fill((highPath as any));
    ctx.translate(0, bandHeight);
    ctx.fillStyle = 'hsl(214, 80%, 80%)';
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    ctx.fill((lowPath as any));
    ctx.restore();
  }
}

const categoryToIndex = new WeakMap<TimelineCategory, number>();

export class TimelineEventOverviewCPUActivity extends TimelineEventOverview {
  private backgroundCanvas: HTMLCanvasElement;
  constructor() {
    super('cpu-activity', i18nString(UIStrings.cpu));
    this.backgroundCanvas = (this.element.createChild('canvas', 'fill background') as HTMLCanvasElement);
  }

  resetCanvas(): void {
    super.resetCanvas();
    this.backgroundCanvas.width = this.element.clientWidth * window.devicePixelRatio;
    this.backgroundCanvas.height = this.element.clientHeight * window.devicePixelRatio;
  }

  update(): void {
    super.update();
    if (!this.model) {
      return;
    }
    const timelineModel = this.model.timelineModel();
    const quantSizePx = 4 * window.devicePixelRatio;
    const width = this.width();
    const height = this.height();
    const baseLine = height;
    const timeOffset = timelineModel.minimumRecordTime();
    const timeSpan = timelineModel.maximumRecordTime() - timeOffset;
    const scale = width / timeSpan;
    const quantTime = quantSizePx / scale;
    const categories = TimelineUIUtils.categories();
    const categoryOrder = TimelineUIUtils.getTimelineMainEventCategories();
    const otherIndex = categoryOrder.indexOf('other');
    const idleIndex = 0;
    console.assert(idleIndex === categoryOrder.indexOf('idle'));
    for (let i = idleIndex + 1; i < categoryOrder.length; ++i) {
      categoryToIndex.set(categories[categoryOrder[i]], i);
    }

    const backgroundContext = (this.backgroundCanvas.getContext('2d') as CanvasRenderingContext2D | null);
    if (!backgroundContext) {
      throw new Error('Could not find 2d canvas');
    }
    for (const track of timelineModel.tracks()) {
      if (track.type === TimelineModel.TimelineModel.TrackType.MainThread && track.forMainFrame) {
        drawThreadEvents(this.context(), track.events);
      } else {
        drawThreadEvents(backgroundContext, track.events);
      }
    }
    applyPattern(backgroundContext);

    function drawThreadEvents(ctx: CanvasRenderingContext2D, events: SDK.TracingModel.Event[]): void {
      const quantizer = new Quantizer(timeOffset, quantTime, drawSample);
      let x = 0;
      const categoryIndexStack: number[] = [];
      const paths: Path2D[] = [];
      const lastY: number[] = [];
      for (let i = 0; i < categoryOrder.length; ++i) {
        paths[i] = new Path2D();
        paths[i].moveTo(0, height);
        lastY[i] = height;
      }

      function drawSample(counters: number[]): void {
        let y = baseLine;
        for (let i = idleIndex + 1; i < categoryOrder.length; ++i) {
          const h = (counters[i] || 0) / quantTime * height;
          y -= h;
          paths[i].bezierCurveTo(x, lastY[i], x, y, x + quantSizePx / 2, y);
          lastY[i] = y;
        }
        x += quantSizePx;
      }

      function onEventStart(e: SDK.TracingModel.Event): void {
        const index = categoryIndexStack.length ? categoryIndexStack[categoryIndexStack.length - 1] : idleIndex;
        quantizer.appendInterval(e.startTime, (index as number));
        categoryIndexStack.push(categoryToIndex.get(TimelineUIUtils.eventStyle(e).category) || otherIndex);
      }

      function onEventEnd(e: SDK.TracingModel.Event): void {
        const lastCategoryIndex = categoryIndexStack.pop();
        if (e.endTime !== undefined && lastCategoryIndex) {
          quantizer.appendInterval(e.endTime, lastCategoryIndex);
        }
      }

      TimelineModel.TimelineModel.TimelineModelImpl.forEachEvent(events, onEventStart, onEventEnd);
      quantizer.appendInterval(timeOffset + timeSpan + quantTime, idleIndex);  // Kick drawing the last bucket.
      for (let i = categoryOrder.length - 1; i > 0; --i) {
        paths[i].lineTo(width, height);
        ctx.fillStyle = categories[categoryOrder[i]].color;
        ctx.fill(paths[i]);
      }
    }

    function applyPattern(ctx: CanvasRenderingContext2D): void {
      const step = 4 * window.devicePixelRatio;
      ctx.save();
      ctx.lineWidth = step / Math.sqrt(8);
      for (let x = 0.5; x < width + height; x += step) {
        ctx.moveTo(x, 0);
        ctx.lineTo(x - height, height);
      }
      ctx.globalCompositeOperation = 'destination-out';
      ctx.stroke();
      ctx.restore();
    }
  }
}

export class TimelineEventOverviewResponsiveness extends TimelineEventOverview {
  constructor() {
    super('responsiveness', null);
  }

  update(): void {
    super.update();
    if (!this.model) {
      return;
    }
    const height = this.height();

    const timeOffset = this.model.timelineModel().minimumRecordTime();
    const timeSpan = this.model.timelineModel().maximumRecordTime() - timeOffset;
    const scale = this.width() / timeSpan;
    const frames = this.model.frames();
    const ctx = this.context();
    const fillPath = new Path2D();
    const markersPath = new Path2D();
    for (let i = 0; i < frames.length; ++i) {
      const frame = frames[i];
      if (!frame.hasWarnings()) {
        continue;
      }
      paintWarningDecoration(frame.startTime, frame.duration);
    }

    for (const track of this.model.timelineModel().tracks()) {
      const events = track.events;
      for (let i = 0; i < events.length; ++i) {
        if (!TimelineModel.TimelineModel.TimelineData.forEvent(events[i]).warning) {
          continue;
        }
        const duration = events[i].duration;
        if (duration !== undefined) {
          paintWarningDecoration(events[i].startTime, duration);
        }
      }
    }

    ctx.fillStyle = 'hsl(0, 80%, 90%)';
    ctx.strokeStyle = 'red';
    ctx.lineWidth = 2 * window.devicePixelRatio;
    ctx.fill(fillPath);
    ctx.stroke(markersPath);

    function paintWarningDecoration(time: number, duration: number): void {
      const x = Math.round(scale * (time - timeOffset));
      const w = Math.round(scale * duration);
      fillPath.rect(x, 0, w, height);
      markersPath.moveTo(x + w, 0);
      markersPath.lineTo(x + w, height);
    }
  }
}

export class TimelineFilmStripOverview extends TimelineEventOverview {
  private frameToImagePromise: Map<SDK.FilmStripModel.Frame, Promise<HTMLImageElement>>;
  private lastFrame: SDK.FilmStripModel.Frame|null;
  private lastElement: Element|null;
  private drawGeneration?: symbol;
  private emptyImage?: HTMLImageElement;
  private imageWidth?: number;

  constructor() {
    super('filmstrip', null);
    this.frameToImagePromise = new Map();
    this.lastFrame = null;
    this.lastElement = null;
    this.reset();
  }

  update(): void {
    super.update();
    const frames = this.model ? this.model.filmStripModel().frames() : [];
    if (!frames.length) {
      return;
    }

    const drawGeneration = Symbol('drawGeneration');
    this.drawGeneration = drawGeneration;
    void this.imageByFrame(frames[0]).then(image => {
      if (this.drawGeneration !== drawGeneration) {
        return;
      }
      if (!image || !image.naturalWidth || !image.naturalHeight) {
        return;
      }
      const imageHeight = this.height() - 2 * TimelineFilmStripOverview.Padding;
      const imageWidth = Math.ceil(imageHeight * image.naturalWidth / image.naturalHeight);
      const popoverScale = Math.min(200 / image.naturalWidth, 1);
      this.emptyImage = new Image(image.naturalWidth * popoverScale, image.naturalHeight * popoverScale);
      this.drawFrames(imageWidth, imageHeight);
    });
  }

  private async imageByFrame(frame: SDK.FilmStripModel.Frame): Promise<HTMLImageElement|null> {
    let imagePromise: Promise<HTMLImageElement|null>|undefined = this.frameToImagePromise.get(frame);
    if (!imagePromise) {
      const data = await frame.imageDataPromise();
      imagePromise = UI.UIUtils.loadImageFromData(data);
      this.frameToImagePromise.set(frame, (imagePromise as Promise<HTMLImageElement>));
    }
    return imagePromise;
  }

  private drawFrames(imageWidth: number, imageHeight: number): void {
    if (!imageWidth || !this.model) {
      return;
    }
    const filmStripModel = this.model.filmStripModel();
    if (!filmStripModel.frames().length) {
      return;
    }
    const padding = TimelineFilmStripOverview.Padding;
    const width = this.width();
    const zeroTime = filmStripModel.zeroTime();
    const spanTime = filmStripModel.spanTime();
    const scale = spanTime / width;
    const context = this.context();
    const drawGeneration = this.drawGeneration;

    context.beginPath();
    for (let x = padding; x < width; x += imageWidth + 2 * padding) {
      const time = zeroTime + (x + imageWidth / 2) * scale;
      const frame = filmStripModel.frameByTimestamp(time);
      if (!frame) {
        continue;
      }
      context.rect(x - 0.5, 0.5, imageWidth + 1, imageHeight + 1);
      void this.imageByFrame(frame).then(drawFrameImage.bind(this, x));
    }
    context.strokeStyle = '#ddd';
    context.stroke();

    function drawFrameImage(this: TimelineFilmStripOverview, x: number, image: HTMLImageElement|null): void {
      // Ignore draws deferred from a previous update call.
      if (this.drawGeneration !== drawGeneration || !image) {
        return;
      }
      context.drawImage(image, x, 1, imageWidth, imageHeight);
    }
  }

  async overviewInfoPromise(x: number): Promise<Element|null> {
    if (!this.model || !this.model.filmStripModel().frames().length) {
      return null;
    }

    const calculator = this.calculator();
    if (!calculator) {
      return null;
    }
    const time = calculator.positionToTime(x);
    const frame = this.model.filmStripModel().frameByTimestamp(time);
    if (frame === this.lastFrame) {
      return this.lastElement;
    }
    const imagePromise = frame ? this.imageByFrame(frame) : Promise.resolve(this.emptyImage);
    const image = await imagePromise;
    const element = document.createElement('div');
    element.classList.add('frame');
    if (image) {
      element.createChild('div', 'thumbnail').appendChild(image);
    }
    this.lastFrame = frame;
    this.lastElement = element;
    return element;
  }

  reset(): void {
    this.lastFrame = null;
    this.lastElement = null;
    this.frameToImagePromise = new Map();
    this.imageWidth = 0;
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/naming-convention
  static readonly Padding = 2;
}

export class TimelineEventOverviewFrames extends TimelineEventOverview {
  constructor() {
    super('framerate', i18nString(UIStrings.fps));
  }

  update(): void {
    super.update();
    if (!this.model) {
      return;
    }
    const frames = this.model.frames();
    if (!frames.length) {
      return;
    }
    const height = this.height();
    const padding = Number(window.devicePixelRatio);
    const baseFrameDurationMs = 1e3 / 60;
    const visualHeight = height - 2 * padding;
    const timeOffset = this.model.timelineModel().minimumRecordTime();
    const timeSpan = this.model.timelineModel().maximumRecordTime() - timeOffset;
    const scale = this.width() / timeSpan;
    const baseY = height - padding;
    const ctx = this.context();
    const bottomY = baseY + 10 * window.devicePixelRatio;
    let x = 0;
    let y: number = bottomY;

    const lineWidth = window.devicePixelRatio;
    const offset = lineWidth & 1 ? 0.5 : 0;
    const tickDepth = 1.5 * window.devicePixelRatio;
    ctx.beginPath();
    ctx.moveTo(0, y);
    for (let i = 0; i < frames.length; ++i) {
      const frame = frames[i];
      x = Math.round((frame.startTime - timeOffset) * scale) + offset;
      ctx.lineTo(x, y);
      ctx.lineTo(x, y + tickDepth);
      y = frame.idle ? bottomY :
                       Math.round(baseY - visualHeight * Math.min(baseFrameDurationMs / frame.duration, 1)) - offset;
      ctx.lineTo(x, y + tickDepth);
      ctx.lineTo(x, y);
    }
    const lastFrame = frames[frames.length - 1];
    if (lastFrame) {
      x = Math.round((lastFrame.startTime + lastFrame.duration - timeOffset) * scale) + offset;
    }
    ctx.lineTo(x, y);
    ctx.lineTo(x, bottomY);
    ctx.fillStyle = 'hsl(110, 50%, 88%)';
    ctx.strokeStyle = 'hsl(110, 50%, 60%)';
    ctx.lineWidth = lineWidth;
    ctx.fill();
    ctx.stroke();
  }
}

export class TimelineEventOverviewMemory extends TimelineEventOverview {
  private heapSizeLabel: HTMLElement;
  constructor() {
    super('memory', i18nString(UIStrings.heap));
    this.heapSizeLabel = this.element.createChild('div', 'memory-graph-label');
  }

  resetHeapSizeLabels(): void {
    this.heapSizeLabel.textContent = '';
  }

  update(): void {
    super.update();
    const ratio = window.devicePixelRatio;

    if (!this.model) {
      this.resetHeapSizeLabels();
      return;
    }

    const tracks = this.model.timelineModel().tracks().filter(
        track => track.type === TimelineModel.TimelineModel.TrackType.MainThread && track.forMainFrame);
    const trackEvents = tracks.map(track => track.events);

    const lowerOffset = 3 * ratio;
    let maxUsedHeapSize = 0;
    let minUsedHeapSize = 100000000000;
    const minTime = this.model.timelineModel().minimumRecordTime();
    const maxTime = this.model.timelineModel().maximumRecordTime();

    function isUpdateCountersEvent(event: SDK.TracingModel.Event): boolean {
      return event.name === TimelineModel.TimelineModel.RecordType.UpdateCounters;
    }
    for (let i = 0; i < trackEvents.length; i++) {
      trackEvents[i] = trackEvents[i].filter(isUpdateCountersEvent);
    }

    function calculateMinMaxSizes(event: SDK.TracingModel.Event): void {
      const counters = event.args.data;
      if (!counters || !counters.jsHeapSizeUsed) {
        return;
      }
      maxUsedHeapSize = Math.max(maxUsedHeapSize, counters.jsHeapSizeUsed);
      minUsedHeapSize = Math.min(minUsedHeapSize, counters.jsHeapSizeUsed);
    }
    for (let i = 0; i < trackEvents.length; i++) {
      trackEvents[i].forEach(calculateMinMaxSizes);
    }
    minUsedHeapSize = Math.min(minUsedHeapSize, maxUsedHeapSize);

    const lineWidth = 1;
    const width = this.width();
    const height = this.height() - lowerOffset;
    const xFactor = width / (maxTime - minTime);
    const yFactor = (height - lineWidth) / Math.max(maxUsedHeapSize - minUsedHeapSize, 1);

    const histogram = new Array(width);

    function buildHistogram(event: SDK.TracingModel.Event): void {
      const counters = event.args.data;
      if (!counters || !counters.jsHeapSizeUsed) {
        return;
      }
      const x = Math.round((event.startTime - minTime) * xFactor);
      const y = Math.round((counters.jsHeapSizeUsed - minUsedHeapSize) * yFactor);
      // TODO(alph): use sum instead of max.
      histogram[x] = Math.max(histogram[x] || 0, y);
    }
    for (let i = 0; i < trackEvents.length; i++) {
      trackEvents[i].forEach(buildHistogram);
    }

    const ctx = this.context();
    const heightBeyondView = height + lowerOffset + lineWidth;

    ctx.translate(0.5, 0.5);
    ctx.beginPath();
    ctx.moveTo(-lineWidth, heightBeyondView);
    let y = 0;
    let isFirstPoint = true;
    let lastX = 0;
    for (let x = 0; x < histogram.length; x++) {
      if (typeof histogram[x] === 'undefined') {
        continue;
      }
      if (isFirstPoint) {
        isFirstPoint = false;
        y = histogram[x];
        ctx.lineTo(-lineWidth, height - y);
      }
      const nextY = histogram[x];
      if (Math.abs(nextY - y) > 2 && Math.abs(x - lastX) > 1) {
        ctx.lineTo(x, height - y);
      }
      y = nextY;
      ctx.lineTo(x, height - y);
      lastX = x;
    }
    ctx.lineTo(width + lineWidth, height - y);
    ctx.lineTo(width + lineWidth, heightBeyondView);
    ctx.closePath();

    ctx.fillStyle = 'hsla(220, 90%, 70%, 0.2)';
    ctx.fill();
    ctx.lineWidth = lineWidth;
    ctx.strokeStyle = 'hsl(220, 90%, 70%)';
    ctx.stroke();

    this.heapSizeLabel.textContent = i18nString(UIStrings.sSDash, {
      PH1: Platform.NumberUtilities.bytesToString(minUsedHeapSize),
      PH2: Platform.NumberUtilities.bytesToString(maxUsedHeapSize),
    });
  }
}

export class Quantizer {
  private lastTime: number;
  private quantDuration: number;
  private readonly callback: (arg0: Array<number>) => void;
  private counters: number[];
  private remainder: number;
  constructor(startTime: number, quantDuration: number, callback: (arg0: Array<number>) => void) {
    this.lastTime = startTime;
    this.quantDuration = quantDuration;
    this.callback = callback;
    this.counters = [];
    this.remainder = quantDuration;
  }

  appendInterval(time: number, group: number): void {
    let interval = time - this.lastTime;
    if (interval <= this.remainder) {
      this.counters[group] = (this.counters[group] || 0) + interval;
      this.remainder -= interval;
      this.lastTime = time;
      return;
    }
    this.counters[group] = (this.counters[group] || 0) + this.remainder;
    this.callback(this.counters);
    interval -= this.remainder;
    while (interval >= this.quantDuration) {
      const counters = [];
      counters[group] = this.quantDuration;
      this.callback(counters);
      interval -= this.quantDuration;
    }
    this.counters = [];
    this.counters[group] = interval;
    this.lastTime = time;
    this.remainder = this.quantDuration - interval;
  }
}

export class TimelineEventOverviewCoverage extends TimelineEventOverview {
  private heapSizeLabel: HTMLElement;
  private coverageModel?: Coverage.CoverageModel.CoverageModel|null;
  constructor() {
    super('coverage', i18nString(UIStrings.coverage));
    this.heapSizeLabel = this.element.createChild('div', 'timeline-overview-coverage-label');
  }

  resetHeapSizeLabels(): void {
    this.heapSizeLabel.textContent = '';
  }

  setModel(model: PerformanceModel|null): void {
    super.setModel(model);
    if (model) {
      const mainTarget = model.mainTarget();
      if (mainTarget) {
        this.coverageModel = mainTarget.model(Coverage.CoverageModel.CoverageModel);
      }
    }
  }

  update(): void {
    super.update();
    const ratio = window.devicePixelRatio;

    if (!this.coverageModel) {
      return;
    }

    let total = 0;
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
    // eslint-disable-next-line @typescript-eslint/naming-convention
    let total_used = 0;
    const usedByTimestamp = new Map<number, number>();
    const totalByTimestamp = new Map<number, Set<Coverage.CoverageModel.CoverageInfo>>();
    for (const urlInfo of this.coverageModel.entries()) {
      for (const info of urlInfo.entries()) {
        total += info.getSize();
        for (const [stamp, used] of info.usedByTimestamp()) {
          total_used += used;

          let uniqueTimestamps = totalByTimestamp.get(stamp);

          if (uniqueTimestamps === undefined) {
            uniqueTimestamps = new Set();
            totalByTimestamp.set(stamp, uniqueTimestamps);
          }
          uniqueTimestamps.add(info);

          const previousCount = usedByTimestamp.get(stamp);

          if (previousCount === undefined) {
            usedByTimestamp.set(stamp, used);
          } else {
            usedByTimestamp.set(stamp, previousCount + used);
          }
        }
      }
    }

    const seen = new Set<Coverage.CoverageModel.CoverageInfo>();
    const coverageByTimestamp = new Map<number, number>();
    let sumTotal = 0, sumUsed = 0;

    const sortedByTimestamp = Array.from(totalByTimestamp.entries()).sort((a, b) => a[0] - b[0]);
    for (const [stamp, infos] of sortedByTimestamp) {
      for (const info of infos.values()) {
        if (seen.has(info)) {
          continue;
        }

        seen.add(info);
        sumTotal += info.getSize();
      }
      sumUsed += usedByTimestamp.get(stamp) || 0;
      coverageByTimestamp.set(stamp, sumUsed / sumTotal);
    }

    const percentUsed = total ? Math.round(100 * total_used / total) : 0;
    const lowerOffset = 3 * ratio;

    const millisecondsPerSecond = 1000;
    if (!this.model) {
      return;
    }
    const minTime = this.model.timelineModel().minimumRecordTime() / millisecondsPerSecond;
    const maxTime = this.model.timelineModel().maximumRecordTime() / millisecondsPerSecond;

    const lineWidth = 1;
    const width = this.width();
    const height = this.height() - lowerOffset;
    const xFactor = width / (maxTime - minTime);
    const yFactor = height - lineWidth;

    let yOffset = 0;
    const ctx = this.context();
    const heightBeyondView = height + lowerOffset + lineWidth;
    ctx.translate(0.5, 0.5);
    ctx.beginPath();
    ctx.moveTo(-lineWidth, heightBeyondView);

    ctx.lineTo(-lineWidth, height - yOffset);

    let previous: (number|null)|null = null;
    for (const stamp of this.coverageModel.getCoverageUpdateTimes()) {
      const coverage: number|null = coverageByTimestamp.get(stamp) || previous;
      previous = coverage;
      if (!coverage) {
        continue;
      }
      if (stamp > maxTime) {
        break;
      }
      const x = (stamp - minTime) * xFactor;
      yOffset = coverage * yFactor;
      ctx.lineTo(x, height - yOffset);
    }

    const white = 'hsl(0, 100%, 100%)';
    const blue = 'hsl(220, 90%, 70%)';
    const transparentBlue = 'hsla(220, 90%, 70%, 0.2)';

    ctx.lineTo(width + lineWidth, height - yOffset);
    ctx.lineTo(width + lineWidth, heightBeyondView);
    ctx.closePath();
    ctx.fillStyle = transparentBlue;
    ctx.fill();
    ctx.lineWidth = lineWidth;
    ctx.strokeStyle = blue;
    ctx.stroke();

    previous = null;
    for (const stamp of this.coverageModel.getCoverageUpdateTimes()) {
      const coverage: number|null = coverageByTimestamp.get(stamp) || previous;
      previous = coverage;
      if (!coverage) {
        continue;
      }
      ctx.beginPath();
      const x = (stamp - minTime) * xFactor;
      const y = height - coverage * yFactor;
      ctx.arc(x, y, 2 * lineWidth, 0, 2 * Math.PI, false);
      ctx.closePath();
      ctx.stroke();
      ctx.fillStyle = coverageByTimestamp.has(stamp) ? blue : white;
      ctx.fill();
    }

    this.heapSizeLabel.textContent = `${percentUsed}% used`;
  }
}
