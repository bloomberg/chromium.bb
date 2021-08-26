// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as UI from '../../ui/legacy/legacy.js';

import {DeviceModeWrapper} from './DeviceModeWrapper.js';
import {Events, InspectedPagePlaceholder} from './InspectedPagePlaceholder.js';

let appInstance: AdvancedApp;

interface Event {
  data: {
    to: string,
    from: string,
    x: number,
    y: number,
    width: number,
    height: number,
  };
}

export class AdvancedApp implements Common.App.App {
  private rootSplitWidget!: UI.SplitWidget.SplitWidget;
  private deviceModeView!: DeviceModeWrapper;
  private inspectedPagePlaceholder!: InspectedPagePlaceholder;
  private toolboxWindow?: Window|null;
  private toolboxRootView?: UI.RootView.RootView;
  private changingDockSide?: boolean;

  constructor() {
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.BeforeDockSideChanged, this.openToolboxWindow, this);
  }

  /**
   * Note: it's used by toolbox.ts without real type checks.
   */
  static instance(): AdvancedApp {
    if (!appInstance) {
      appInstance = new AdvancedApp();
    }
    return appInstance;
  }

  presentUI(document: Document): void {
    const rootView = new UI.RootView.RootView();

    this.rootSplitWidget = new UI.SplitWidget.SplitWidget(false, true, 'InspectorView.splitViewState', 555, 300, true);
    this.rootSplitWidget.show(rootView.element);
    this.rootSplitWidget.setSidebarWidget(UI.InspectorView.InspectorView.instance());
    this.rootSplitWidget.setDefaultFocusedChild(UI.InspectorView.InspectorView.instance());
    UI.InspectorView.InspectorView.instance().setOwnerSplit(this.rootSplitWidget);

    this.inspectedPagePlaceholder = InspectedPagePlaceholder.instance();
    this.inspectedPagePlaceholder.addEventListener(Events.Update, this.onSetInspectedPageBounds.bind(this), this);
    this.deviceModeView =
        DeviceModeWrapper.instance({inspectedPagePlaceholder: this.inspectedPagePlaceholder, forceNew: false});

    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.BeforeDockSideChanged, this.onBeforeDockSideChange, this);
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.DockSideChanged, this.onDockSideChange, this);
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.AfterDockSideChanged, this.onAfterDockSideChange, this);
    this.onDockSideChange();

    console.timeStamp('AdvancedApp.attachToBody');
    rootView.attachToDocument(document);
    rootView.focus();
    this.inspectedPagePlaceholder.update();
  }

  private openToolboxWindow(event: Event): void {
    if ((event.data.to as string) !== UI.DockController.State.Undocked) {
      return;
    }

    if (this.toolboxWindow) {
      return;
    }

    const url = window.location.href.replace('devtools_app.html', 'device_mode_emulation_frame.html');
    this.toolboxWindow = window.open(url, undefined);
  }

  deviceModeEmulationFrameLoaded(toolboxDocument: Document): void {
    UI.UIUtils.initializeUIUtils(
        toolboxDocument, Common.Settings.Settings.instance().createSetting('uiTheme', 'default'));
    UI.UIUtils.installComponentRootStyles((toolboxDocument.body as Element));
    UI.ContextMenu.ContextMenu.installHandler(toolboxDocument);

    this.toolboxRootView = new UI.RootView.RootView();
    this.toolboxRootView.attachToDocument(toolboxDocument);

    this.updateDeviceModeView();
  }

  private updateDeviceModeView(): void {
    if (this.isDocked()) {
      this.rootSplitWidget.setMainWidget(this.deviceModeView);
    } else if (this.toolboxRootView) {
      this.deviceModeView.show(this.toolboxRootView.element);
    }
  }

  private onBeforeDockSideChange(event: Event): void {
    if (event.data.to === UI.DockController.State.Undocked && this.toolboxRootView) {
      // Hide inspectorView and force layout to mimic the undocked state.
      this.rootSplitWidget.hideSidebar();
      this.inspectedPagePlaceholder.update();
    }

    this.changingDockSide = true;
  }

  private onDockSideChange(event?: Event): void {
    this.updateDeviceModeView();

    const toDockSide = event ? event.data.to : UI.DockController.DockController.instance().dockSide();
    if (toDockSide === UI.DockController.State.Undocked) {
      this.updateForUndocked();
    } else if (this.toolboxRootView && event && event.data.from === UI.DockController.State.Undocked) {
      // Don't update yet for smooth transition.
      this.rootSplitWidget.hideSidebar();
    } else {
      this.updateForDocked(toDockSide);
    }
  }

  private onAfterDockSideChange(event: Event): void {
    // We may get here on the first dock side change while loading without BeforeDockSideChange.
    if (!this.changingDockSide) {
      return;
    }
    if ((event.data.from as string) === UI.DockController.State.Undocked) {
      this.updateForDocked((event.data.to as string));
    }
    this.changingDockSide = false;
    this.inspectedPagePlaceholder.update();
  }

  private updateForDocked(dockSide: string): void {
    const resizerElement = (this.rootSplitWidget.resizerElement() as HTMLElement);
    resizerElement.style.transform = dockSide === UI.DockController.State.DockedToRight ?
        'translateX(2px)' :
        dockSide === UI.DockController.State.DockedToLeft ? 'translateX(-2px)' : '';
    this.rootSplitWidget.setVertical(
        dockSide === UI.DockController.State.DockedToRight || dockSide === UI.DockController.State.DockedToLeft);
    this.rootSplitWidget.setSecondIsSidebar(
        dockSide === UI.DockController.State.DockedToRight || dockSide === UI.DockController.State.DockedToBottom);
    this.rootSplitWidget.toggleResizer(this.rootSplitWidget.resizerElement(), true);
    this.rootSplitWidget.toggleResizer(
        UI.InspectorView.InspectorView.instance().topResizerElement(),
        dockSide === UI.DockController.State.DockedToBottom);
    this.rootSplitWidget.showBoth();
  }

  private updateForUndocked(): void {
    this.rootSplitWidget.toggleResizer(this.rootSplitWidget.resizerElement(), false);
    this.rootSplitWidget.toggleResizer(UI.InspectorView.InspectorView.instance().topResizerElement(), false);
    this.rootSplitWidget.hideMain();
  }

  private isDocked(): boolean {
    return UI.DockController.DockController.instance().dockSide() !== UI.DockController.State.Undocked;
  }

  private onSetInspectedPageBounds(event: Event): void {
    if (this.changingDockSide) {
      return;
    }
    const window = this.inspectedPagePlaceholder.element.window();
    if (!window.innerWidth || !window.innerHeight) {
      return;
    }
    if (!this.inspectedPagePlaceholder.isShowing()) {
      return;
    }
    const bounds = event.data;
    console.timeStamp('AdvancedApp.setInspectedPageBounds');
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.setInspectedPageBounds(bounds);
  }
}

// Export required for usage in toolbox.ts
// @ts-ignore Exported for Tests.js
globalThis.Emulation = globalThis.Emulation || {};
// @ts-ignore Exported for Tests.js
globalThis.Emulation.AdvancedApp = AdvancedApp;

let advancedAppProviderInstance: AdvancedAppProvider;

export class AdvancedAppProvider implements Common.AppProvider.AppProvider {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): AdvancedAppProvider {
    const {forceNew} = opts;
    if (!advancedAppProviderInstance || forceNew) {
      advancedAppProviderInstance = new AdvancedAppProvider();
    }

    return advancedAppProviderInstance;
  }

  createApp(): Common.App.App {
    return AdvancedApp.instance();
  }
}
