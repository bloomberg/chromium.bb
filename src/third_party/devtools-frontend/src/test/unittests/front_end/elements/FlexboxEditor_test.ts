// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as ElementsModule from '../../../../front_end/elements/elements.js';
import {describeWithEnvironment} from '../helpers/EnvironmentHelpers.js';
import {assertElement, assertShadowRoot, getEventPromise, renderElementIntoDOM} from '../helpers/DOMHelpers.js';

const {assert} = chai;

describeWithEnvironment('FlexboxEditor', async () => {
  let Elements: typeof ElementsModule;

  before(async () => {
    Elements = await import('../../../../front_end/elements/elements.js');
  });

  function assertValues(component: HTMLElement, values: string[]) {
    assertShadowRoot(component.shadowRoot);
    const propertyElements = component.shadowRoot.querySelectorAll('.property');
    const properties = [];
    for (const propElement of propertyElements) {
      properties.push(propElement.textContent?.trim());
    }
    assert.deepEqual(properties, values);
  }

  it('renders the editor', async () => {
    const component = new Elements.FlexboxEditor.FlexboxEditor();
    renderElementIntoDOM(component);
    component.data = {
      authoredProperties: new Map([
        ['flex-direction', 'row'],
        ['flex-wrap', 'wrap'],
        ['align-content', 'flex-end'],
        ['justify-content', 'flex-start'],
        ['align-items', 'flex-start'],
      ]),
      computedProperties: new Map(),
    };
    assertValues(component, [
      'flex-direction: row',
      'flex-wrap: wrap',
      'align-content: flex-end',
      'justify-content: flex-start',
      'align-items: flex-start',
    ]);
    component.data = {
      authoredProperties: new Map(),
      computedProperties: new Map([
        ['flex-direction', 'row'],
        ['flex-wrap', 'wrap'],
        ['align-content', 'flex-end'],
        ['justify-content', 'flex-start'],
        ['align-items', 'flex-start'],
      ]),
    };
    assertValues(component, [
      'flex-direction: row',
      'flex-wrap: wrap',
      'align-content: flex-end',
      'justify-content: flex-start',
      'align-items: flex-start',
    ]);
    component.data = {
      authoredProperties: new Map(),
      computedProperties: new Map(),
    };
    assertValues(component, ['flex-direction:', 'flex-wrap:', 'align-content:', 'justify-content:', 'align-items:']);
  });

  it('allows selecting a property value', async () => {
    const component = new Elements.FlexboxEditor.FlexboxEditor();
    renderElementIntoDOM(component);
    component.data = {
      authoredProperties: new Map([
        ['flex-direction', 'row'],
      ]),
      computedProperties: new Map(),
    };
    assertValues(
        component, ['flex-direction: row', 'flex-wrap:', 'align-content:', 'justify-content:', 'align-items:']);
    const eventPromise =
        getEventPromise<ElementsModule.FlexboxEditor.PropertySelectedEvent>(component, 'property-selected');
    assertShadowRoot(component.shadowRoot);
    const flexDirectionColumnButton = component.shadowRoot.querySelector('.row .buttons .button:nth-child(2)');
    assertElement(flexDirectionColumnButton, HTMLButtonElement);
    flexDirectionColumnButton.click();
    const event = await eventPromise;
    assert.deepEqual(event.data, {name: 'flex-direction', value: 'column'});
  });

  it('allows deselecting a property value', async () => {
    const component = new Elements.FlexboxEditor.FlexboxEditor();
    renderElementIntoDOM(component);
    component.data = {
      authoredProperties: new Map([
        ['flex-direction', 'column'],
      ]),
      computedProperties: new Map(),
    };
    assertValues(
        component, ['flex-direction: column', 'flex-wrap:', 'align-content:', 'justify-content:', 'align-items:']);
    const eventPromise =
        getEventPromise<ElementsModule.FlexboxEditor.PropertyDeselectedEvent>(component, 'property-deselected');
    assertShadowRoot(component.shadowRoot);
    const flexDirectionColumnButton = component.shadowRoot.querySelector('.row .buttons .button:nth-child(2)');
    assertElement(flexDirectionColumnButton, HTMLButtonElement);
    flexDirectionColumnButton.click();
    const event = await eventPromise;
    assert.deepEqual(event.data, {name: 'flex-direction', value: 'column'});
  });
});
