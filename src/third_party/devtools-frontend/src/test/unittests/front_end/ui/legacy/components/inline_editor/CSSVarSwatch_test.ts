// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotNullOrUndefined} from '../../../../../../../front_end/core/platform/platform.js';
import * as InlineEditor from '../../../../../../../front_end/ui/legacy/components/inline_editor/inline_editor.js';
import {assertShadowRoot, renderElementIntoDOM} from '../../../../helpers/DOMHelpers.js';
import {describeWithLocale} from '../../../../helpers/EnvironmentHelpers.js';

const {assert} = chai;

function assertSwatch(swatch: InlineEditor.CSSVarSwatch.CSSVarSwatch, expected: {
  valueTooltip: string|null,
  linkTooltip: string,
  isDefined: boolean,
  varText: string,
  parsedColor?: string,
}) {
  assertShadowRoot(swatch.shadowRoot);
  const container = swatch.shadowRoot.querySelector('span');
  assertNotNullOrUndefined(container);

  const link = container.querySelector('.css-var-link');
  assertNotNullOrUndefined(link);

  assert.strictEqual(
      container.getAttribute('title'), expected.valueTooltip || '', 'The computed values appears as a tooltip');
  assert.strictEqual(
      link.classList.contains('undefined'), !expected.isDefined,
      'The link only has the class undefined when the property is undefined');
  assert.strictEqual(link.getAttribute('title'), expected.linkTooltip, 'The link has the right tooltip');
  assert.strictEqual(link.textContent, expected.varText, 'The link has the right text content');
}

describeWithLocale('CSSVarSwatch', () => {
  it('can be instantiated successfully', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);

    assert.instanceOf(component, HTMLElement, 'The swatch is an instance of HTMLElement');
  });

  it('renders a simple var function', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--test)',
      computedValue: '2px',
      fromFallback: false,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: '2px',
      linkTooltip: '2px',
      isDefined: true,
      varText: '--test',
    });
  });

  it('renders a var function with an undefined property', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--undefined)',
      computedValue: null,
      fromFallback: false,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: null,
      linkTooltip: '--undefined is not defined',
      isDefined: false,
      varText: '--undefined',
    });
  });

  it('renders a var function with an undefined property but a fallback value', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--undefined, 3px)',
      computedValue: '3px',
      fromFallback: true,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: '3px',
      linkTooltip: '--undefined is not defined',
      isDefined: false,
      varText: '--undefined',
    });
  });

  it('renders a var() function with an color property but a fallback value', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--undefined-color, green)',
      computedValue: 'green',
      fromFallback: true,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: 'green',
      linkTooltip: '--undefined-color is not defined',
      isDefined: false,
      varText: '--undefined-color',
    });
  });

  it('render the var() function and the fallback value contains spaces', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--undefined-color,    green   )',
      computedValue: 'green',
      fromFallback: true,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: 'green',
      linkTooltip: '--undefined-color is not defined',
      isDefined: false,
      varText: '--undefined-color',
    });
  });

  it('renders a var() function with an color property', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var(--test, green)',
      computedValue: 'red',
      fromFallback: false,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: 'red',
      linkTooltip: 'red',
      isDefined: true,
      varText: '--test',
    });
  });

  it('renders a var() function with spaces', () => {
    const component = new InlineEditor.CSSVarSwatch.CSSVarSwatch();
    renderElementIntoDOM(component);
    component.data = {
      text: 'var( --test     )',
      computedValue: 'red',
      fromFallback: false,
      onLinkActivate: () => {},
    };

    assertSwatch(component, {
      valueTooltip: 'red',
      linkTooltip: 'red',
      isDefined: true,
      varText: ' --test ',
    });
  });
});
