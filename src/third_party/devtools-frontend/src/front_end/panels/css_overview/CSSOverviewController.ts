// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable rulesdir/no_underscored_properties */

import * as Common from '../../core/common/common.js';
import * as SDK from '../../core/sdk/sdk.js';

export class OverviewController extends Common.ObjectWrapper.ObjectWrapper {
  currentUrl: string;
  constructor() {
    super();

    this.currentUrl = SDK.SDKModel.TargetManager.instance().inspectedURL();
    SDK.SDKModel.TargetManager.instance().addEventListener(
        SDK.SDKModel.Events.InspectedURLChanged, this._checkUrlAndResetIfChanged, this);
  }

  _checkUrlAndResetIfChanged(): void {
    if (this.currentUrl === SDK.SDKModel.TargetManager.instance().inspectedURL()) {
      return;
    }

    this.currentUrl = SDK.SDKModel.TargetManager.instance().inspectedURL();
    this.dispatchEventToListeners(Events.Reset);
  }
}

export const Events = {
  RequestOverviewStart: Symbol('RequestOverviewStart'),
  RequestNodeHighlight: Symbol('RequestNodeHighlight'),
  PopulateNodes: Symbol('PopulateNodes'),
  RequestOverviewCancel: Symbol('RequestOverviewCancel'),
  OverviewCompleted: Symbol('OverviewCompleted'),
  Reset: Symbol('Reset'),
};
