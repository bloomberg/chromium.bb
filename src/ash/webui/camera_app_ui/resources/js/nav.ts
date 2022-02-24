// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {toggleExpertMode} from './expert.js';
import * as state from './state.js';
import * as toast from './toast.js';
import {ViewName} from './type.js';
import * as util from './util.js';
import {EnterOptions, LeaveCondition, View} from './views/view.js';
import {windowController} from './window_controller.js';

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost visible view is active (clickable/focusable).
 */
let allViews: View[] = [];

/**
 * Index of the current topmost visible view in the stacked views.
 */
let topmostIndex = -1;

/**
 * Gets view and all recursive subviews.
 */
function* getRecursiveViews(view: View): Generator<View> {
  yield view;
  for (const subview of view.getSubViews()) {
    yield* getRecursiveViews(subview);
  }
}

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 * @param views All views in ascending z-order.
 */
export function setup(views: View[]): void {
  allViews = views.flatMap((v) => [...getRecursiveViews(v)]);
  // Manage all tabindex usages in for navigation.
  document.body.addEventListener('keydown', (event) => {
    const e = assertInstanceof(event, KeyboardEvent);
    if (e.key === 'Tab') {
      state.set(state.State.TAB_NAVIGATION, true);
    }
  });
  document.body.addEventListener(
      'pointerdown', () => state.set(state.State.TAB_NAVIGATION, false));
}

/**
 * Activates the view to be focusable.
 * @param index Index of the view.
 */
function activate(index: number) {
  // Restore the view's child elements' tabindex and then focus the view.
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'false');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    if (element.dataset['tabindex'] === undefined) {
      // First activation, no need to restore tabindex from data-tabindex.
      return;
    }
    element.setAttribute('tabindex', element.dataset['tabindex']);
    element.removeAttribute('data-tabindex');
  });
  view.focus();
}

/**
 * Deactivates the view to be unfocusable.
 * @param index Index of the view.
 */
function deactivate(index: number) {
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'true');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    element.dataset['tabindex'] =
        assertExists(element.getAttribute('tabindex'));
    element.setAttribute('tabindex', '-1');
  });
  const activeElement = document.activeElement;
  if (activeElement instanceof HTMLElement) {
    activeElement.blur();
  }
}

/**
 * Checks if the view is already shown.
 * @param index Index of the view.
 * @return Whether the view is shown or not.
 */
function isShown(index: number): boolean {
  return state.get(allViews[index].name);
}

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost visible view.
 * @param index Index of the view.
 * @return View shown.
 */
function show(index: number): View {
  const view = allViews[index];
  if (!isShown(index)) {
    state.set(view.name, true);
    view.layout();
    if (index > topmostIndex) {
      if (topmostIndex >= 0) {
        deactivate(topmostIndex);
      }
      activate(index);
      topmostIndex = index;
    }
  }
  return view;
}

/**
 * Finds the next topmost visible view in the stacked views.
 * @return Index of the view found; otherwise, -1.
 */
function findNextTopmostIndex(): number {
  for (let i = topmostIndex - 1; i >= 0; i--) {
    if (isShown(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Hides the view indexed in the stacked views and deactivate the view if it was
 * the topmost visible view.
 * @param index Index of the view.
 */
function hide(index: number) {
  if (index === topmostIndex) {
    deactivate(index);
    const next = findNextTopmostIndex();
    if (next >= 0) {
      activate(next);
    }
    topmostIndex = next;
  }
  state.set(allViews[index].name, false);
}

/**
 * Finds the view by its name in the stacked views.
 * @param name View name.
 * @return Index of the view found; otherwise, -1.
 */
function findIndex(name: ViewName): number {
  return allViews.findIndex((view) => view.name === name);
}

/**
 * Opens a navigation session of the view; shows the view before entering it and
 * hides the view after leaving it for the ended session.
 * @param name View name.
 * @param args Optional rest parameters for entering the view.
 * @return Promise for the operation or result.
 */
export function open(
    name: ViewName, options?: EnterOptions): Promise<LeaveCondition> {
  const index = findIndex(name);
  return show(index).enter(options).finally(() => {
    hide(index);
  });
}

/**
 * Closes the current navigation session of the view by leaving it.
 * @param name View name.
 * @param condition Optional condition for leaving the view.
 * @return Whether successfully leaving the view or not.
 */
export function close(name: ViewName, condition?: unknown): boolean {
  const index = findIndex(name);
  return allViews[index].leave({kind: 'CLOSED', val: condition});
}

/**
 * Handles key pressed event.
 * @param event Key press event.
 */
export function onKeyPressed(event: KeyboardEvent): void {
  const key = util.getShortcutIdentifier(event);
  switch (key) {
    case 'BrowserBack':
      // Only works for non-intent instance.
      if (!state.get(state.State.INTENT)) {
        windowController.minimize();
      }
      break;
    case 'Alt--':
      // Prevent intent window from minimizing.
      if (state.get(state.State.INTENT)) {
        event.preventDefault();
      }
      break;
    case 'Ctrl-=':
    case 'Ctrl--':
      // Blocks the in-app zoom in/out to avoid unexpected layout.
      event.preventDefault();
      break;
    case 'Ctrl-V':
      toast.showDebugMessage('SWA');
      break;
    case 'Ctrl-Shift-E':
      toggleExpertMode();
      break;
    default:
      // Make the topmost visible view handle the pressed key.
      if (topmostIndex >= 0 && allViews[topmostIndex].onKeyPressed(key)) {
        event.preventDefault();
      }
  }
}

/**
 * Handles when the window state or size changed.
 */
export function onWindowStatusChanged(): void {
  // All visible views need being relayout after window is resized or state
  // changed.
  for (let i = allViews.length - 1; i >= 0; i--) {
    if (isShown(i)) {
      allViews[i].layout();
    }
  }
}

/**
 * Returns whether the view is the top view above all shown view.
 * @param name Name of the view
 */
export function isTopMostView(name: ViewName): boolean {
  return topmostIndex === findIndex(name);
}
