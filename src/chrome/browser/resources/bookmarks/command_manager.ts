// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows context menus and handles keyboard
 * shortcuts.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import './edit_dialog.js';
import './shared_style.js';
import './strings.m.js';
import './edit_dialog.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {KeyboardShortcutList} from 'chrome://resources/js/cr/ui/keyboard_shortcut_list.m.js';
import {StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, flush, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {deselectItems, selectAll, selectFolder} from './actions.js';
import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {BrowserProxy} from './browser_proxy.js';
import {Command, IncognitoAvailability, MenuSource, OPEN_CONFIRMATION_LIMIT, ROOT_NODE_ID} from './constants.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import {BookmarksEditDialogElement} from './edit_dialog.js';
import {BookmarksStoreClientInterface, StoreClient} from './store_client.js';
import {BookmarkNode, BookmarksPageState, OpenCommandMenuDetail} from './types.js';
import {canEditNode, canReorderChildren, getDisplayedList} from './util.js';

const BookmarksCommandManagerElementBase =
    mixinBehaviors([StoreClient], PolymerElement) as {
      new (): PolymerElement & BookmarksStoreClientInterface &
      StoreObserver<BookmarksPageState>
    };

export interface BookmarksCommandManagerElement {
  $: {
    dropdown: CrLazyRenderElement<CrActionMenuElement>,
    editDialog: CrLazyRenderElement<BookmarksEditDialogElement>,
    openDialog: CrLazyRenderElement<CrDialogElement>,
  }
}

let instance: BookmarksCommandManagerElement|null = null;

export class BookmarksCommandManagerElement extends
    BookmarksCommandManagerElementBase {
  static get is() {
    return 'bookmarks-command-manager';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      menuCommands_: {
        type: Array,
        computed: 'computeMenuCommands_(menuSource_)',
      },

      menuIds_: Object,

      menuSource_: Number,

      canPaste_: Boolean,

      globalCanEdit_: Boolean,
    };
  }

  /**
   * Indicates where the context menu was opened from. Will be NONE if
   * menu is not open, indicating that commands are from keyboard shortcuts
   * or elsewhere in the UI.
   */
  private menuSource_: MenuSource = MenuSource.NONE;
  private confirmOpenCallback_: (() => void)|null = null;
  private canPaste_: boolean;
  private globalCanEdit_: boolean;
  private menuIds_: Set<string>;
  private menuCommands_: Command[];
  private browserProxy_: BrowserProxy;
  private shortcuts_: Map<Command, KeyboardShortcutList>;
  private eventTracker_: EventTracker = new EventTracker();

  connectedCallback() {
    super.connectedCallback();
    assert(instance === null);
    instance = this;

    this.browserProxy_ = BrowserProxy.getInstance();

    this.watch(
        'globalCanEdit_', state => (state as BookmarksPageState).prefs.canEdit);
    this.updateFromStore();

    this.shortcuts_ = new Map();

    this.addShortcut_(Command.EDIT, 'F2', 'Enter');
    this.addShortcut_(Command.DELETE, 'Delete', 'Delete Backspace');

    this.addShortcut_(Command.OPEN, 'Enter', 'Meta|o');
    this.addShortcut_(Command.OPEN_NEW_TAB, 'Ctrl|Enter', 'Meta|Enter');
    this.addShortcut_(Command.OPEN_NEW_WINDOW, 'Shift|Enter');

    // Note: the undo shortcut is also defined in bookmarks_ui.cc
    // TODO(b/893033): de-duplicate shortcut by moving all shortcut
    // definitions from JS to C++.
    this.addShortcut_(Command.UNDO, 'Ctrl|z', 'Meta|z');
    this.addShortcut_(Command.REDO, 'Ctrl|y Ctrl|Shift|Z', 'Meta|Shift|Z');

    this.addShortcut_(Command.SELECT_ALL, 'Ctrl|a', 'Meta|a');
    this.addShortcut_(Command.DESELECT_ALL, 'Escape');

    this.addShortcut_(Command.CUT, 'Ctrl|x', 'Meta|x');
    this.addShortcut_(Command.COPY, 'Ctrl|c', 'Meta|c');
    this.addShortcut_(Command.PASTE, 'Ctrl|v', 'Meta|v');

    this.eventTracker_.add(document, 'open-command-menu', e =>
                           this.onOpenCommandMenu_(
                               e as CustomEvent<OpenCommandMenuDetail>));
    this.eventTracker_.add(document, 'keydown', e =>
                           this.onKeydown_(e as KeyboardEvent));

    const addDocumentListenerForCommand = (eventName: string,
                                           command: Command) => {
      this.eventTracker_.add(document, eventName, e => {
        if ((e.composedPath()[0] as HTMLElement).tagName === 'INPUT') {
          return;
        }

        const items = this.getState().selection.items;
        if (this.canExecute(command, items)) {
          this.handle(command, items);
        }
      });
    };
    addDocumentListenerForCommand('command-undo', Command.UNDO);
    addDocumentListenerForCommand('cut', Command.CUT);
    addDocumentListenerForCommand('copy', Command.COPY);
    addDocumentListenerForCommand('paste', Command.PASTE);

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    instance = null;
    this.eventTracker_.removeAll();
  }

  /**
   * Display the command context menu at (|x|, |y|) in window coordinates.
   * Commands will execute on |items| if given, or on the currently selected
   * items.
   */
  openCommandMenuAtPosition(
      x: number, y: number, source: MenuSource, items?: Set<string>) {
    this.menuSource_ = source;
    this.menuIds_ = items || this.getState().selection.items;

    const dropdown = this.$.dropdown.get();
    // Ensure that the menu is fully rendered before trying to position it.
    flush();
    DialogFocusManager.getInstance().showDialog(
        dropdown.getDialog(), function() {
          dropdown.showAtPosition({top: y, left: x});
        });
  }

  /**
   * Display the command context menu positioned to cover the |target|
   * element. Commands will execute on the currently selected items.
   */
  openCommandMenuAtElement(target: Element, source: MenuSource) {
    this.menuSource_ = source;
    this.menuIds_ = this.getState().selection.items;

    const dropdown = this.$.dropdown.get();
    // Ensure that the menu is fully rendered before trying to position it.
    flush();
    DialogFocusManager.getInstance().showDialog(
        dropdown.getDialog(), function() {
          dropdown.showAt(target);
        });
  }

  closeCommandMenu() {
    this.menuIds_ = new Set();
    this.menuSource_ = MenuSource.NONE;
    this.$.dropdown.get().close();
  }

  ////////////////////////////////////////////////////////////////////////////
  // Command handlers:

  /**
   * Determine if the |command| can be executed with the given |itemIds|.
   * Commands which appear in the context menu should be implemented
   * separately using `isCommandVisible_` and `isCommandEnabled_`.
   */
  canExecute(command: Command, itemIds: Set<string>): boolean {
    const state = this.getState();
    switch (command) {
      case Command.OPEN:
        return itemIds.size > 0;
      case Command.UNDO:
      case Command.REDO:
        return this.globalCanEdit_;
      case Command.SELECT_ALL:
      case Command.DESELECT_ALL:
        return true;
      case Command.COPY:
        return itemIds.size > 0;
      case Command.CUT:
        return itemIds.size > 0 &&
            !this.containsMatchingNode_(itemIds, function(node) {
              return !canEditNode(state, node.id);
            });
      case Command.PASTE:
        return state.search.term === '' &&
            canReorderChildren(state, state.selectedFolder);
      default:
        return this.isCommandVisible_(command, itemIds) &&
            this.isCommandEnabled_(command, itemIds);
    }
  }

  isCommandVisible_(command: Command, itemIds: Set<string>): boolean {
    switch (command) {
      case Command.EDIT:
        return itemIds.size === 1 && this.globalCanEdit_;
      case Command.PASTE:
        return this.globalCanEdit_;
      case Command.CUT:
      case Command.COPY:
        return itemIds.size >= 1 && this.globalCanEdit_;
      case Command.COPY_URL:
        return this.isSingleBookmark_(itemIds);
      case Command.DELETE:
        return itemIds.size > 0 && this.globalCanEdit_;
      case Command.SHOW_IN_FOLDER:
        return this.menuSource_ === MenuSource.ITEM && itemIds.size === 1 &&
            this.getState().search.term !== '' &&
            !this.containsMatchingNode_(itemIds, function(node) {
              return !node.parentId || node.parentId === ROOT_NODE_ID;
            });
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
      case Command.OPEN_INCOGNITO:
        return itemIds.size > 0;
      case Command.ADD_BOOKMARK:
      case Command.ADD_FOLDER:
      case Command.SORT:
      case Command.EXPORT:
      case Command.IMPORT:
      case Command.HELP_CENTER:
        return true;
    }
    return assert(false);
  }

  private isCommandEnabled_(command: Command, itemIds: Set<string>): boolean {
    const state = this.getState();
    switch (command) {
      case Command.EDIT:
      case Command.DELETE:
        return !this.containsMatchingNode_(itemIds, function(node) {
          return !canEditNode(state, node.id);
        });
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
        return this.expandUrls_(itemIds).length > 0;
      case Command.OPEN_INCOGNITO:
        return this.expandUrls_(itemIds).length > 0 &&
            state.prefs.incognitoAvailability !==
            IncognitoAvailability.DISABLED;
      case Command.SORT:
        return this.canChangeList_() &&
            state.nodes[state.selectedFolder]!.children!.length > 1;
      case Command.ADD_BOOKMARK:
      case Command.ADD_FOLDER:
        return this.canChangeList_();
      case Command.IMPORT:
        return this.globalCanEdit_;
      case Command.PASTE:
        return this.canPaste_;
      default:
        return true;
    }
  }

  /**
   * Returns whether the currently displayed bookmarks list can be changed.
   */
  private canChangeList_(): boolean {
    const state = this.getState();
    return state.search.term === '' &&
        canReorderChildren(state, state.selectedFolder);
  }

  handle(command: Command, itemIds: Set<string>) {
    const state = this.getState();
    switch (command) {
      case Command.EDIT: {
        const id = Array.from(itemIds)[0]!;
        this.$.editDialog.get().showEditDialog(state.nodes[id]!);
        break;
      }
      case Command.COPY_URL:
      case Command.COPY: {
        const idList = Array.from(itemIds);
        chrome.bookmarkManagerPrivate.copy(idList, () => {
          let labelPromise: Promise<string>;
          if (command === Command.COPY_URL) {
            labelPromise =
                Promise.resolve(loadTimeData.getString('toastUrlCopied'));
          } else if (idList.length === 1) {
            labelPromise =
                Promise.resolve(loadTimeData.getString('toastItemCopied'));
          } else {
            labelPromise = PluralStringProxyImpl.getInstance().getPluralString(
                'toastItemsCopied', idList.length);
          }

          this.showTitleToast_(
              labelPromise, state.nodes[idList[0]!]!.title, false);
        });
        break;
      }
      case Command.SHOW_IN_FOLDER: {
        const id = Array.from(itemIds)[0];
        this.dispatch(
            selectFolder(assert(state.nodes[id!]!.parentId!), state.nodes));
        DialogFocusManager.getInstance().clearFocus();
        this.dispatchEvent(new CustomEvent(
            'highlight-items', {bubbles: true, composed: true, detail: [id]}));
        break;
      }
      case Command.DELETE: {
        const idList = Array.from(this.minimizeDeletionSet_(itemIds));
        const title = state.nodes[idList[0]!]!.title;
        let labelPromise: Promise<string>;

        if (idList.length === 1) {
          labelPromise =
              Promise.resolve(loadTimeData.getString('toastItemDeleted'));
        } else {
          labelPromise = PluralStringProxyImpl.getInstance().getPluralString(
              'toastItemsDeleted', idList.length);
        }

        chrome.bookmarkManagerPrivate.removeTrees(idList, () => {
          this.showTitleToast_(labelPromise, title, true);
        });
        break;
      }
      case Command.UNDO:
        chrome.bookmarkManagerPrivate.undo();
        getToastManager().hide();
        break;
      case Command.REDO:
        chrome.bookmarkManagerPrivate.redo();
        break;
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
      case Command.OPEN_INCOGNITO:
        this.openUrls_(this.expandUrls_(itemIds), command);
        break;
      case Command.OPEN:
        if (this.isFolder_(itemIds)) {
          const folderId = Array.from(itemIds)[0]!;
          this.dispatch(selectFolder(folderId, state.nodes));
        } else {
          this.openUrls_(this.expandUrls_(itemIds), command);
        }
        break;
      case Command.SELECT_ALL:
        const displayedIds = getDisplayedList(state);
        this.dispatch(selectAll(displayedIds, state));
        break;
      case Command.DESELECT_ALL:
        this.dispatch(deselectItems());
        IronA11yAnnouncer.requestAvailability();
        this.dispatchEvent(new CustomEvent('iron-announce', {
          bubbles: true,
          composed: true,
          detail: {text: loadTimeData.getString('itemsUnselected')}
        }));
        break;
      case Command.CUT:
        chrome.bookmarkManagerPrivate.cut(Array.from(itemIds));
        break;
      case Command.PASTE:
        const selectedFolder = state.selectedFolder;
        const selectedItems = state.selection.items;
        trackUpdatedItems();
        chrome.bookmarkManagerPrivate.paste(
            selectedFolder, Array.from(selectedItems), highlightUpdatedItems);
        break;
      case Command.SORT:
        chrome.bookmarkManagerPrivate.sortChildren(
            assert(state.selectedFolder));
        getToastManager().querySelector('dom-if')!.if = true;
        getToastManager().show(loadTimeData.getString('toastFolderSorted'));
        break;
      case Command.ADD_BOOKMARK:
        this.$.editDialog.get().showAddDialog(
            false, assert(state.selectedFolder));
        break;
      case Command.ADD_FOLDER:
        this.$.editDialog.get().showAddDialog(
            true, assert(state.selectedFolder));
        break;
      case Command.IMPORT:
        chrome.bookmarks.import();
        break;
      case Command.EXPORT:
        chrome.bookmarks.export();
        break;
      case Command.HELP_CENTER:
        window.open('https://support.google.com/chrome/?p=bookmarks');
        break;
      default:
        assert(false);
    }
    this.recordCommandHistogram_(
        itemIds, 'BookmarkManager.CommandExecuted', command);
  }

  handleKeyEvent(e: Event, itemIds: Set<string>): boolean {
    for (const commandTuple of this.shortcuts_) {
      const command = commandTuple[0] as Command;
      const shortcut = commandTuple[1] as KeyboardShortcutList;
      if (shortcut.matchesEvent(e) && this.canExecute(command, itemIds)) {
        this.handle(command, itemIds);

        e.stopPropagation();
        e.preventDefault();
        return true;
      }
    }

    return false;
  }

  ////////////////////////////////////////////////////////////////////////////
  // Private functions:

  /**
   * Register a keyboard shortcut for a command.
   */
  private addShortcut_(
      command: Command, shortcut: string, macShortcut?: string) {
    shortcut = (isMac && macShortcut) ? macShortcut : shortcut;
    this.shortcuts_.set(command, new KeyboardShortcutList(shortcut));
  }

  /**
   * Minimize the set of |itemIds| by removing any node which has an ancestor
   * node already in the set. This ensures that instead of trying to delete
   * both a node and its descendant, we will only try to delete the topmost
   * node, preventing an error in the bookmarkManagerPrivate.removeTrees API
   * call.
   */
  private minimizeDeletionSet_(itemIds: Set<string>): Set<string> {
    const minimizedSet = new Set() as Set<string>;
    const nodes = this.getState().nodes;
    itemIds.forEach(function(itemId) {
      let currentId = itemId!;
      while (currentId !== ROOT_NODE_ID) {
        currentId = assert(nodes[currentId]!.parentId!);
        if (itemIds.has(currentId)) {
          return;
        }
      }
      minimizedSet.add(itemId);
    });
    return minimizedSet;
  }

  /**
   * Open the given |urls| in response to a |command|. May show a confirmation
   * dialog before opening large numbers of URLs.
   */
  private openUrls_(urls: Array<string>, command: Command) {
    assert(
        command === Command.OPEN || command === Command.OPEN_NEW_TAB ||
        command === Command.OPEN_NEW_WINDOW ||
        command === Command.OPEN_INCOGNITO);

    if (urls.length === 0) {
      return;
    }

    const openUrlsCallback = function() {
      const incognito = command === Command.OPEN_INCOGNITO;
      if (command === Command.OPEN_NEW_WINDOW || incognito) {
        chrome.windows.create({url: urls, incognito: incognito});
      } else {
        if (command === Command.OPEN) {
          chrome.tabs.create({url: urls.shift(), active: true});
        }
        urls.forEach(function(url) {
          chrome.tabs.create({url: url, active: false});
        });
      }
    };

    if (urls.length <= OPEN_CONFIRMATION_LIMIT) {
      openUrlsCallback();
      return;
    }

    this.confirmOpenCallback_ = openUrlsCallback;
    const dialog = this.$.openDialog.get();
    dialog.querySelector('[slot=body]')!.textContent =
        loadTimeData.getStringF('openDialogBody', urls.length);

    DialogFocusManager.getInstance().showDialog(this.$.openDialog.get());
  }

  /**
   * Returns all URLs in the given set of nodes and their immediate children.
   * Note that these will be ordered by insertion order into the |itemIds|
   * set, and that it is possible to duplicate a URL by passing in both the
   * parent ID and child ID.
   */
  private expandUrls_(itemIds: Set<string>): string[] {
    const urls: string[] = [];
    const nodes = this.getState().nodes;

    itemIds.forEach(function(id) {
      const node = nodes[id]!;
      if (node.url) {
        urls.push(node.url);
      } else {
        node.children!.forEach(function(childId) {
          const childNode = nodes[childId]!;
          if (childNode.url) {
            urls.push(childNode.url);
          }
        });
      }
    });

    return urls;
  }

  private containsMatchingNode_(
      itemIds: Set<string>, predicate: (p1: BookmarkNode) => boolean): boolean {
    const nodes = this.getState().nodes;

    return Array.from(itemIds).some(function(id) {
      return predicate(nodes[id]!);
    });
  }

  private isSingleBookmark_(itemIds: Set<string>): boolean {
    return itemIds.size === 1 &&
        this.containsMatchingNode_(itemIds, function(node) {
          return !!node.url;
        });
  }

  private isFolder_(itemIds: Set<string>): boolean {
    return itemIds.size === 1 &&
        this.containsMatchingNode_(itemIds, node => !node.url);
  }

  private getCommandLabel_(command: Command): string {
    // Handle non-pluralized strings first.
    let label = null;
    switch (command) {
      case Command.EDIT:
        if (this.menuIds_.size !== 1) {
          return '';
        }

        const id = Array.from(this.menuIds_)[0]!;
        const itemUrl = this.getState().nodes[id]!.url;
        label = itemUrl ? 'menuEdit' : 'menuRename';
        break;
      case Command.CUT:
        label = 'menuCut';
        break;
      case Command.COPY:
        label = 'menuCopy';
        break;
      case Command.COPY_URL:
        label = 'menuCopyURL';
        break;
      case Command.PASTE:
        label = 'menuPaste';
        break;
      case Command.DELETE:
        label = 'menuDelete';
        break;
      case Command.SHOW_IN_FOLDER:
        label = 'menuShowInFolder';
        break;
      case Command.SORT:
        label = 'menuSort';
        break;
      case Command.ADD_BOOKMARK:
        label = 'menuAddBookmark';
        break;
      case Command.ADD_FOLDER:
        label = 'menuAddFolder';
        break;
      case Command.IMPORT:
        label = 'menuImport';
        break;
      case Command.EXPORT:
        label = 'menuExport';
        break;
      case Command.HELP_CENTER:
        label = 'menuHelpCenter';
        break;
    }
    if (label !== null) {
      return loadTimeData.getString(assert(label));
    }

    // Handle pluralized strings.
    switch (command) {
      case Command.OPEN_NEW_TAB:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllNewTab', 'menuOpenNewTab',
            'menuOpenAllNewTabWithCount');
      case Command.OPEN_NEW_WINDOW:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllNewWindow', 'menuOpenNewWindow',
            'menuOpenAllNewWindowWithCount');
      case Command.OPEN_INCOGNITO:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllIncognito', 'menuOpenIncognito',
            'menuOpenAllIncognitoWithCount');
    }

    assertNotReached();
    return '';
  }

  private getPluralizedOpenAllString_(
      case0: string, case1: string, caseOther: string): string {
    const multipleNodes = this.menuIds_.size > 1 ||
        this.containsMatchingNode_(this.menuIds_, node => !node.url);

    const urls = this.expandUrls_(this.menuIds_);
    if (urls.length === 0) {
      return loadTimeData.getStringF(case0, urls.length);
    }

    if (urls.length === 1 && !multipleNodes) {
      return loadTimeData.getString(case1);
    }

    return loadTimeData.getStringF(caseOther, urls.length);
  }

  private getCommandSublabel_(command: Command): string {
    const multipleNodes = this.menuIds_.size > 1 ||
        this.containsMatchingNode_(this.menuIds_, function(node) {
          return !node.url;
        });
    switch (command) {
      case Command.OPEN_NEW_TAB:
        const urls = this.expandUrls_(this.menuIds_);
        return multipleNodes && urls.length > 0 ? String(urls.length) : '';
      default:
        return '';
    }
  }

  private computeMenuCommands_(): Command[] {
    switch (this.menuSource_) {
      case MenuSource.ITEM:
      case MenuSource.TREE:
        return [
          Command.EDIT,
          Command.SHOW_IN_FOLDER,
          Command.DELETE,
          // <hr>
          Command.CUT,
          Command.COPY,
          Command.COPY_URL,
          Command.PASTE,
          // <hr>
          Command.OPEN_NEW_TAB,
          Command.OPEN_NEW_WINDOW,
          Command.OPEN_INCOGNITO,
        ];
      case MenuSource.TOOLBAR:
        return [
          Command.SORT,
          // <hr>
          Command.ADD_BOOKMARK,
          Command.ADD_FOLDER,
          // <hr>
          Command.IMPORT,
          Command.EXPORT,
          // <hr>
          Command.HELP_CENTER,
        ];
      case MenuSource.LIST:
        return [
          Command.ADD_BOOKMARK,
          Command.ADD_FOLDER,
        ];
      case MenuSource.NONE:
        return [];
    }
    assert(false);
    return [];
  }

  private showDividerAfter_(command: Command, itemIds: Set<string>): boolean {
    switch (command) {
      case Command.SORT:
      case Command.ADD_FOLDER:
      case Command.EXPORT:
        return this.menuSource_ === MenuSource.TOOLBAR;
      case Command.DELETE:
        return this.globalCanEdit_;
      case Command.PASTE:
        return this.globalCanEdit_ || this.isSingleBookmark_(itemIds);
    }
    return false;
  }

  private recordCommandHistogram_(
      itemIds: Set<string>, histogram: string, command: number) {
    if (command === Command.OPEN) {
      command =
          this.isFolder_(itemIds) ? Command.OPEN_FOLDER : Command.OPEN_BOOKMARK;
    }

    this.browserProxy_.recordInHistogram(histogram, command, Command.MAX_VALUE);
  }

  /**
   * Show a toast with a bookmark |title| inserted into a label, with the
   * title ellipsised if necessary.
   */
  private async showTitleToast_(
      labelPromise: Promise<string>, title: string,
      canUndo: boolean): Promise<void> {
    const label = await labelPromise;
    const pieces =
        loadTimeData.getSubstitutedStringPieces(label, title).map(function(p) {
          // Make the bookmark name collapsible.
          const result =
              p as {value: string, arg: string, collapsible: boolean};
          result.collapsible = !!p.arg;
          return result;
        });
    getToastManager().querySelector('dom-if')!.if = canUndo;
    getToastManager().showForStringPieces(pieces);
  }

  private updateCanPaste_(targetId: string): Promise<void> {
    return new Promise(resolve => {
      chrome.bookmarkManagerPrivate.canPaste(`${targetId}`, result => {
        this.canPaste_ = result;
        resolve();
      });
    });
  }

  ////////////////////////////////////////////////////////////////////////////
  // Event handlers:

  private async onOpenCommandMenu_(
      e: CustomEvent<OpenCommandMenuDetail>): Promise<void> {
    if (e.detail.targetId) {
      await this.updateCanPaste_(e.detail.targetId);
    }
    if (e.detail.targetElement) {
      this.openCommandMenuAtElement(e.detail.targetElement!, e.detail.source);
    } else {
      this.openCommandMenuAtPosition(e.detail.x!, e.detail.y!, e.detail.source);
    }
    this.browserProxy_.recordInHistogram(
        'BookmarkManager.CommandMenuOpened', e.detail.source,
        MenuSource.NUM_VALUES);
  }

  private onCommandClick_(e: Event) {
    this.handle(
        Number((e.currentTarget as HTMLElement).getAttribute('command')) as
            Command,
        assert(this.menuIds_));
    this.closeCommandMenu();
  }

  private onKeydown_(e: Event) {
    const path = e.composedPath();
    if ((path[0] as HTMLElement).tagName === 'INPUT') {
      return;
    }
    if ((e.target === document.body ||
         path.some(
             el => (el as HTMLElement).tagName === 'BOOKMARKS-TOOLBAR')) &&
        !DialogFocusManager.getInstance().hasOpenDialog()) {
      this.handleKeyEvent(e, this.getState().selection.items);
    }
  }

  /**
   * Close the menu on mousedown so clicks can propagate to the underlying UI.
   * This allows the user to right click the list while a context menu is
   * showing and get another context menu.
   */
  private onMenuMousedown_(e: Event): void {
    if ((e.composedPath()[0] as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.closeCommandMenu();
  }

  private onOpenCancelTap_() {
    this.$.openDialog.get().cancel();
  }

  private onOpenConfirmTap_() {
    const confirmOpenCallback = assert(this.confirmOpenCallback_!);
    confirmOpenCallback();
    this.$.openDialog.get().close();
  }

  static getInstance(): BookmarksCommandManagerElement {
    return assert(instance)!;
  }
}

customElements.define(
    BookmarksCommandManagerElement.is, BookmarksCommandManagerElement);
