// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Root from '../../core/root/root.js';
import * as UI from '../../ui/legacy/legacy.js';

// eslint-disable-next-line rulesdir/es_modules_import
import type * as ConsoleCounters from './console_counters.js';

let loadedConsoleCountersModule: (typeof ConsoleCounters|undefined);

async function loadConsoleCountersModule(): Promise<typeof ConsoleCounters> {
  if (!loadedConsoleCountersModule) {
    // Side-effect import reconsole_counters in module.json
    await Root.Runtime.Runtime.instance().loadModulePromise('panels/console_counters');
    loadedConsoleCountersModule = await import('./console_counters.js');
  }
  return loadedConsoleCountersModule;
}

UI.Toolbar.registerToolbarItem({
  async loadItem() {
    const ConsoleCounters = await loadConsoleCountersModule();
    return ConsoleCounters.WarningErrorCounter.WarningErrorCounter.instance();
  },
  order: 1,
  location: UI.Toolbar.ToolbarItemLocation.MAIN_TOOLBAR_RIGHT,
  showLabel: undefined,
  condition: undefined,
  separator: undefined,
  actionId: undefined,
});
