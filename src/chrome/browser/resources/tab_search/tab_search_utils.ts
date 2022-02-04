// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {highlight} from 'chrome://resources/js/search_highlight_utils.js';
import {Tab} from './tab_search.mojom-webui.js';

type Range = {
  start: number,
  length: number
};

export function highlightText(
    container: HTMLElement, text: string, ranges: Range[]|undefined) {
  container.textContent = '';
  const node = document.createTextNode(text);
  container.appendChild(node);
  if (ranges) {
    highlight(node, ranges);
  }
}

export function tabHasMediaAlerts(tab: Tab): boolean {
  return tab.alertStates.length > 0;
}
