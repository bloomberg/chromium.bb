// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import {ViewName} from '../type.js';
import {WaitableEvent} from '../waitable_event.js';

/**
 * message for message of the dialog view, cancellable for whether the dialog
 * view is cancellable.
 */
export interface DialogEnterOptions {
  message?: string;
  cancellable?: boolean;
}

/**
 * Warning message name.
 */
type WarningEnterOptions = string;

/**
 * Options for open PTZ panel.
 */
export class PTZPanelOptions {
  readonly stream: MediaStream;
  readonly vidPid: string|null;
  readonly resetPTZ: () => Promise<void>;

  constructor({stream, vidPid, resetPTZ}: {
    stream: MediaStream,
    vidPid: string|null,
    resetPTZ: () => Promise<void>,
  }) {
    this.stream = stream;
    this.vidPid = vidPid;
    this.resetPTZ = resetPTZ;
  }
}

// TODO(pihsun): After we migrate all files into TypeScript, we can have some
// sort of "global" view registration, so we can enforce the enter / leave type
// at compile time.
export type EnterOptions =
    DialogEnterOptions|WarningEnterOptions|PTZPanelOptions;

interface ViewOptions {
  /** enables dismissible by Esc-key. */
  dismissByEsc?: boolean;

  /** enables dismissible by background-click. */
  dismissByBackgroundClick?: boolean;

  /**
   * selects element to be focused in focus(). Focus to first element whose
   * tabindex is not -1 when argument is not presented.
   */
  defaultFocusSelector?: string;
}

/**
 * Base controller of a view for views' navigation sessions (nav.js).
 */
export class View {
  root: HTMLElement;

  /**
   * Signal it to ends the session.
   */
  private session: WaitableEvent<unknown>|null = null;

  private readonly dismissByEsc: boolean;
  private readonly defaultFocusSelector: string;

  /**
   * @param name Unique name of view which should be same as its DOM element id.
   */
  constructor(readonly name: ViewName, {
    dismissByEsc = false,
    dismissByBackgroundClick,
    defaultFocusSelector = '[tabindex]:not([tabindex="-1"])',
  }: ViewOptions = {}) {
    this.root = dom.get(`#${name}`, HTMLElement);
    this.dismissByEsc = dismissByEsc;
    this.defaultFocusSelector = defaultFocusSelector;

    if (dismissByBackgroundClick) {
      this.root.addEventListener('click', (event) => {
        event.target === this.root && this.leave({bkgnd: true});
      });
    }
  }

  /**
   * Gets sub-views nested under this view.
   */
  getSubViews(): View[] {
    return [];
  }

  /**
   * Hook of the subclass for handling the key.
   * @param key Key to be handled.
   * @return Whether the key has been handled or not.
   */
  handlingKey(_key: string): boolean {
    return false;
  }

  /**
   * Handles the pressed key.
   * @param key Key to be handled.
   * @return Whether the key has been handled or not.
   */
  onKeyPressed(key: string): boolean {
    if (this.handlingKey(key)) {
      return true;
    } else if (this.dismissByEsc && key === 'Escape') {
      this.leave();
      return true;
    }
    return false;
  }

  /**
   * Focuses the default element on the view if applicable.
   */
  focus(): void {
    const el = this.root.querySelector(this.defaultFocusSelector);
    if (el !== null) {
      assertInstanceof(el, HTMLElement).focus();
    }
  }

  /**
   * Layouts the view.
   */
  layout(): void {
    // To be overridden by subclasses.
  }

  /**
   * Hook of the subclass for entering the view.
   * @param options Optional rest parameters for entering the view.
   */
  entering(_options?: EnterOptions): void {
    // To be overridden by subclasses.
  }

  /**
   * Enters the view.
   * @param options Optional rest parameters for entering the view.
   * @return Promise for the navigation session.
   */
  enter(options?: EnterOptions): Promise<unknown> {
    // The session is started by entering the view and ended by leaving the
    // view.
    if (this.session === null) {
      this.session = new WaitableEvent();
    }
    this.entering(options);
    return this.session.wait();
  }

  /**
   * Hook of the subclass for leaving the view.
   * @param condition Optional condition for leaving the view.
   * @return Whether able to leaving the view or not.
   */
  leaving(_condition?: unknown): boolean {
    return true;
  }

  /**
   * Leaves the view.
   * @param condition Optional condition for leaving the view and also as
   *     the result for the ended session.
   * @return Whether able to leaving the view or not.
   */
  leave(condition?: unknown): boolean {
    if (this.session !== null && this.leaving(condition)) {
      this.session.signal(condition);
      this.session = null;
      return true;
    }
    return false;
  }
}
