// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Coordinator from '../../../../front_end/render_coordinator/render_coordinator.js';
import * as Resources from '../../../../front_end/resources/resources.js';
import * as Components from '../../../../front_end/ui/components/components.js';
import {assertElement, assertShadowRoot, getElementWithinComponent, renderElementIntoDOM} from '../helpers/DOMHelpers.js';
import {getValuesOfAllBodyRows} from '../ui/components/DataGridHelpers.js';

const coordinator = Coordinator.RenderCoordinator.RenderCoordinator.instance();

const {assert} = chai;

async function renderTrustTokensView(tokens: Protocol.Storage.TrustTokens[]):
    Promise<Resources.TrustTokensView.TrustTokensView> {
  const component = new Resources.TrustTokensView.TrustTokensView();
  renderElementIntoDOM(component);
  component.data = {tokens};

  // The data-grid's renderer is scheduled, so we need to wait until the coordinator
  // is done before we can test against it.
  await coordinator.done();

  return component;
}

function getInternalDataGridShadowRoot(component: Resources.TrustTokensView.TrustTokensView): ShadowRoot {
  const dataGridController = getElementWithinComponent(
      component, 'devtools-data-grid-controller', Components.DataGridController.DataGridController);
  const dataGrid = getElementWithinComponent(dataGridController, 'devtools-data-grid', Components.DataGrid.DataGrid);
  assertShadowRoot(dataGrid.shadowRoot);
  return dataGrid.shadowRoot;
}

describe('TrustTokensView', () => {
  it('renders trust token data', async () => {
    const component = await renderTrustTokensView([
      {issuerOrigin: 'foo.com', count: 42},
      {issuerOrigin: 'bar.org', count: 7},
    ]);

    const dataGridShadowRoot = getInternalDataGridShadowRoot(component);
    const rowValues = getValuesOfAllBodyRows(dataGridShadowRoot);
    assert.deepEqual(rowValues, [
      ['bar.org', '7'],
      ['foo.com', '42'],
    ]);
  });

  it('does not display issuers with zero stored tokens', async () => {
    const component = await renderTrustTokensView([
      {issuerOrigin: 'no-issuer.org', count: 0},
      {issuerOrigin: 'foo.com', count: 42},
    ]);

    const dataGridShadowRoot = getInternalDataGridShadowRoot(component);
    const rowValues = getValuesOfAllBodyRows(dataGridShadowRoot);
    assert.deepEqual(rowValues, [['foo.com', '42']]);
  });

  it('removes trailing slashes from issuer origins', async () => {
    const component = await renderTrustTokensView([
      {issuerOrigin: 'example.com/', count: 20},
      {issuerOrigin: 'sub.domain.org/', count: 14},
    ]);

    const dataGridShadowRoot = getInternalDataGridShadowRoot(component);
    const rowValues = getValuesOfAllBodyRows(dataGridShadowRoot);
    assert.deepEqual(rowValues, [
      ['example.com', '20'],
      ['sub.domain.org', '14'],
    ]);
  });

  it('hides trust token table when there are no trust tokens', async () => {
    const component = await renderTrustTokensView([]);
    assertShadowRoot(component.shadowRoot);

    const nullGridElement = component.shadowRoot.querySelector('devtools-data-grid-controller');
    assert.isNull(nullGridElement);

    const noTrustTokensElement = component.shadowRoot.querySelector('div.no-tt-message');
    assertElement(noTrustTokensElement, HTMLDivElement);
  });
});
