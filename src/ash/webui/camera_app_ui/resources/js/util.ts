// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from './animation.js';
import {assert, assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as Comlink from './lib/comlink.js';
import * as loadTimeData from './models/load_time_data.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {Facing} from './type.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Creates a canvas element for 2D drawing.
 * @param params Width/Height of the canvas.
 * @return Returns canvas element and the context for 2D drawing.
 */
export function newDrawingCanvas(
    {width, height}: {width: number, height: number}):
    {canvas: HTMLCanvasElement, ctx: CanvasRenderingContext2D} {
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  const ctx =
      assertInstanceof(canvas.getContext('2d'), CanvasRenderingContext2D);
  return {canvas, ctx};
}

export function bitmapToJpegBlob(bitmap: ImageBitmap): Promise<Blob> {
  const {canvas, ctx} =
      newDrawingCanvas({width: bitmap.width, height: bitmap.height});
  ctx.drawImage(bitmap, 0, 0);
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) {
        resolve(blob);
      } else {
        reject(new Error('Photo blob error.'));
      }
    }, 'image/jpeg');
  });
}

/**
 * Returns a shortcut string, such as Ctrl-Alt-A.
 * @param event Keyboard event.
 * @return Shortcut identifier.
 */
export function getShortcutIdentifier(event: KeyboardEvent): string {
  let identifier = (event.ctrlKey ? 'Ctrl-' : '') +
      (event.altKey ? 'Alt-' : '') + (event.shiftKey ? 'Shift-' : '') +
      (event.metaKey ? 'Meta-' : '');
  if (event.key) {
    switch (event.key) {
      case 'ArrowLeft':
        identifier += 'Left';
        break;
      case 'ArrowRight':
        identifier += 'Right';
        break;
      case 'ArrowDown':
        identifier += 'Down';
        break;
      case 'ArrowUp':
        identifier += 'Up';
        break;
      case 'a':
      case 'p':
      case 's':
      case 'v':
      case 'r':
        identifier += event.key.toUpperCase();
        break;
      default:
        identifier += event.key;
    }
  }
  return identifier;
}

/**
 * Opens help.
 */
export function openHelp(): void {
  window.open(
      'https://support.google.com/chromebook/?p=camera_usage_on_chromebook');
}

/**
 * Sets up i18n messages on DOM subtree by i18n attributes.
 * @param rootElement Root of DOM subtree to be set up with.
 */
export function setupI18nElements(rootElement: Element|DocumentFragment): void {
  const getElements = (attr) =>
      dom.getAllFrom(rootElement, '[' + attr + ']', HTMLElement);
  const getMessage = (element, attr) =>
      loadTimeData.getI18nMessage(element.getAttribute(attr));
  const setAriaLabel = (element, attr) =>
      element.setAttribute('aria-label', getMessage(element, attr));

  getElements('i18n-text')
      .forEach(
          (element) => element.textContent = getMessage(element, 'i18n-text'));
  getElements('i18n-tooltip-true')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-true', getMessage(element, 'i18n-tooltip-true')));
  getElements('i18n-tooltip-false')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-false', getMessage(element, 'i18n-tooltip-false')));
  getElements('i18n-aria')
      .forEach((element) => setAriaLabel(element, 'i18n-aria'));
  tooltip.setup(getElements('i18n-label'))
      .forEach((element) => setAriaLabel(element, 'i18n-label'));
}

/**
 * Reads blob into Image.
 */
export function blobToImage(blob: Blob): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
}

/**
 * Gets default facing according to device mode.
 */
export function getDefaultFacing(): Facing {
  return state.get(state.State.TABLET) ? Facing.ENVIRONMENT : Facing.USER;
}

/**
 * Toggle checked value of element.
 */
export function toggleChecked(
    element: HTMLInputElement, checked: boolean): void {
  element.checked = checked;
  element.dispatchEvent(new Event('change'));
}

/**
 * Binds on/off of specified state with different aria label on an element.
 */
export function bindElementAriaLabelWithState(
    {element, state: s, onLabel, offLabel}: {
      element: Element,
      state: state.State,
      onLabel: I18nString,
      offLabel: I18nString,
    }): void {
  const update = (value) => {
    const label = value ? onLabel : offLabel;
    element.setAttribute('i18n-label', label);
    element.setAttribute('aria-label', loadTimeData.getI18nMessage(label));
  };
  update(state.get(s));
  state.addObserver(s, update);
}

/**
 * Sets inkdrop effect on button or label in setting menu.
 */
export function setInkdropEffect(el: HTMLElement): void {
  const tpl = instantiateTemplate('#inkdrop-template');
  el.appendChild(tpl);
  el.addEventListener('click', (e) => {
    const tRect =
        assertInstanceof(e.target, HTMLElement).getBoundingClientRect();
    const elRect = el.getBoundingClientRect();
    const dropX = tRect.left + e.offsetX - elRect.left;
    const dropY = tRect.top + e.offsetY - elRect.top;
    const maxDx = Math.max(Math.abs(dropX), Math.abs(elRect.width - dropX));
    const maxDy = Math.max(Math.abs(dropY), Math.abs(elRect.height - dropY));
    const radius = Math.hypot(maxDx, maxDy);
    el.style.setProperty('--drop-x', `${dropX}px`);
    el.style.setProperty('--drop-y', `${dropY}px`);
    el.style.setProperty('--drop-radius', `${radius}px`);
    animate.playOnChild(el);
  });
}

/**
 * Instantiates template with the target selector.
 */
export function instantiateTemplate(selector: string): DocumentFragment {
  const tpl = dom.get(selector, HTMLTemplateElement);
  const doc = assertInstanceof(
      document.importNode(tpl.content, true), DocumentFragment);
  setupI18nElements(doc);
  return doc;
}

/**
 * Creates JS module by given |scriptUrl| under untrusted context with given
 * origin and returns its proxy.
 * @param scriptUrl The URL of the script to load.
 */
export async function createUntrustedJSModule<T>(scriptUrl: string):
    Promise<Comlink.Remote<T>> {
  const untrustedPageReady = new WaitableEvent();
  const iFrame = document.createElement('iframe');
  iFrame.addEventListener('load', () => untrustedPageReady.signal());
  iFrame.setAttribute(
      'src',
      'chrome-untrusted://camera-app/views/untrusted_script_loader.html');
  iFrame.hidden = true;
  document.body.appendChild(iFrame);
  await untrustedPageReady.wait();

  // TODO(pihsun): actually get correct type from the function definition.
  const untrustedRemote =
      Comlink.wrap<{loadScript(url: string): Promise<void>}>(
          Comlink.windowEndpoint(iFrame.contentWindow, self));
  await untrustedRemote.loadScript(scriptUrl);
  // loadScript adds the script exports to what's exported by the
  // untrustedRemote, so we manually cast it to the expected type.
  return untrustedRemote as unknown as Comlink.Remote<T>;
}

/**
 * Sleeps for a specified time.
 * @param ms Milliseconds to sleep.
 */
export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Gets value in px of a property in a StylePropertyMapReadOnly
 */
export function getStyleValueInPx(
    style: (StylePropertyMapReadOnly|StylePropertyMap), prop: string): number {
  return assertInstanceof(style.get(prop), CSSNumericValue).to('px').value;
}

/**
 * Trigger callback in fixed interval like |setInterval()| with specified delay
 * before calling the first callback.
 */
export class DelayInterval {
  private intervalId: number|null = null;
  private readonly delayTimeoutId: number;
  /**
   * @param delayMs Delay milliseconds at start.
   * @param intervalMs Interval in milliseconds.
   */
  constructor(callback: () => void, delayMs: number, intervalMs: number) {
    this.delayTimeoutId = setTimeout(() => {
      this.intervalId = setInterval(() => {
        callback();
      }, intervalMs);
      callback();
    }, delayMs);
  }

  /**
   * Stop the interval.
   */
  stop(): void {
    if (this.intervalId === null) {
      clearTimeout(this.delayTimeoutId);
    } else {
      clearInterval(this.intervalId);
    }
  }
}

/**
 * Share file with share API.
 */
export async function share(file: File): Promise<void> {
  const shareData = {files: [file]};
  try {
    if (!navigator.canShare(shareData)) {
      throw new Error('cannot share');
    }
    await navigator.share(shareData);
  } catch (e) {
    // TODO(b/191950622): Handles all share error case, e.g. no
    // share target, share abort... with right treatment like toast
    // message.
  }
}

/**
 * Check if a string value is a variant of an enum.
 * @param value value to be checked
 * @return the value if it's an enum variant, null otherwise
 */
export function checkEnumVariant<T extends string>(
    enumType: {[key: string]: T}, value: string|null): T|null {
  if (value === null || !Object.values<string>(enumType).includes(value)) {
    return null;
  }
  return value as T;
}

/**
 * Asserts that a string value is a variant of an enum.
 * @param value value to be checked
 * @return the value if it's an enum variant, throws assertion error otherwise.
 */
export function assertEnumVariant<T extends string>(
    enumType: {[key: string]: T}, value: string): T {
  const ret = checkEnumVariant(enumType, value);
  assert(ret !== null);
  return ret;
}
