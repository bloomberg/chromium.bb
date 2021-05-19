/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Root from '../../core/root/root.js';

import {ActionDelegate as ActionDelegateInterface} from './ActionRegistration.js';  // eslint-disable-line no-unused-vars
import {Context} from './Context.js';          // eslint-disable-line no-unused-vars
import {ContextMenu} from './ContextMenu.js';  // eslint-disable-line no-unused-vars
import {Dialog} from './Dialog.js';
import {DockController, State} from './DockController.js';
import {GlassPane} from './GlassPane.js';
import {Icon} from './Icon.js';  // eslint-disable-line no-unused-vars
import {Infobar, Type as InfobarType} from './Infobar.js';
import {KeyboardShortcut} from './KeyboardShortcut.js';
import {Panel} from './Panel.js';  // eslint-disable-line no-unused-vars
import {SplitWidget} from './SplitWidget.js';
import {Events as TabbedPaneEvents} from './TabbedPane.js';
import {TabbedPane, TabbedPaneTabDelegate} from './TabbedPane.js';  // eslint-disable-line no-unused-vars
import {ToolbarButton} from './Toolbar.js';
import {View, ViewLocation, ViewLocationResolver} from './View.js';  // eslint-disable-line no-unused-vars
import {ViewManager} from './ViewManager.js';
import {VBox, Widget, WidgetFocusRestorer} from './Widget.js';  // eslint-disable-line no-unused-vars

const UIStrings = {
  /**
  *@description Title of more tabs button in inspector view
  */
  moreTools: 'More Tools',
  /**
  *@description Text that appears when hovor over the close button on the drawer view
  */
  closeDrawer: 'Close drawer',
  /**
  *@description The aria label for main tabbed pane that contains Panels
  */
  panels: 'Panels',
  /**
  *@description Title of an action that reloads the DevTools
  */
  reloadDevtools: 'Reload DevTools',
  /**
  *@description Text for context menu action to move a tab to the main panel
  */
  moveToTop: 'Move to top',
  /**
  *@description Text for context menu action to move a tab to the drawer
  */
  moveToBottom: 'Move to bottom',
};
const str_ = i18n.i18n.registerUIStrings('ui/legacy/InspectorView.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
/** @type {!InspectorView} */
let inspectorViewInstance;

/**
 * @implements {ViewLocationResolver}
 */
export class InspectorView extends VBox {
  constructor() {
    super();
    GlassPane.setContainer(this.element);
    this.setMinimumSize(240, 72);

    // DevTools sidebar is a vertical split of panels tabbed pane and a drawer.
    this._drawerSplitWidget = new SplitWidget(false, true, 'Inspector.drawerSplitViewState', 200, 200);
    this._drawerSplitWidget.hideSidebar();
    this._drawerSplitWidget.enableShowModeSaving();
    this._drawerSplitWidget.show(this.element);

    this._tabDelegate = new InspectorViewTabDelegate();

    // Create drawer tabbed pane.
    this._drawerTabbedLocation =
        ViewManager.instance().createTabbedLocation(this._showDrawer.bind(this, false), 'drawer-view', true, true);
    const moreTabsButton = this._drawerTabbedLocation.enableMoreTabsButton();
    moreTabsButton.setTitle(i18nString(UIStrings.moreTools));
    this._drawerTabbedPane = this._drawerTabbedLocation.tabbedPane();
    this._drawerTabbedPane.setMinimumSize(0, 27);
    this._drawerTabbedPane.element.classList.add('drawer-tabbed-pane');
    const closeDrawerButton = new ToolbarButton(i18nString(UIStrings.closeDrawer), 'largeicon-delete');
    closeDrawerButton.addEventListener(ToolbarButton.Events.Click, this._closeDrawer, this);
    this._drawerTabbedPane.addEventListener(TabbedPaneEvents.TabSelected, this._tabSelected, this);
    this._drawerTabbedPane.setTabDelegate(this._tabDelegate);

    this._drawerSplitWidget.installResizer(this._drawerTabbedPane.headerElement());
    this._drawerSplitWidget.setSidebarWidget(this._drawerTabbedPane);
    this._drawerTabbedPane.rightToolbar().appendToolbarItem(closeDrawerButton);

    /**
     * Lazily-initialized in {_attachReloadRequiredInfobar} because we only need it
     * if the InfoBar is presented.
     * @type {?HTMLDivElement}
     */
    this._infoBarDiv;

    // Create main area tabbed pane.
    this._tabbedLocation = ViewManager.instance().createTabbedLocation(
        Host.InspectorFrontendHost.InspectorFrontendHostInstance.bringToFront.bind(
            Host.InspectorFrontendHost.InspectorFrontendHostInstance),
        'panel', true, true, Root.Runtime.Runtime.queryParam('panel'));

    this._tabbedPane = this._tabbedLocation.tabbedPane();
    this._tabbedPane.element.classList.add('main-tabbed-pane');
    this._tabbedPane.registerRequiredCSS('ui/legacy/inspectorViewTabbedPane.css', {enableLegacyPatching: false});
    this._tabbedPane.addEventListener(TabbedPaneEvents.TabSelected, this._tabSelected, this);
    this._tabbedPane.setAccessibleName(i18nString(UIStrings.panels));
    this._tabbedPane.setTabDelegate(this._tabDelegate);

    // Store the initial selected panel for use in launch histograms
    Host.userMetrics.setLaunchPanel(this._tabbedPane.selectedTabId);

    if (Host.InspectorFrontendHost.isUnderTest()) {
      this._tabbedPane.setAutoSelectFirstItemOnShow(false);
    }
    this._drawerSplitWidget.setMainWidget(this._tabbedPane);

    this._keyDownBound = this._keyDown.bind(this);
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.events.addEventListener(
        Host.InspectorFrontendHostAPI.Events.ShowPanel, showPanel.bind(this));

    /**
     * @this {InspectorView}
     * @param {!Common.EventTarget.EventTargetEvent} event
     */
    function showPanel(event) {
      const panelName = /** @type {string} */ (event.data);
      this.showPanel(panelName);
    }
  }

  /**
   * @param {{forceNew: ?boolean}=} opts
   * @return {!InspectorView}
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!inspectorViewInstance || forceNew) {
      inspectorViewInstance = new InspectorView();
    }

    return inspectorViewInstance;
  }

  /**
   * @override
   */
  wasShown() {
    this.element.ownerDocument.addEventListener('keydown', this._keyDownBound, false);
  }

  /**
   * @override
   */
  willHide() {
    this.element.ownerDocument.removeEventListener('keydown', this._keyDownBound, false);
  }

  /**
   * @override
   * @param {string} locationName
   * @return {?ViewLocation}
   */
  resolveLocation(locationName) {
    if (locationName === 'drawer-view') {
      return this._drawerTabbedLocation;
    }
    if (locationName === 'panel') {
      return this._tabbedLocation;
    }
    return null;
  }

  async createToolbars() {
    await this._tabbedPane.leftToolbar().appendItemsAtLocation('main-toolbar-left');
    await this._tabbedPane.rightToolbar().appendItemsAtLocation('main-toolbar-right');
  }

  /**
   * @param {!View} view
   */
  addPanel(view) {
    this._tabbedLocation.appendView(view);
  }

  /**
   * @param {string} panelName
   * @return {boolean}
   */
  hasPanel(panelName) {
    return this._tabbedPane.hasTab(panelName);
  }

  /**
   * @param {string} panelName
   * @return {!Promise.<!Panel>}
   */
  async panel(panelName) {
    const view = ViewManager.instance().view(panelName);
    if (!view) {
      throw new Error(`Expected view for panel '${panelName}'`);
    }
    return /** @type {!Promise.<!Panel>} */ (view.widget());
  }

  /**
   * @param {boolean} allTargetsSuspended
   */
  onSuspendStateChanged(allTargetsSuspended) {
    this._currentPanelLocked = allTargetsSuspended;
    this._tabbedPane.setCurrentTabLocked(this._currentPanelLocked);
    this._tabbedPane.leftToolbar().setEnabled(!this._currentPanelLocked);
    this._tabbedPane.rightToolbar().setEnabled(!this._currentPanelLocked);
  }

  /**
   * @param {string} panelName
   * @return {boolean}
   */
  canSelectPanel(panelName) {
    return !this._currentPanelLocked || this._tabbedPane.selectedTabId === panelName;
  }

  /**
   * @param {string} panelName
   * @return {!Promise<void>}
   */
  async showPanel(panelName) {
    await ViewManager.instance().showView(panelName);
  }

  /**
   * @param {string} tabId
   * @param {?Icon} icon
   */
  setPanelIcon(tabId, icon) {
    // Find the tabbed location where the panel lives
    const tabbedPane = this._getTabbedPaneForTabId(tabId);
    if (tabbedPane) {
      tabbedPane.setTabIcon(tabId, icon);
    }
  }

  /**
   * @param {boolean} isDrawerOpen
   */
  _emitDrawerChangeEvent(isDrawerOpen) {
    const evt = new CustomEvent(Events.DrawerChange, {bubbles: true, cancelable: true, detail: {isDrawerOpen}});
    document.body.dispatchEvent(evt);
  }

  /**
   * @param {string} tabId
   * @return {?TabbedPane}
   */
  _getTabbedPaneForTabId(tabId) {
    // Tab exists in the main panel
    if (this._tabbedPane.hasTab(tabId)) {
      return this._tabbedPane;
    }

    // Tab exists in the drawer
    if (this._drawerTabbedPane.hasTab(tabId)) {
      return this._drawerTabbedPane;
    }

    // Tab is not open
    return null;
  }

  /**
   * @return {?Widget}
   */
  currentPanelDeprecated() {
    return (
        /** @type {?Widget} */ (ViewManager.instance().materializedWidget(this._tabbedPane.selectedTabId || '')));
  }

  /**
   * @param {boolean} focus
   */
  _showDrawer(focus) {
    if (this._drawerTabbedPane.isShowing()) {
      return;
    }
    this._drawerSplitWidget.showBoth();
    if (focus) {
      this._focusRestorer = new WidgetFocusRestorer(this._drawerTabbedPane);
    } else {
      this._focusRestorer = null;
    }
    this._emitDrawerChangeEvent(true);
  }

  /**
   * @return {boolean}
   */
  drawerVisible() {
    return this._drawerTabbedPane.isShowing();
  }

  _closeDrawer() {
    if (!this._drawerTabbedPane.isShowing()) {
      return;
    }
    if (this._focusRestorer) {
      this._focusRestorer.restore();
    }
    this._drawerSplitWidget.hideSidebar(true);

    this._emitDrawerChangeEvent(false);
  }

  /**
   * @param {boolean} minimized
   */
  setDrawerMinimized(minimized) {
    this._drawerSplitWidget.setSidebarMinimized(minimized);
    this._drawerSplitWidget.setResizable(!minimized);
  }

  /**
   * @return {boolean}
   */
  isDrawerMinimized() {
    return this._drawerSplitWidget.isSidebarMinimized();
  }

  /**
   * @param {string} id
   * @param {boolean=} userGesture
   */
  closeDrawerTab(id, userGesture) {
    this._drawerTabbedPane.closeTab(id, userGesture);
    Host.userMetrics.panelClosed(id);
  }

  /**
   * @param {!Event} event
   */
  _keyDown(event) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
    if (!KeyboardShortcut.eventHasCtrlOrMeta(keyboardEvent) || keyboardEvent.altKey || keyboardEvent.shiftKey) {
      return;
    }

    // Ctrl/Cmd + 1-9 should show corresponding panel.
    const panelShortcutEnabled = Common.Settings.moduleSetting('shortcutPanelSwitch').get();
    if (panelShortcutEnabled) {
      let panelIndex = -1;
      if (keyboardEvent.keyCode > 0x30 && keyboardEvent.keyCode < 0x3A) {
        panelIndex = keyboardEvent.keyCode - 0x31;
      } else if (
          keyboardEvent.keyCode > 0x60 && keyboardEvent.keyCode < 0x6A &&
          keyboardEvent.location === KeyboardEvent.DOM_KEY_LOCATION_NUMPAD) {
        panelIndex = keyboardEvent.keyCode - 0x61;
      }
      if (panelIndex !== -1) {
        const panelName = this._tabbedPane.tabIds()[panelIndex];
        if (panelName) {
          if (!Dialog.hasInstance() && !this._currentPanelLocked) {
            this.showPanel(panelName);
          }
          event.consume(true);
        }
      }
    }
  }

  /**
   * @override
   */
  onResize() {
    GlassPane.containerMoved(this.element);
  }

  /**
   * @return {!Element}
   */
  topResizerElement() {
    return this._tabbedPane.headerElement();
  }

  toolbarItemResized() {
    this._tabbedPane.headerResized();
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _tabSelected(event) {
    const tabId = /** @type {string} */ (event.data['tabId']);
    Host.userMetrics.panelShown(tabId);
  }

  /**
   * @param {!SplitWidget} splitWidget
   */
  setOwnerSplit(splitWidget) {
    this._ownerSplitWidget = splitWidget;
  }

  /**
   * @return {?SplitWidget}
   */
  ownerSplit() {
    return this._ownerSplitWidget || null;
  }

  minimize() {
    if (this._ownerSplitWidget) {
      this._ownerSplitWidget.setSidebarMinimized(true);
    }
  }

  restore() {
    if (this._ownerSplitWidget) {
      this._ownerSplitWidget.setSidebarMinimized(false);
    }
  }

  /**
   * @param {string} message
   */
  displayReloadRequiredWarning(message) {
    if (!this._reloadRequiredInfobar) {
      const infobar = new Infobar(InfobarType.Info, message, [
        {
          text: i18nString(UIStrings.reloadDevtools),
          highlight: true,
          delegate: () => {
            if (DockController.instance().canDock() && DockController.instance().dockSide() === State.Undocked) {
              Host.InspectorFrontendHost.InspectorFrontendHostInstance.setIsDocked(true, function() {});
            }
            Host.InspectorFrontendHost.InspectorFrontendHostInstance.reattach(() => window.location.reload());
          },
          dismiss: false
        },
      ]);
      infobar.setParentView(this);
      this._attachReloadRequiredInfobar(infobar);
      this._reloadRequiredInfobar = infobar;
      infobar.setCloseCallback(() => {
        delete this._reloadRequiredInfobar;
      });
    }
  }

  /**
   * @param {!Infobar} infobar
   */
  _attachReloadRequiredInfobar(infobar) {
    if (!this._infoBarDiv) {
      this._infoBarDiv = /** @type {!HTMLDivElement} */ (document.createElement('div'));
      this._infoBarDiv.classList.add('flex-none');
      this.contentElement.insertBefore(this._infoBarDiv, this.contentElement.firstChild);
    }
    this._infoBarDiv.appendChild(infobar.element);
  }
}

/** @type {!ActionDelegate} */
let actionDelegateInstance;

/**
 * @implements {ActionDelegateInterface}
 */
export class ActionDelegate {
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!actionDelegateInstance || forceNew) {
      actionDelegateInstance = new ActionDelegate();
    }

    return actionDelegateInstance;
  }

  /**
   * @override
   * @param {!Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    switch (actionId) {
      case 'main.toggle-drawer':
        if (InspectorView.instance().drawerVisible()) {
          InspectorView.instance()._closeDrawer();
        } else {
          InspectorView.instance()._showDrawer(true);
        }
        return true;
      case 'main.next-tab':
        InspectorView.instance()._tabbedPane.selectNextTab();
        InspectorView.instance()._tabbedPane.focus();
        return true;
      case 'main.previous-tab':
        InspectorView.instance()._tabbedPane.selectPrevTab();
        InspectorView.instance()._tabbedPane.focus();
        return true;
    }
    return false;
  }
}

/**
 * @implements {TabbedPaneTabDelegate}
 */
export class InspectorViewTabDelegate {
  /**
   * @override
   * @param {!TabbedPane} tabbedPane
   * @param {!Array.<string>} ids
   */
  closeTabs(tabbedPane, ids) {
    tabbedPane.closeTabs(ids, true);
    // Log telemetry about the closure
    ids.forEach(id => {
      Host.userMetrics.panelClosed(id);
    });
  }

  /**
   * @param {string} tabId
   */
  moveToDrawer(tabId) {
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.TabMovedToDrawer);
    ViewManager.instance().moveView(tabId, 'drawer-view');
  }

  /**
   * @param {string} tabId
   */
  moveToMainPanel(tabId) {
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.TabMovedToMainPanel);
    ViewManager.instance().moveView(tabId, 'panel');
  }

  /**
   * @override
   * @param {string} tabId
   * @param {!ContextMenu} contextMenu
   */
  onContextMenu(tabId, contextMenu) {
    // Special case for console, we don't show the movable context panel for this two tabs
    if (tabId === 'console' || tabId === 'console-view') {
      return;
    }

    const locationName = ViewManager.instance().locationNameForViewId(tabId);
    if (locationName === 'drawer-view') {
      contextMenu.defaultSection().appendItem(i18nString(UIStrings.moveToTop), this.moveToMainPanel.bind(this, tabId));
    } else {
      contextMenu.defaultSection().appendItem(i18nString(UIStrings.moveToBottom), this.moveToDrawer.bind(this, tabId));
    }
  }
}

/** @enum {string} */
export const Events = {
  DrawerChange: 'drawerchange',
};
