// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as FrontendHelpers from '../../../test/unittests/front_end/helpers/EnvironmentHelpers.js';
import * as ComponentHelpers from '../../component_helpers/component_helpers.js';
import * as Components from '../../ui/components/components.js';

import {belgraveHouse, officesAndProductsData} from './sample-data.js';

await ComponentHelpers.ComponentServerSetup.setup();
await FrontendHelpers.initializeGlobalVars();

const component = new Components.TreeOutline.TreeOutline<string>();
component.data = {
  defaultRenderer: Components.TreeOutline.defaultRenderer,
  tree: officesAndProductsData,
};
component.setAttribute('animated', 'animated');

component.addEventListener('treenodemouseover', (event: Event) => {
  const evt = event as Components.TreeOutline.ItemMouseOverEvent<string>;
  // eslint-disable-next-line no-console
  console.log('Node', evt.data.node, 'mouseover');
});
component.addEventListener('treenodemouseout', (event: Event) => {
  const evt = event as Components.TreeOutline.ItemMouseOutEvent<string>;
  // eslint-disable-next-line no-console
  console.log('Node', evt.data.node, 'mouseout');
});

document.getElementById('container')?.appendChild(component);
document.getElementById('recursively-expand')?.addEventListener('click', () => {
  component.expandRecursively();
});
document.getElementById('expand-to-belgrave-house')?.addEventListener('click', () => {
  component.expandToAndSelectTreeNode(belgraveHouse);
});
