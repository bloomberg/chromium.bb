// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as Extensions from '../../models/extensions/extensions.js';
import * as Workspace from '../../models/workspace/workspace.js';
import * as ObjectUI from '../../ui/legacy/components/object_ui/object_ui.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Snippets from '../snippets/snippets.js';

import {CallStackSidebarPane} from './CallStackSidebarPane.js';
import {DebuggerPausedMessage} from './DebuggerPausedMessage.js';
import sourcesPanelStyles from './sourcesPanel.css.js';

import type {NavigatorView} from './NavigatorView.js';
import {ContentScriptsNavigatorView, FilesNavigatorView, NetworkNavigatorView, OverridesNavigatorView, SnippetsNavigatorView} from './SourcesNavigator.js';
import {Events, SourcesView} from './SourcesView.js';
import {ThreadsSidebarPane} from './ThreadsSidebarPane.js';
import {UISourceCodeFrame} from './UISourceCodeFrame.js';

const UIStrings = {
  /**
  *@description Text that appears when user drag and drop something (for example, a file) in Sources Panel of the Sources panel
  */
  dropWorkspaceFolderHere: 'Drop workspace folder here',
  /**
  *@description Text to show more options
  */
  moreOptions: 'More options',
  /**
  * @description Tooltip for the the navigator toggle in the Sources panel. Command to open/show the
  * sidebar containing the navigator tool.
  */
  showNavigator: 'Show navigator',
  /**
  * @description Tooltip for the the navigator toggle in the Sources panel. Command to close/hide
  * the sidebar containing the navigator tool.
  */
  hideNavigator: 'Hide navigator',
  /**
  * @description Tooltip for the the debugger toggle in the Sources panel. Command to open/show the
  * sidebar containing the debugger tool.
  */
  showDebugger: 'Show debugger',
  /**
  * @description Tooltip for the the debugger toggle in the Sources panel. Command to close/hide the
  * sidebar containing the debugger tool.
  */
  hideDebugger: 'Hide debugger',
  /**
  *@description Text in Sources Panel of the Sources panel
  */
  groupByFolder: 'Group by folder',
  /**
  *@description Text for pausing the debugger on exceptions
  */
  pauseOnExceptions: 'Pause on exceptions',
  /**
  *@description Text in Sources Panel of the Sources panel
  */
  dontPauseOnExceptions: 'Don\'t pause on exceptions',
  /**
  *@description Tooltip text that appears when hovering over the largeicon play button in the Sources Panel of the Sources panel
  */
  resumeWithAllPausesBlockedForMs: 'Resume with all pauses blocked for 500 ms',
  /**
  *@description Tooltip text that appears when hovering over the largeicon terminate execution button in the Sources Panel of the Sources panel
  */
  terminateCurrentJavascriptCall: 'Terminate current JavaScript call',
  /**
  *@description Text in Sources Panel of the Sources panel
  */
  pauseOnCaughtExceptions: 'Pause on caught exceptions',
  /**
  *@description A context menu item in the Sources Panel of the Sources panel
  */
  revealInSidebar: 'Reveal in sidebar',
  /**
  *@description A context menu item in the Sources Panel of the Sources panel when debugging JS code.
  * When clicked, the execution is resumed until it reaches the line specified by the right-click that
  * opened the context menu.
  */
  continueToHere: 'Continue to here',
  /**
  *@description A context menu item in the Console that stores selection as a temporary global variable
  *@example {string} PH1
  */
  storeSAsGlobalVariable: 'Store {PH1} as global variable',
  /**
  *@description A context menu item in the Console, Sources, and Network panel
  *@example {string} PH1
  */
  copyS: 'Copy {PH1}',
  /**
  *@description A context menu item for strings in the Console, Sources, and Network panel.
  * When clicked, the raw contents of the string is copied to the clipboard.
  */
  copyStringContents: 'Copy string contents',
  /**
  *@description A context menu item for strings in the Console, Sources, and Network panel.
  * When clicked, the string is copied to the clipboard as a valid JavaScript literal.
  */
  copyStringAsJSLiteral: 'Copy string as JavaScript literal',
  /**
  *@description A context menu item for strings in the Console, Sources, and Network panel.
  * When clicked, the string is copied to the clipboard as a valid JSON literal.
  */
  copyStringAsJSONLiteral: 'Copy string as JSON literal',
  /**
  *@description A context menu item in the Sources Panel of the Sources panel
  */
  showFunctionDefinition: 'Show function definition',
  /**
  *@description Text in Sources Panel of the Sources panel
  */
  openInSourcesPanel: 'Open in Sources panel',
};
const str_ = i18n.i18n.registerUIStrings('panels/sources/SourcesPanel.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
const primitiveRemoteObjectTypes = new Set(['number', 'boolean', 'bigint', 'undefined']);
let sourcesPanelInstance: SourcesPanel;
let wrapperViewInstance: WrapperView;

export class SourcesPanel extends UI.Panel.Panel implements UI.ContextMenu.Provider, SDK.TargetManager.Observer,
                                                            UI.View.ViewLocationResolver {
  private readonly workspace: Workspace.Workspace.WorkspaceImpl;
  private readonly togglePauseAction: UI.ActionRegistration.Action;
  private readonly stepOverAction: UI.ActionRegistration.Action;
  private readonly stepIntoAction: UI.ActionRegistration.Action;
  private readonly stepOutAction: UI.ActionRegistration.Action;
  private readonly stepAction: UI.ActionRegistration.Action;
  private readonly toggleBreakpointsActiveAction: UI.ActionRegistration.Action;
  private readonly debugToolbar: UI.Toolbar.Toolbar;
  private readonly debugToolbarDrawer: HTMLDivElement;
  private readonly debuggerPausedMessage: DebuggerPausedMessage;
  private splitWidget: UI.SplitWidget.SplitWidget;
  editorView: UI.SplitWidget.SplitWidget;
  private navigatorTabbedLocation: UI.View.TabbedViewLocation;
  sourcesViewInternal: SourcesView;
  private readonly toggleNavigatorSidebarButton: UI.Toolbar.ToolbarButton;
  private readonly toggleDebuggerSidebarButton: UI.Toolbar.ToolbarButton;
  private threadsSidebarPane: UI.View.View|null;
  private readonly watchSidebarPane: UI.View.View;
  private readonly callstackPane: CallStackSidebarPane;
  private liveLocationPool: Bindings.LiveLocation.LiveLocationPool;
  private lastModificationTime: number;
  private pausedInternal?: boolean;
  private switchToPausedTargetTimeout?: number;
  private ignoreExecutionLineEvents?: boolean;
  private executionLineLocation?: Bindings.DebuggerWorkspaceBinding.Location|null;
  private pauseOnExceptionButton?: UI.Toolbar.ToolbarToggle;
  private sidebarPaneStack?: UI.View.ViewLocation;
  private tabbedLocationHeader?: Element|null;
  private extensionSidebarPanesContainer?: UI.View.ViewLocation;
  sidebarPaneView?: UI.Widget.VBox|UI.SplitWidget.SplitWidget;
  constructor() {
    super('sources');

    new UI.DropTarget.DropTarget(
        this.element, [UI.DropTarget.Type.Folder], i18nString(UIStrings.dropWorkspaceFolderHere),
        this.handleDrop.bind(this));

    this.workspace = Workspace.Workspace.WorkspaceImpl.instance();
    this.togglePauseAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.toggle-pause') as UI.ActionRegistration.Action);
    this.stepOverAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.step-over') as UI.ActionRegistration.Action);
    this.stepIntoAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.step-into') as UI.ActionRegistration.Action);
    this.stepOutAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.step-out') as UI.ActionRegistration.Action);
    this.stepAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.step') as UI.ActionRegistration.Action);
    this.toggleBreakpointsActiveAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('debugger.toggle-breakpoints-active') as
         UI.ActionRegistration.Action);

    this.debugToolbar = this.createDebugToolbar();
    this.debugToolbarDrawer = this.createDebugToolbarDrawer();
    this.debuggerPausedMessage = new DebuggerPausedMessage();

    const initialDebugSidebarWidth = 225;
    this.splitWidget =
        new UI.SplitWidget.SplitWidget(true, true, 'sourcesPanelSplitViewState', initialDebugSidebarWidth);
    this.splitWidget.enableShowModeSaving();
    this.splitWidget.show(this.element);

    // Create scripts navigator
    const initialNavigatorWidth = 225;
    this.editorView =
        new UI.SplitWidget.SplitWidget(true, false, 'sourcesPanelNavigatorSplitViewState', initialNavigatorWidth);
    this.editorView.enableShowModeSaving();
    this.splitWidget.setMainWidget(this.editorView);

    // Create navigator tabbed pane with toolbar.
    this.navigatorTabbedLocation = UI.ViewManager.ViewManager.instance().createTabbedLocation(
        this.revealNavigatorSidebar.bind(this), 'navigator-view', true);
    const tabbedPane = this.navigatorTabbedLocation.tabbedPane();
    tabbedPane.setMinimumSize(100, 25);
    tabbedPane.element.classList.add('navigator-tabbed-pane');
    const navigatorMenuButton = new UI.Toolbar.ToolbarMenuButton(this.populateNavigatorMenu.bind(this), true);
    navigatorMenuButton.setTitle(i18nString(UIStrings.moreOptions));
    tabbedPane.rightToolbar().appendToolbarItem(navigatorMenuButton);

    if (UI.ViewManager.ViewManager.instance().hasViewsForLocation('run-view-sidebar')) {
      const navigatorSplitWidget =
          new UI.SplitWidget.SplitWidget(false, true, 'sourcePanelNavigatorSidebarSplitViewState');
      navigatorSplitWidget.setMainWidget(tabbedPane);
      const runViewTabbedPane = UI.ViewManager.ViewManager.instance()
                                    .createTabbedLocation(this.revealNavigatorSidebar.bind(this), 'run-view-sidebar')
                                    .tabbedPane();
      navigatorSplitWidget.setSidebarWidget(runViewTabbedPane);
      navigatorSplitWidget.installResizer(runViewTabbedPane.headerElement());
      this.editorView.setSidebarWidget(navigatorSplitWidget);
    } else {
      this.editorView.setSidebarWidget(tabbedPane);
    }

    this.sourcesViewInternal = new SourcesView();
    this.sourcesViewInternal.addEventListener(Events.EditorSelected, this.editorSelected.bind(this));

    this.toggleNavigatorSidebarButton = this.editorView.createShowHideSidebarButton(
        i18nString(UIStrings.showNavigator), i18nString(UIStrings.hideNavigator));
    this.toggleDebuggerSidebarButton = this.splitWidget.createShowHideSidebarButton(
        i18nString(UIStrings.showDebugger), i18nString(UIStrings.hideDebugger));
    this.editorView.setMainWidget(this.sourcesViewInternal);

    this.threadsSidebarPane = null;
    this.watchSidebarPane = (UI.ViewManager.ViewManager.instance().view('sources.watch') as UI.View.View);
    this.callstackPane = CallStackSidebarPane.instance();

    Common.Settings.Settings.instance()
        .moduleSetting('sidebarPosition')
        .addChangeListener(this.updateSidebarPosition.bind(this));
    this.updateSidebarPosition();

    this.updateDebuggerButtonsAndStatus();
    this.pauseOnExceptionEnabledChanged();
    Common.Settings.Settings.instance()
        .moduleSetting('pauseOnExceptionEnabled')
        .addChangeListener(this.pauseOnExceptionEnabledChanged, this);

    this.liveLocationPool = new Bindings.LiveLocation.LiveLocationPool();

    this.setTarget(UI.Context.Context.instance().flavor(SDK.Target.Target));
    Common.Settings.Settings.instance()
        .moduleSetting('breakpointsActive')
        .addChangeListener(this.breakpointsActiveStateChanged, this);
    UI.Context.Context.instance().addFlavorChangeListener(SDK.Target.Target, this.onCurrentTargetChanged, this);
    UI.Context.Context.instance().addFlavorChangeListener(SDK.DebuggerModel.CallFrame, this.callFrameChanged, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.DebuggerModel.DebuggerModel, SDK.DebuggerModel.Events.DebuggerWasEnabled, this.debuggerWasEnabled, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.DebuggerModel.DebuggerModel, SDK.DebuggerModel.Events.DebuggerPaused, this.debuggerPaused, this);
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.DebuggerModel.DebuggerModel, SDK.DebuggerModel.Events.DebuggerResumed,
        event => this.debuggerResumed(event.data));
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.DebuggerModel.DebuggerModel, SDK.DebuggerModel.Events.GlobalObjectCleared,
        event => this.debuggerResumed(event.data));
    Extensions.ExtensionServer.ExtensionServer.instance().addEventListener(
        Extensions.ExtensionServer.Events.SidebarPaneAdded, this.extensionSidebarPaneAdded, this);
    SDK.TargetManager.TargetManager.instance().observeTargets(this);
    this.lastModificationTime = window.performance.now();
  }

  static instance(opts: {
    forceNew: boolean|null,
  }|undefined = {forceNew: null}): SourcesPanel {
    const {forceNew} = opts;
    if (!sourcesPanelInstance || forceNew) {
      sourcesPanelInstance = new SourcesPanel();
    }

    return sourcesPanelInstance;
  }

  static updateResizerAndSidebarButtons(panel: SourcesPanel): void {
    panel.sourcesViewInternal.leftToolbar().removeToolbarItems();
    panel.sourcesViewInternal.rightToolbar().removeToolbarItems();
    panel.sourcesViewInternal.bottomToolbar().removeToolbarItems();
    const isInWrapper = WrapperView.isShowing() && !UI.InspectorView.InspectorView.instance().isDrawerMinimized();
    if (panel.splitWidget.isVertical() || isInWrapper) {
      panel.splitWidget.uninstallResizer(panel.sourcesViewInternal.toolbarContainerElement());
    } else {
      panel.splitWidget.installResizer(panel.sourcesViewInternal.toolbarContainerElement());
    }
    if (!isInWrapper) {
      panel.sourcesViewInternal.leftToolbar().appendToolbarItem(panel.toggleNavigatorSidebarButton);
      if (panel.splitWidget.isVertical()) {
        panel.sourcesViewInternal.rightToolbar().appendToolbarItem(panel.toggleDebuggerSidebarButton);
      } else {
        panel.sourcesViewInternal.bottomToolbar().appendToolbarItem(panel.toggleDebuggerSidebarButton);
      }
    }
  }

  targetAdded(_target: SDK.Target.Target): void {
    this.showThreadsIfNeeded();
  }

  targetRemoved(_target: SDK.Target.Target): void {
  }

  private showThreadsIfNeeded(): void {
    if (ThreadsSidebarPane.shouldBeShown() && !this.threadsSidebarPane) {
      this.threadsSidebarPane = (UI.ViewManager.ViewManager.instance().view('sources.threads') as UI.View.View);
      if (this.sidebarPaneStack && this.threadsSidebarPane) {
        this.sidebarPaneStack.showView(
            this.threadsSidebarPane, this.splitWidget.isVertical() ? this.watchSidebarPane : this.callstackPane);
      }
    }
  }

  private setTarget(target: SDK.Target.Target|null): void {
    if (!target) {
      return;
    }
    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
    if (!debuggerModel) {
      return;
    }

    if (debuggerModel.isPaused()) {
      this.showDebuggerPausedDetails(
          (debuggerModel.debuggerPausedDetails() as SDK.DebuggerModel.DebuggerPausedDetails));
    } else {
      this.pausedInternal = false;
      this.clearInterface();
      this.toggleDebuggerSidebarButton.setEnabled(true);
    }
  }

  private onCurrentTargetChanged({data: target}: Common.EventTarget.EventTargetEvent<SDK.Target.Target|null>): void {
    this.setTarget(target);
  }
  paused(): boolean {
    return this.pausedInternal || false;
  }

  wasShown(): void {
    UI.Context.Context.instance().setFlavor(SourcesPanel, this);
    this.registerCSSFiles([sourcesPanelStyles]);
    super.wasShown();
    const wrapper = WrapperView.instance();
    if (wrapper && wrapper.isShowing()) {
      UI.InspectorView.InspectorView.instance().setDrawerMinimized(true);
      SourcesPanel.updateResizerAndSidebarButtons(this);
    }
    this.editorView.setMainWidget(this.sourcesViewInternal);
  }

  willHide(): void {
    super.willHide();
    UI.Context.Context.instance().setFlavor(SourcesPanel, null);
    if (WrapperView.isShowing()) {
      WrapperView.instance().showViewInWrapper();
      UI.InspectorView.InspectorView.instance().setDrawerMinimized(false);
      SourcesPanel.updateResizerAndSidebarButtons(this);
    }
  }

  resolveLocation(locationName: string): UI.View.ViewLocation|null {
    if (locationName === 'sources.sidebar-top' || locationName === 'sources.sidebar-bottom' ||
        locationName === 'sources.sidebar-tabs') {
      return this.sidebarPaneStack || null;
    }
    return this.navigatorTabbedLocation;
  }

  ensureSourcesViewVisible(): boolean {
    if (WrapperView.isShowing()) {
      return true;
    }
    if (!UI.InspectorView.InspectorView.instance().canSelectPanel('sources')) {
      return false;
    }
    UI.ViewManager.ViewManager.instance().showView('sources');
    return true;
  }

  onResize(): void {
    if (Common.Settings.Settings.instance().moduleSetting('sidebarPosition').get() === 'auto') {
      this.element.window().requestAnimationFrame(this.updateSidebarPosition.bind(this));
    }  // Do not force layout.
  }

  searchableView(): UI.SearchableView.SearchableView {
    return this.sourcesViewInternal.searchableView();
  }

  private debuggerPaused(event: Common.EventTarget.EventTargetEvent<SDK.DebuggerModel.DebuggerModel>): void {
    const debuggerModel = event.data;
    const details = debuggerModel.debuggerPausedDetails();
    if (!this.pausedInternal) {
      this.setAsCurrentPanel();
    }

    if (UI.Context.Context.instance().flavor(SDK.Target.Target) === debuggerModel.target()) {
      this.showDebuggerPausedDetails((details as SDK.DebuggerModel.DebuggerPausedDetails));
    } else if (!this.pausedInternal) {
      UI.Context.Context.instance().setFlavor(SDK.Target.Target, debuggerModel.target());
    }
  }

  private showDebuggerPausedDetails(details: SDK.DebuggerModel.DebuggerPausedDetails): void {
    this.pausedInternal = true;
    this.updateDebuggerButtonsAndStatus();
    UI.Context.Context.instance().setFlavor(SDK.DebuggerModel.DebuggerPausedDetails, details);
    this.toggleDebuggerSidebarButton.setEnabled(false);
    this.revealDebuggerSidebar();
    window.focus();
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.bringToFront();
  }

  private debuggerResumed(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    const target = debuggerModel.target();
    if (UI.Context.Context.instance().flavor(SDK.Target.Target) !== target) {
      return;
    }
    this.pausedInternal = false;
    this.clearInterface();
    this.toggleDebuggerSidebarButton.setEnabled(true);
    this.switchToPausedTargetTimeout = window.setTimeout(this.switchToPausedTarget.bind(this, debuggerModel), 500);
  }

  private debuggerWasEnabled(event: Common.EventTarget.EventTargetEvent<SDK.DebuggerModel.DebuggerModel>): void {
    const debuggerModel = event.data;
    if (UI.Context.Context.instance().flavor(SDK.Target.Target) !== debuggerModel.target()) {
      return;
    }

    this.updateDebuggerButtonsAndStatus();
  }

  get visibleView(): UI.Widget.Widget|null {
    return this.sourcesViewInternal.visibleView();
  }

  showUISourceCode(
      uiSourceCode: Workspace.UISourceCode.UISourceCode, lineNumber?: number, columnNumber?: number,
      omitFocus?: boolean): void {
    if (omitFocus) {
      const wrapperShowing = WrapperView.isShowing();
      if (!this.isShowing() && !wrapperShowing) {
        return;
      }
    } else {
      this.showEditor();
    }
    this.sourcesViewInternal.showSourceLocation(uiSourceCode, lineNumber, columnNumber, omitFocus);
  }

  private showEditor(): void {
    if (WrapperView.isShowing()) {
      return;
    }
    this.setAsCurrentPanel();
  }

  showUILocation(uiLocation: Workspace.UISourceCode.UILocation, omitFocus?: boolean): void {
    this.showUISourceCode(uiLocation.uiSourceCode, uiLocation.lineNumber, uiLocation.columnNumber, omitFocus);
  }

  private revealInNavigator(uiSourceCode: Workspace.UISourceCode.UISourceCode, skipReveal?: boolean): void {
    for (const navigator of registeredNavigatorViews) {
      const navigatorView = navigator.navigatorView();
      const viewId = navigator.viewId;
      if (viewId && navigatorView.acceptProject(uiSourceCode.project())) {
        navigatorView.revealUISourceCode(uiSourceCode, true);
        if (skipReveal) {
          this.navigatorTabbedLocation.tabbedPane().selectTab(viewId);
        } else {
          UI.ViewManager.ViewManager.instance().showView(viewId);
        }
      }
    }
  }

  private populateNavigatorMenu(contextMenu: UI.ContextMenu.ContextMenu): void {
    const groupByFolderSetting = Common.Settings.Settings.instance().moduleSetting('navigatorGroupByFolder');
    contextMenu.appendItemsAtLocation('navigatorMenu');
    contextMenu.viewSection().appendCheckboxItem(
        i18nString(UIStrings.groupByFolder), () => groupByFolderSetting.set(!groupByFolderSetting.get()),
        groupByFolderSetting.get());
  }

  setIgnoreExecutionLineEvents(ignoreExecutionLineEvents: boolean): void {
    this.ignoreExecutionLineEvents = ignoreExecutionLineEvents;
  }

  updateLastModificationTime(): void {
    this.lastModificationTime = window.performance.now();
  }

  private async executionLineChanged(liveLocation: Bindings.LiveLocation.LiveLocation): Promise<void> {
    const uiLocation = await liveLocation.uiLocation();
    if (!uiLocation) {
      return;
    }
    if (window.performance.now() - this.lastModificationTime < lastModificationTimeout) {
      return;
    }
    this.sourcesViewInternal.showSourceLocation(
        uiLocation.uiSourceCode, uiLocation.lineNumber, uiLocation.columnNumber, undefined, true);
  }

  private lastModificationTimeoutPassedForTest(): void {
    lastModificationTimeout = Number.MIN_VALUE;
  }

  private updateLastModificationTimeForTest(): void {
    lastModificationTimeout = Number.MAX_VALUE;
  }

  private async callFrameChanged(): Promise<void> {
    const callFrame = UI.Context.Context.instance().flavor(SDK.DebuggerModel.CallFrame);
    if (!callFrame) {
      return;
    }
    if (this.executionLineLocation) {
      this.executionLineLocation.dispose();
    }
    this.executionLineLocation =
        await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().createCallFrameLiveLocation(
            callFrame.location(), this.executionLineChanged.bind(this), this.liveLocationPool);
  }

  private pauseOnExceptionEnabledChanged(): void {
    const enabled = Common.Settings.Settings.instance().moduleSetting('pauseOnExceptionEnabled').get();
    const button = (this.pauseOnExceptionButton as UI.Toolbar.ToolbarToggle);
    button.setToggled(enabled);
    button.setTitle(enabled ? i18nString(UIStrings.pauseOnExceptions) : i18nString(UIStrings.dontPauseOnExceptions));
    this.debugToolbarDrawer.classList.toggle('expanded', enabled);
  }

  private async updateDebuggerButtonsAndStatus(): Promise<void> {
    const currentTarget = UI.Context.Context.instance().flavor(SDK.Target.Target);
    const currentDebuggerModel = currentTarget ? currentTarget.model(SDK.DebuggerModel.DebuggerModel) : null;
    if (!currentDebuggerModel) {
      this.togglePauseAction.setEnabled(false);
      this.stepOverAction.setEnabled(false);
      this.stepIntoAction.setEnabled(false);
      this.stepOutAction.setEnabled(false);
      this.stepAction.setEnabled(false);
    } else if (this.pausedInternal) {
      this.togglePauseAction.setToggled(true);
      this.togglePauseAction.setEnabled(true);
      this.stepOverAction.setEnabled(true);
      this.stepIntoAction.setEnabled(true);
      this.stepOutAction.setEnabled(true);
      this.stepAction.setEnabled(true);
    } else {
      this.togglePauseAction.setToggled(false);
      this.togglePauseAction.setEnabled(!currentDebuggerModel.isPausing());
      this.stepOverAction.setEnabled(false);
      this.stepIntoAction.setEnabled(false);
      this.stepOutAction.setEnabled(false);
      this.stepAction.setEnabled(false);
    }

    const details = currentDebuggerModel ? currentDebuggerModel.debuggerPausedDetails() : null;
    await this.debuggerPausedMessage.render(
        details, Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance(),
        Bindings.BreakpointManager.BreakpointManager.instance());
    if (details) {
      this.updateDebuggerButtonsAndStatusForTest();
    }
  }

  private updateDebuggerButtonsAndStatusForTest(): void {
  }

  private clearInterface(): void {
    this.updateDebuggerButtonsAndStatus();
    UI.Context.Context.instance().setFlavor(SDK.DebuggerModel.DebuggerPausedDetails, null);

    if (this.switchToPausedTargetTimeout) {
      clearTimeout(this.switchToPausedTargetTimeout);
    }
    this.liveLocationPool.disposeAll();
  }

  private switchToPausedTarget(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    delete this.switchToPausedTargetTimeout;
    if (this.pausedInternal || debuggerModel.isPaused()) {
      return;
    }

    for (const debuggerModel of SDK.TargetManager.TargetManager.instance().models(SDK.DebuggerModel.DebuggerModel)) {
      if (debuggerModel.isPaused()) {
        UI.Context.Context.instance().setFlavor(SDK.Target.Target, debuggerModel.target());
        break;
      }
    }
  }

  private togglePauseOnExceptions(): void {
    Common.Settings.Settings.instance()
        .moduleSetting('pauseOnExceptionEnabled')
        // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
        // @ts-expect-error
        .set(!(this.pauseOnExceptionButton).toggled());
  }

  runSnippet(): void {
    const uiSourceCode = this.sourcesViewInternal.currentUISourceCode();
    if (uiSourceCode) {
      Snippets.ScriptSnippetFileSystem.evaluateScriptSnippet(uiSourceCode);
    }
  }

  private editorSelected(event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>): void {
    const uiSourceCode = event.data;
    if (this.editorView.mainWidget() &&
        Common.Settings.Settings.instance().moduleSetting('autoRevealInNavigator').get()) {
      this.revealInNavigator(uiSourceCode, true);
    }
  }

  togglePause(): boolean {
    const target = UI.Context.Context.instance().flavor(SDK.Target.Target);
    if (!target) {
      return true;
    }
    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
    if (!debuggerModel) {
      return true;
    }

    if (this.pausedInternal) {
      this.pausedInternal = false;
      debuggerModel.resume();
    } else {
      // Make sure pauses didn't stick skipped.
      debuggerModel.pause();
    }

    this.clearInterface();
    return true;
  }

  private prepareToResume(): SDK.DebuggerModel.DebuggerModel|null {
    if (!this.pausedInternal) {
      return null;
    }

    this.pausedInternal = false;

    this.clearInterface();
    const target = UI.Context.Context.instance().flavor(SDK.Target.Target);
    return target ? target.model(SDK.DebuggerModel.DebuggerModel) : null;
  }

  private longResume(): void {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.skipAllPausesUntilReloadOrTimeout(500);
      debuggerModel.resume();
    }
  }

  private terminateExecution(): void {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.runtimeModel().terminateExecution();
      debuggerModel.resume();
    }
  }

  stepOver(): boolean {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.stepOver();
    }
    return true;
  }

  stepInto(): boolean {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.stepInto();
    }
    return true;
  }

  stepIntoAsync(): boolean {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.scheduleStepIntoAsync();
    }
    return true;
  }

  stepOut(): boolean {
    const debuggerModel = this.prepareToResume();
    if (debuggerModel) {
      debuggerModel.stepOut();
    }
    return true;
  }

  private async continueToLocation(uiLocation: Workspace.UISourceCode.UILocation): Promise<void> {
    const executionContext = UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
    if (!executionContext) {
      return;
    }
    // Always use 0 column.
    const rawLocations =
        await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(
            uiLocation.uiSourceCode, uiLocation.lineNumber, 0);
    const rawLocation = rawLocations.find(location => location.debuggerModel === executionContext.debuggerModel);
    if (rawLocation && this.prepareToResume()) {
      rawLocation.continueToLocation();
    }
  }

  toggleBreakpointsActive(): void {
    Common.Settings.Settings.instance()
        .moduleSetting('breakpointsActive')
        .set(!Common.Settings.Settings.instance().moduleSetting('breakpointsActive').get());
  }

  private breakpointsActiveStateChanged(): void {
    const active = Common.Settings.Settings.instance().moduleSetting('breakpointsActive').get();
    this.toggleBreakpointsActiveAction.setToggled(!active);
    this.sourcesViewInternal.toggleBreakpointsActiveState(active);
  }

  private createDebugToolbar(): UI.Toolbar.Toolbar {
    const debugToolbar = new UI.Toolbar.Toolbar('scripts-debug-toolbar');

    const longResumeButton =
        new UI.Toolbar.ToolbarButton(i18nString(UIStrings.resumeWithAllPausesBlockedForMs), 'largeicon-play');
    longResumeButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this.longResume, this);
    const terminateExecutionButton = new UI.Toolbar.ToolbarButton(
        i18nString(UIStrings.terminateCurrentJavascriptCall), 'largeicon-terminate-execution');
    terminateExecutionButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this.terminateExecution, this);
    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createLongPressActionButton(
        this.togglePauseAction, [terminateExecutionButton, longResumeButton], []));

    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.stepOverAction));
    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.stepIntoAction));
    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.stepOutAction));
    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.stepAction));

    debugToolbar.appendSeparator();
    debugToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.toggleBreakpointsActiveAction));

    this.pauseOnExceptionButton = new UI.Toolbar.ToolbarToggle('', 'largeicon-pause-on-exceptions');
    this.pauseOnExceptionButton.addEventListener(
        UI.Toolbar.ToolbarButton.Events.Click, this.togglePauseOnExceptions, this);
    debugToolbar.appendToolbarItem(this.pauseOnExceptionButton);

    return debugToolbar;
  }

  private createDebugToolbarDrawer(): HTMLDivElement {
    const debugToolbarDrawer = document.createElement('div');
    debugToolbarDrawer.classList.add('scripts-debug-toolbar-drawer');

    const label = i18nString(UIStrings.pauseOnCaughtExceptions);
    const setting = Common.Settings.Settings.instance().moduleSetting('pauseOnCaughtException');
    debugToolbarDrawer.appendChild(UI.SettingsUI.createSettingCheckbox(label, setting, true));

    return debugToolbarDrawer;
  }

  appendApplicableItems(event: Event, contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    this.appendUISourceCodeItems(event, contextMenu, target);
    this.appendUISourceCodeFrameItems(event, contextMenu, target);
    this.appendUILocationItems(contextMenu, target);
    this.appendRemoteObjectItems(contextMenu, target);
    this.appendNetworkRequestItems(contextMenu, target);
  }

  private appendUISourceCodeItems(event: Event, contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    if (!(target instanceof Workspace.UISourceCode.UISourceCode) || !event.target) {
      return;
    }

    const uiSourceCode = (target as Workspace.UISourceCode.UISourceCode);
    const eventTarget = (event.target as Node);
    if (!uiSourceCode.project().isServiceProject() &&
        !eventTarget.isSelfOrDescendant(this.navigatorTabbedLocation.widget().element)) {
      contextMenu.revealSection().appendItem(
          i18nString(UIStrings.revealInSidebar), this.handleContextMenuReveal.bind(this, uiSourceCode));
    }
  }

  private appendUISourceCodeFrameItems(event: Event, contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    if (!(target instanceof UISourceCodeFrame)) {
      return;
    }
    if (target.uiSourceCode().contentType().isFromSourceMap() || target.textEditor.selection().isEmpty()) {
      return;
    }
    contextMenu.debugSection().appendAction('debugger.evaluate-selection');
  }

  appendUILocationItems(contextMenu: UI.ContextMenu.ContextMenu, object: Object): void {
    if (!(object instanceof Workspace.UISourceCode.UILocation)) {
      return;
    }
    const uiLocation = (object as Workspace.UISourceCode.UILocation);
    const uiSourceCode = uiLocation.uiSourceCode;

    if (!Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance()
             .scriptsForUISourceCode(uiSourceCode)
             .every(script => script.isJavaScript())) {
      // Ignore List and 'Continue to here' currently only works for JavaScript debugging.
      return;
    }
    const contentType = uiSourceCode.contentType();
    if (contentType.hasScripts()) {
      const target = UI.Context.Context.instance().flavor(SDK.Target.Target);
      const debuggerModel = target ? target.model(SDK.DebuggerModel.DebuggerModel) : null;
      if (debuggerModel && debuggerModel.isPaused()) {
        contextMenu.debugSection().appendItem(
            i18nString(UIStrings.continueToHere), this.continueToLocation.bind(this, uiLocation));
      }

      this.callstackPane.appendIgnoreListURLContextMenuItems(contextMenu, uiSourceCode);
    }
  }

  private handleContextMenuReveal(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    this.editorView.showBoth();
    this.revealInNavigator(uiSourceCode);
  }

  private appendRemoteObjectItems(contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    if (!(target instanceof SDK.RemoteObject.RemoteObject)) {
      return;
    }
    const indent = Common.Settings.Settings.instance().moduleSetting('textEditorIndent').get();
    const remoteObject = (target as SDK.RemoteObject.RemoteObject);
    const executionContext = UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);

    function getObjectTitle(): string|undefined {
      if (remoteObject.type === 'wasm') {
        return remoteObject.subtype;
      }
      if (remoteObject.subtype === 'node') {
        return 'outerHTML';
      }
      return remoteObject.type;
    }
    const copyContextMenuTitle = getObjectTitle();

    contextMenu.debugSection().appendItem(
        i18nString(UIStrings.storeSAsGlobalVariable, {PH1: String(copyContextMenuTitle)}),
        () => SDK.ConsoleModel.ConsoleModel.instance().saveToTempVariable(executionContext, remoteObject));

    const ctxMenuClipboardSection = contextMenu.clipboardSection();
    const inspectorFrontendHost = Host.InspectorFrontendHost.InspectorFrontendHostInstance;

    if (remoteObject.type === 'string') {
      ctxMenuClipboardSection.appendItem(i18nString(UIStrings.copyStringContents), () => {
        inspectorFrontendHost.copyText(remoteObject.value);
      });
      ctxMenuClipboardSection.appendItem(i18nString(UIStrings.copyStringAsJSLiteral), () => {
        inspectorFrontendHost.copyText(Platform.StringUtilities.formatAsJSLiteral(remoteObject.value));
      });
      ctxMenuClipboardSection.appendItem(i18nString(UIStrings.copyStringAsJSONLiteral), () => {
        inspectorFrontendHost.copyText(JSON.stringify(remoteObject.value));
      });
    }
    // We are trying to copy a primitive value.
    else if (primitiveRemoteObjectTypes.has(remoteObject.type)) {
      ctxMenuClipboardSection.appendItem(i18nString(UIStrings.copyS, {PH1: String(copyContextMenuTitle)}), () => {
        inspectorFrontendHost.copyText(remoteObject.description);
      });
    }
    // We are trying to copy a remote object.
    else if (remoteObject.type === 'object') {
      const copyDecodedValueHandler = async(): Promise<void> => {
        const result = await remoteObject.callFunctionJSON(toStringForClipboard, [{
                                                             value: {
                                                               subtype: remoteObject.subtype,
                                                               indent: indent,
                                                             },
                                                           }]);
        inspectorFrontendHost.copyText(result);
      };

      ctxMenuClipboardSection.appendItem(
          i18nString(UIStrings.copyS, {PH1: String(copyContextMenuTitle)}), copyDecodedValueHandler);
    }

    else if (remoteObject.type === 'function') {
      contextMenu.debugSection().appendItem(
          i18nString(UIStrings.showFunctionDefinition), this.showFunctionDefinition.bind(this, remoteObject));
    }

    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    function toStringForClipboard(this: Object, data: any): string|undefined {
      const subtype = data.subtype;
      const indent = data.indent;

      if (subtype === 'node') {
        return this instanceof Element ? this.outerHTML : undefined;
      }
      if (subtype && typeof this === 'undefined') {
        return String(subtype);
      }
      try {
        return JSON.stringify(this, null, indent);
      } catch (error) {
        return String(this);
      }
    }
  }

  private appendNetworkRequestItems(contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    if (!(target instanceof SDK.NetworkRequest.NetworkRequest)) {
      return;
    }
    const request = (target as SDK.NetworkRequest.NetworkRequest);
    const uiSourceCode = this.workspace.uiSourceCodeForURL(request.url());
    if (!uiSourceCode) {
      return;
    }
    const openText = i18nString(UIStrings.openInSourcesPanel);
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const callback: () => any = this.showUILocation.bind(this, uiSourceCode.uiLocation(0, 0));
    contextMenu.revealSection().appendItem(openText, callback);
  }

  private showFunctionDefinition(remoteObject: SDK.RemoteObject.RemoteObject): void {
    remoteObject.debuggerModel().functionDetailsPromise(remoteObject).then(this.didGetFunctionDetails.bind(this));
  }

  private async didGetFunctionDetails(response: {
    location: SDK.DebuggerModel.Location|null,
  }|null): Promise<void> {
    if (!response || !response.location) {
      return;
    }

    const uiLocation =
        await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().rawLocationToUILocation(
            response.location);
    if (uiLocation) {
      this.showUILocation(uiLocation);
    }
  }

  private revealNavigatorSidebar(): void {
    this.setAsCurrentPanel();
    this.editorView.showBoth(true);
  }

  private revealDebuggerSidebar(): void {
    this.setAsCurrentPanel();
    this.splitWidget.showBoth(true);
  }

  private updateSidebarPosition(): void {
    let vertically;
    const position = Common.Settings.Settings.instance().moduleSetting('sidebarPosition').get();
    if (position === 'right') {
      vertically = false;
    } else if (position === 'bottom') {
      vertically = true;
    } else {
      vertically = UI.InspectorView.InspectorView.instance().element.offsetWidth < 680;
    }

    if (this.sidebarPaneView && vertically === !this.splitWidget.isVertical()) {
      return;
    }

    if (this.sidebarPaneView && this.sidebarPaneView.shouldHideOnDetach()) {
      return;
    }  // We can't reparent extension iframes.

    if (this.sidebarPaneView) {
      this.sidebarPaneView.detach();
    }

    this.splitWidget.setVertical(!vertically);
    this.splitWidget.element.classList.toggle('sources-split-view-vertical', vertically);

    SourcesPanel.updateResizerAndSidebarButtons(this);

    // Create vertical box with stack.
    const vbox = new UI.Widget.VBox();
    vbox.element.appendChild(this.debugToolbar.element);
    vbox.element.appendChild(this.debugToolbarDrawer);

    vbox.setMinimumAndPreferredSizes(minToolbarWidth, 25, minToolbarWidth, 100);
    this.sidebarPaneStack =
        UI.ViewManager.ViewManager.instance().createStackLocation(this.revealDebuggerSidebar.bind(this));
    this.sidebarPaneStack.widget().element.classList.add('overflow-auto');
    this.sidebarPaneStack.widget().show(vbox.element);
    this.sidebarPaneStack.widget().element.appendChild(this.debuggerPausedMessage.element());
    this.sidebarPaneStack.appendApplicableItems('sources.sidebar-top');

    if (this.threadsSidebarPane) {
      this.sidebarPaneStack.showView(this.threadsSidebarPane);
    }

    const jsBreakpoints = (UI.ViewManager.ViewManager.instance().view('sources.jsBreakpoints') as UI.View.View);
    const scopeChainView = (UI.ViewManager.ViewManager.instance().view('sources.scopeChain') as UI.View.View);

    if (this.tabbedLocationHeader) {
      this.splitWidget.uninstallResizer(this.tabbedLocationHeader);
      this.tabbedLocationHeader = null;
    }

    if (!vertically) {
      // Populate the rest of the stack.
      this.sidebarPaneStack.appendView(this.watchSidebarPane);
      this.sidebarPaneStack.showView(jsBreakpoints);
      this.sidebarPaneStack.showView(scopeChainView);
      this.sidebarPaneStack.showView(this.callstackPane);
      this.extensionSidebarPanesContainer = this.sidebarPaneStack;
      this.sidebarPaneView = vbox;
      this.splitWidget.uninstallResizer(this.debugToolbar.gripElementForResize());
    } else {
      const splitWidget = new UI.SplitWidget.SplitWidget(true, true, 'sourcesPanelDebuggerSidebarSplitViewState', 0.5);
      splitWidget.setMainWidget(vbox);

      // Populate the left stack.
      this.sidebarPaneStack.showView(jsBreakpoints);
      this.sidebarPaneStack.showView(this.callstackPane);

      const tabbedLocation =
          UI.ViewManager.ViewManager.instance().createTabbedLocation(this.revealDebuggerSidebar.bind(this));
      splitWidget.setSidebarWidget(tabbedLocation.tabbedPane());
      this.tabbedLocationHeader = tabbedLocation.tabbedPane().headerElement();
      this.splitWidget.installResizer(this.tabbedLocationHeader);
      this.splitWidget.installResizer(this.debugToolbar.gripElementForResize());
      tabbedLocation.appendView(scopeChainView);
      tabbedLocation.appendView(this.watchSidebarPane);
      tabbedLocation.appendApplicableItems('sources.sidebar-tabs');
      this.extensionSidebarPanesContainer = tabbedLocation;
      this.sidebarPaneView = splitWidget;
    }

    this.sidebarPaneStack.appendApplicableItems('sources.sidebar-bottom');
    const extensionSidebarPanes = Extensions.ExtensionServer.ExtensionServer.instance().sidebarPanes();
    for (let i = 0; i < extensionSidebarPanes.length; ++i) {
      this.addExtensionSidebarPane(extensionSidebarPanes[i]);
    }

    this.splitWidget.setSidebarWidget(this.sidebarPaneView);
  }

  setAsCurrentPanel(): Promise<void> {
    if (Common.Settings.Settings.instance().moduleSetting('autoFocusOnDebuggerPausedEnabled').get()) {
      return UI.ViewManager.ViewManager.instance().showView('sources');
    }
    return Promise.resolve();
  }

  private extensionSidebarPaneAdded(
      event: Common.EventTarget.EventTargetEvent<Extensions.ExtensionPanel.ExtensionSidebarPane>): void {
    this.addExtensionSidebarPane(event.data);
  }

  private addExtensionSidebarPane(pane: Extensions.ExtensionPanel.ExtensionSidebarPane): void {
    if (pane.panelName() === this.name) {
      (this.extensionSidebarPanesContainer as UI.View.ViewLocation).appendView(pane);
    }
  }

  sourcesView(): SourcesView {
    return this.sourcesViewInternal;
  }

  private handleDrop(dataTransfer: DataTransfer): void {
    const items = dataTransfer.items;
    if (!items.length) {
      return;
    }
    const entry = items[0].webkitGetAsEntry();
    if (entry && entry.isDirectory) {
      Host.InspectorFrontendHost.InspectorFrontendHostInstance.upgradeDraggedFileSystemPermissions(entry.filesystem);
    }
  }
}

export let lastModificationTimeout = 200;
export const minToolbarWidth = 215;

let uILocationRevealerInstance: UILocationRevealer;

export class UILocationRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): UILocationRevealer {
    const {forceNew} = opts;
    if (!uILocationRevealerInstance || forceNew) {
      uILocationRevealerInstance = new UILocationRevealer();
    }

    return uILocationRevealerInstance;
  }

  reveal(uiLocation: Object, omitFocus?: boolean): Promise<void> {
    if (!(uiLocation instanceof Workspace.UISourceCode.UILocation)) {
      return Promise.reject(new Error('Internal error: not a ui location'));
    }
    SourcesPanel.instance().showUILocation(uiLocation, omitFocus);
    return Promise.resolve();
  }
}

let debuggerLocationRevealerInstance: DebuggerLocationRevealer;

export class DebuggerLocationRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): DebuggerLocationRevealer {
    const {forceNew} = opts;
    if (!debuggerLocationRevealerInstance || forceNew) {
      debuggerLocationRevealerInstance = new DebuggerLocationRevealer();
    }

    return debuggerLocationRevealerInstance;
  }

  async reveal(rawLocation: Object, omitFocus?: boolean): Promise<void> {
    if (!(rawLocation instanceof SDK.DebuggerModel.Location)) {
      throw new Error('Internal error: not a debugger location');
    }
    const uiLocation =
        await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().rawLocationToUILocation(
            rawLocation);
    if (uiLocation) {
      SourcesPanel.instance().showUILocation(uiLocation, omitFocus);
    }
  }
}

let uISourceCodeRevealerInstance: UISourceCodeRevealer;

export class UISourceCodeRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): UISourceCodeRevealer {
    const {forceNew} = opts;
    if (!uISourceCodeRevealerInstance || forceNew) {
      uISourceCodeRevealerInstance = new UISourceCodeRevealer();
    }

    return uISourceCodeRevealerInstance;
  }

  reveal(uiSourceCode: Object, omitFocus?: boolean): Promise<void> {
    if (!(uiSourceCode instanceof Workspace.UISourceCode.UISourceCode)) {
      return Promise.reject(new Error('Internal error: not a ui source code'));
    }
    SourcesPanel.instance().showUISourceCode(uiSourceCode, undefined, undefined, omitFocus);
    return Promise.resolve();
  }
}

let debuggerPausedDetailsRevealerInstance: DebuggerPausedDetailsRevealer;

export class DebuggerPausedDetailsRevealer implements Common.Revealer.Revealer {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): DebuggerPausedDetailsRevealer {
    const {forceNew} = opts;
    if (!debuggerPausedDetailsRevealerInstance || forceNew) {
      debuggerPausedDetailsRevealerInstance = new DebuggerPausedDetailsRevealer();
    }

    return debuggerPausedDetailsRevealerInstance;
  }

  reveal(_object: Object): Promise<void> {
    return SourcesPanel.instance().setAsCurrentPanel();
  }
}

let revealingActionDelegateInstance: RevealingActionDelegate;

export class RevealingActionDelegate implements UI.ActionRegistration.ActionDelegate {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): RevealingActionDelegate {
    const {forceNew} = opts;
    if (!revealingActionDelegateInstance || forceNew) {
      revealingActionDelegateInstance = new RevealingActionDelegate();
    }

    return revealingActionDelegateInstance;
  }
  handleAction(context: UI.Context.Context, actionId: string): boolean {
    const panel = SourcesPanel.instance();
    if (!panel.ensureSourcesViewVisible()) {
      return false;
    }
    switch (actionId) {
      case 'debugger.toggle-pause':
        panel.togglePause();
        return true;
    }
    return false;
  }
}

let debuggingActionDelegateInstance: DebuggingActionDelegate;

export class DebuggingActionDelegate implements UI.ActionRegistration.ActionDelegate {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): DebuggingActionDelegate {
    const {forceNew} = opts;
    if (!debuggingActionDelegateInstance || forceNew) {
      debuggingActionDelegateInstance = new DebuggingActionDelegate();
    }

    return debuggingActionDelegateInstance;
  }
  handleAction(context: UI.Context.Context, actionId: string): boolean {
    const panel = SourcesPanel.instance();
    switch (actionId) {
      case 'debugger.step-over': {
        panel.stepOver();
        return true;
      }
      case 'debugger.step-into': {
        panel.stepIntoAsync();
        return true;
      }
      case 'debugger.step': {
        panel.stepInto();
        return true;
      }
      case 'debugger.step-out': {
        panel.stepOut();
        return true;
      }
      case 'debugger.run-snippet': {
        panel.runSnippet();
        return true;
      }
      case 'debugger.toggle-breakpoints-active': {
        panel.toggleBreakpointsActive();
        return true;
      }
      case 'debugger.evaluate-selection': {
        const frame = UI.Context.Context.instance().flavor(UISourceCodeFrame);
        if (frame) {
          let text = frame.textEditor.text(frame.textEditor.selection());
          const executionContext = UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
          if (executionContext) {
            const message = SDK.ConsoleModel.ConsoleModel.instance().addCommandMessage(executionContext, text);
            text = ObjectUI.JavaScriptREPL.JavaScriptREPL.wrapObjectLiteral(text);
            SDK.ConsoleModel.ConsoleModel.instance().evaluateCommandInConsole(
                executionContext, message, text, /* useCommandLineAPI */ true);
          }
        }
        return true;
      }
    }
    return false;
  }
}

export class WrapperView extends UI.Widget.VBox {
  private readonly view: SourcesView;
  constructor() {
    super();
    this.element.classList.add('sources-view-wrapper');
    this.view = SourcesPanel.instance().sourcesView();
  }

  static instance(): WrapperView {
    if (!wrapperViewInstance) {
      wrapperViewInstance = new WrapperView();
    }

    return wrapperViewInstance;
  }

  static isShowing(): boolean {
    return Boolean(wrapperViewInstance) && wrapperViewInstance.isShowing();
  }

  wasShown(): void {
    if (!SourcesPanel.instance().isShowing()) {
      this.showViewInWrapper();
    } else {
      UI.InspectorView.InspectorView.instance().setDrawerMinimized(true);
    }
    SourcesPanel.updateResizerAndSidebarButtons(SourcesPanel.instance());
  }

  willHide(): void {
    UI.InspectorView.InspectorView.instance().setDrawerMinimized(false);
    queueMicrotask(() => {
      SourcesPanel.updateResizerAndSidebarButtons(SourcesPanel.instance());
    });
  }

  showViewInWrapper(): void {
    this.view.show(this.element);
  }
}

const registeredNavigatorViews: NavigatorViewRegistration[] = [
  {
    viewId: 'navigator-network',
    navigatorView: NetworkNavigatorView.instance,
    experiment: undefined,
  },
  {
    viewId: 'navigator-files',
    navigatorView: FilesNavigatorView.instance,
    experiment: undefined,
  },
  {
    viewId: 'navigator-snippets',
    navigatorView: SnippetsNavigatorView.instance,
    experiment: undefined,
  },
  {
    viewId: 'navigator-overrides',
    navigatorView: OverridesNavigatorView.instance,
    experiment: undefined,
  },
  {
    viewId: 'navigator-contentScripts',
    navigatorView: ContentScriptsNavigatorView.instance,
    experiment: undefined,
  },
];
export interface NavigatorViewRegistration {
  navigatorView: () => NavigatorView;
  viewId: string;
  experiment?: string;
}
