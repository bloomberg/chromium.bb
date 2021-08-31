// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable rulesdir/no_underscored_properties */

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as UI from '../../ui/legacy/legacy.js';

import {ScreencastView} from './ScreencastView.js';

const UIStrings = {
  /**
  *@description Tooltip text that appears when hovering over largeicon phone button in Screencast App of the Remote Devices tab when toggling screencast
  */
  toggleScreencast: 'Toggle screencast',
};
const str_ = i18n.i18n.registerUIStrings('panels/screencast/ScreencastApp.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
let appInstance: ScreencastApp;

export class ScreencastApp implements Common.App.App,
                                      SDK.SDKModel.SDKModelObserver<SDK.ScreenCaptureModel.ScreenCaptureModel> {
  _enabledSetting: Common.Settings.Setting<boolean>;
  _toggleButton: UI.Toolbar.ToolbarToggle;
  _rootSplitWidget?: UI.SplitWidget.SplitWidget;
  _screenCaptureModel?: SDK.ScreenCaptureModel.ScreenCaptureModel;
  _screencastView?: ScreencastView;
  constructor() {
    this._enabledSetting = Common.Settings.Settings.instance().createSetting('screencastEnabled', true);
    this._toggleButton = new UI.Toolbar.ToolbarToggle(i18nString(UIStrings.toggleScreencast), 'largeicon-phone');
    this._toggleButton.setToggled(this._enabledSetting.get());
    this._toggleButton.setEnabled(false);
    this._toggleButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this._toggleButtonClicked, this);
    SDK.SDKModel.TargetManager.instance().observeModels(SDK.ScreenCaptureModel.ScreenCaptureModel, this);
  }

  static _instance(): ScreencastApp {
    if (!appInstance) {
      appInstance = new ScreencastApp();
    }
    return appInstance;
  }

  presentUI(document: Document): void {
    const rootView = new UI.RootView.RootView();

    this._rootSplitWidget =
        new UI.SplitWidget.SplitWidget(false, true, 'InspectorView.screencastSplitViewState', 300, 300);
    this._rootSplitWidget.setVertical(true);
    this._rootSplitWidget.setSecondIsSidebar(true);
    this._rootSplitWidget.show(rootView.element);
    this._rootSplitWidget.hideMain();

    this._rootSplitWidget.setSidebarWidget(UI.InspectorView.InspectorView.instance());
    UI.InspectorView.InspectorView.instance().setOwnerSplit(this._rootSplitWidget);
    rootView.attachToDocument(document);
    rootView.focus();
  }

  modelAdded(screenCaptureModel: SDK.ScreenCaptureModel.ScreenCaptureModel): void {
    if (this._screenCaptureModel) {
      return;
    }
    this._screenCaptureModel = screenCaptureModel;
    this._toggleButton.setEnabled(true);
    this._screencastView = new ScreencastView(screenCaptureModel);
    if (this._rootSplitWidget) {
      this._rootSplitWidget.setMainWidget(this._screencastView);
    }
    this._screencastView.initialize();
    this._onScreencastEnabledChanged();
  }

  modelRemoved(screenCaptureModel: SDK.ScreenCaptureModel.ScreenCaptureModel): void {
    if (this._screenCaptureModel !== screenCaptureModel) {
      return;
    }
    delete this._screenCaptureModel;
    this._toggleButton.setEnabled(false);
    if (this._screencastView) {
      this._screencastView.detach();
      delete this._screencastView;
    }
    this._onScreencastEnabledChanged();
  }

  _toggleButtonClicked(): void {
    const enabled = !this._toggleButton.toggled();
    this._enabledSetting.set(enabled);
    this._onScreencastEnabledChanged();
  }

  _onScreencastEnabledChanged(): void {
    if (!this._rootSplitWidget) {
      return;
    }
    const enabled = Boolean(this._enabledSetting.get() && this._screencastView);
    this._toggleButton.setToggled(enabled);
    if (enabled) {
      this._rootSplitWidget.showBoth();
    } else {
      this._rootSplitWidget.hideMain();
    }
  }
}

let toolbarButtonProviderInstance: ToolbarButtonProvider;

export class ToolbarButtonProvider implements UI.Toolbar.Provider {
  static instance(opts: {forceNew: boolean} = {forceNew: false}): ToolbarButtonProvider {
    const {forceNew} = opts;
    if (!toolbarButtonProviderInstance || forceNew) {
      toolbarButtonProviderInstance = new ToolbarButtonProvider();
    }

    return toolbarButtonProviderInstance;
  }

  item(): UI.Toolbar.ToolbarItem|null {
    return ScreencastApp._instance()._toggleButton;
  }
}

let screencastAppProviderInstance: ScreencastAppProvider;

export class ScreencastAppProvider implements Common.AppProvider.AppProvider {
  static instance(opts: {forceNew: boolean} = {forceNew: false}): ScreencastAppProvider {
    const {forceNew} = opts;
    if (!screencastAppProviderInstance || forceNew) {
      screencastAppProviderInstance = new ScreencastAppProvider();
    }

    return screencastAppProviderInstance;
  }

  createApp(): Common.App.App {
    return ScreencastApp._instance();
  }
}
