// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../core/i18n/i18n.js';
import * as Root from '../core/root/root.js';
import * as UI from '../ui/legacy/legacy.js';

// eslint-disable-next-line rulesdir/es_modules_import
import type * as Profiler from '../panels/profiler/profiler.js';

const UIStrings = {
  /**
  *@description Title of the Profiler tool
  */
  profiler: 'Profiler',
  /**
  *@description Command for showing the Profiler tool
  */
  showProfiler: 'Show Profiler',
};
const str_ = i18n.i18n.registerUIStrings('js_profiler/js_profiler-meta.ts', UIStrings);
const i18nLazyString = i18n.i18n.getLazilyComputedLocalizedString.bind(undefined, str_);

let loadedProfilerModule: (typeof Profiler|undefined);

async function loadProfilerModule(): Promise<typeof Profiler> {
  if (!loadedProfilerModule) {
    // Side-effect import resources in module.json
    await Root.Runtime.Runtime.instance().loadModulePromise('profiler');
    loadedProfilerModule = await import('../panels/profiler/profiler.js');
  }
  return loadedProfilerModule;
}

UI.ViewManager.registerViewExtension({
  location: UI.ViewManager.ViewLocationValues.PANEL,
  id: 'js_profiler',
  title: i18nLazyString(UIStrings.profiler),
  commandPrompt: i18nLazyString(UIStrings.showProfiler),
  order: 65,
  async loadView() {
    const Profiler = await loadProfilerModule();
    return Profiler.ProfilesPanel.JSProfilerPanel.instance();
  },
});
