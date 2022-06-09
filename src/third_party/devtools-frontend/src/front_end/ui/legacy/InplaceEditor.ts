// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Platform from '../../core/platform/platform.js';

import * as ARIAUtils from './ARIAUtils.js';
import {Keys} from './KeyboardShortcut.js';
import {ElementFocusRestorer, markBeingEdited} from './UIUtils.js';

// TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
// eslint-disable-next-line @typescript-eslint/naming-convention
let _defaultInstance: InplaceEditor<unknown>|null = null;

export class InplaceEditor<T> {
  private focusRestorer?: ElementFocusRestorer;
  static startEditing<T>(element: Element, config?: Config<T>): Controller|null {
    if (!_defaultInstance) {
      _defaultInstance = new InplaceEditor();
    }
    return _defaultInstance.startEditing(element, config as Config<unknown>);
  }

  editorContent(editingContext: EditingContext<T>): string {
    const element = editingContext.element;
    if (element.tagName === 'INPUT' && (element as HTMLInputElement).type === 'text') {
      return (element as HTMLInputElement).value;
    }

    return element.textContent || '';
  }

  setUpEditor(editingContext: EditingContext<T>): void {
    const element = (editingContext.element as HTMLElement);
    element.classList.add('editing');
    element.setAttribute('contenteditable', 'plaintext-only');

    const oldRole = element.getAttribute('role');
    ARIAUtils.markAsTextBox(element);
    editingContext.oldRole = oldRole;

    const oldTabIndex = element.tabIndex;
    if (typeof oldTabIndex !== 'number' || oldTabIndex < 0) {
      element.tabIndex = 0;
    }
    this.focusRestorer = new ElementFocusRestorer(element);
    editingContext.oldTabIndex = oldTabIndex;
  }

  closeEditor(editingContext: EditingContext<T>): void {
    const element = (editingContext.element as HTMLElement);
    element.classList.remove('editing');
    element.removeAttribute('contenteditable');

    if (typeof editingContext.oldRole !== 'string') {
      element.removeAttribute('role');
    } else {
      element.setAttribute('role', editingContext.oldRole);
    }

    if (typeof editingContext.oldTabIndex !== 'number') {
      element.removeAttribute('tabIndex');
    } else {
      element.tabIndex = editingContext.oldTabIndex;
    }
    element.scrollTop = 0;
    element.scrollLeft = 0;
  }

  cancelEditing(editingContext: EditingContext<T>): void {
    const element = (editingContext.element as HTMLElement);
    if (element.tagName === 'INPUT' && (element as HTMLInputElement).type === 'text') {
      (element as HTMLInputElement).value = editingContext.oldText || '';
    } else {
      element.textContent = editingContext.oldText;
    }
  }

  startEditing(element: Element, inputConfig?: Config<T>): Controller|null {
    if (!markBeingEdited(element, true)) {
      return null;
    }

    const config = inputConfig || new Config(function() {}, function() {});
    const editingContext:
        EditingContext<T> = {element: element, config: config, oldRole: null, oldTabIndex: null, oldText: null};
    const committedCallback = config.commitHandler;
    const cancelledCallback = config.cancelHandler;
    const pasteCallback = config.pasteHandler;
    const context = config.context;
    let moveDirection = '';
    const self = this;

    this.setUpEditor(editingContext);

    editingContext.oldText = this.editorContent(editingContext);

    function blurEventListener(e?: Event): void {
      if (config.blurHandler && !config.blurHandler(element, e)) {
        return;
      }
      editingCommitted.call(element);
    }

    function cleanUpAfterEditing(): void {
      markBeingEdited(element, false);

      element.removeEventListener('blur', blurEventListener, false);
      element.removeEventListener('keydown', keyDownEventListener, true);
      if (pasteCallback) {
        element.removeEventListener('paste', pasteEventListener, true);
      }

      if (self.focusRestorer) {
        self.focusRestorer.restore();
      }
      self.closeEditor(editingContext);
    }

    function editingCancelled(this: Element): void {
      self.cancelEditing(editingContext);
      cleanUpAfterEditing();
      cancelledCallback(this, context);
    }

    function editingCommitted(this: Element): void {
      cleanUpAfterEditing();

      committedCallback(this, self.editorContent(editingContext), editingContext.oldText || '', context, moveDirection);
    }

    function defaultFinishHandler(event: KeyboardEvent): string {
      if (event.key === 'Enter') {
        return 'commit';
      }
      if (event.keyCode === Keys.Esc.code || event.key === Platform.KeyboardUtilities.ESCAPE_KEY) {
        return 'cancel';
      }
      if (event.key === 'Tab') {
        return 'move-' + (event.shiftKey ? 'backward' : 'forward');
      }
      return '';
    }

    function handleEditingResult(result: string|undefined, event: Event): void {
      if (result === 'commit') {
        editingCommitted.call(element);
        event.consume(true);
      } else if (result === 'cancel') {
        editingCancelled.call(element);
        event.consume(true);
      } else if (result && result.startsWith('move-')) {
        moveDirection = result.substring(5);
        if ((event as KeyboardEvent).key === 'Tab') {
          event.consume(true);
        }
        blurEventListener();
      }
    }

    function pasteEventListener(event: Event): void {
      if (!pasteCallback) {
        return;
      }
      const result = pasteCallback(event);
      handleEditingResult(result, event);
    }

    function keyDownEventListener(event: Event): void {
      let result = defaultFinishHandler((event as KeyboardEvent));
      if (!result && config.postKeydownFinishHandler) {
        const postKeydownResult = config.postKeydownFinishHandler(event);
        if (postKeydownResult) {
          result = postKeydownResult;
        }
      }
      handleEditingResult(result, event);
    }

    element.addEventListener('blur', blurEventListener, false);
    element.addEventListener('keydown', keyDownEventListener, true);
    if (pasteCallback !== undefined) {
      element.addEventListener('paste', pasteEventListener, true);
    }

    const handle = {cancel: editingCancelled.bind(element), commit: editingCommitted.bind(element)};
    return handle;
  }
}

export type CommitHandler<T> = (arg0: Element, arg1: string, arg2: string, arg3: T, arg4: string) => void;
export type CancelHandler<T> = (arg0: Element, arg1: T) => void;
export type BlurHandler = (arg0: Element, arg1?: Event|undefined) => boolean;

export class Config<T = undefined> {
  commitHandler: CommitHandler<T>;
  cancelHandler: CancelHandler<T>;
  context: T;
  blurHandler: BlurHandler|undefined;
  pasteHandler!: EventHandler|null;
  postKeydownFinishHandler!: EventHandler|null;

  constructor(
      commitHandler: CommitHandler<T>, cancelHandler: CancelHandler<T>, context?: T, blurHandler?: BlurHandler) {
    this.commitHandler = commitHandler;
    this.cancelHandler = cancelHandler;
    this.context = context as T;
    this.blurHandler = blurHandler;
  }

  setPasteHandler(pasteHandler: EventHandler): void {
    this.pasteHandler = pasteHandler;
  }

  setPostKeydownFinishHandler(postKeydownFinishHandler: EventHandler): void {
    this.postKeydownFinishHandler = postKeydownFinishHandler;
  }
}

export type EventHandler = (event: Event) => string|undefined;

export interface Controller {
  cancel: () => void;
  commit: () => void;
}

export interface EditingContext<T> {
  element: Element;
  config: Config<T>;
  oldRole: string|null;
  oldText: string|null;
  oldTabIndex: number|null;
}
