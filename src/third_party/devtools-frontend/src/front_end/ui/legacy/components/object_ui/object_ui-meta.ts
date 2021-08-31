// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Root from '../../../../core/root/root.js';
import * as SDK from '../../../../core/sdk/sdk.js';
import * as UI from '../../legacy.js';

// eslint-disable-next-line rulesdir/es_modules_import
import type * as ObjectUI from './object_ui.js';

let loadedObjectUIModule: (typeof ObjectUI|undefined);

async function loadObjectUIModule(): Promise<typeof ObjectUI> {
  if (!loadedObjectUIModule) {
    // Side-effect import resources in module.json
    await Root.Runtime.Runtime.instance().loadModulePromise('object_ui');
    loadedObjectUIModule = await import('./object_ui.js');
  }
  return loadedObjectUIModule;
}

UI.UIUtils.registerRenderer({
  contextTypes() {
    return [SDK.RemoteObject.RemoteObject];
  },
  async loadRenderer() {
    const ObjectUI = await loadObjectUIModule();
    return ObjectUI.ObjectPropertiesSection.Renderer.instance();
  },
});
