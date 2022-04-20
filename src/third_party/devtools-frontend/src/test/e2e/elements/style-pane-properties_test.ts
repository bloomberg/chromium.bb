// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';
import type * as puppeteer from 'puppeteer';

import {$$, assertNotNullOrUndefined, click, getBrowserAndPages, goToResource, timeout, waitFor, waitForFunction} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {editQueryRuleText, getComputedStylesForDomNode, getDisplayedCSSPropertyNames, getDisplayedStyleRules, getStyleRule, getStyleSectionSubtitles, waitForPartialContentOfSelectedElementsNode, waitForContentOfSelectedElementsNode, waitForElementsStyleSection, waitForPropertyToHighlight, waitForStyleRule} from '../helpers/elements-helpers.js';

const PROPERTIES_TO_DELETE_SELECTOR = '#properties-to-delete';
const PROPERTIES_TO_INSPECT_SELECTOR = '#properties-to-inspect';
const KEYFRAMES_100_PERCENT_RULE_SELECTOR = '100%';
const FIRST_PROPERTY_NAME_SELECTOR = '.tree-outline li:nth-of-type(1) > .webkit-css-property';
const SECOND_PROPERTY_NAME_SELECTOR = '.tree-outline li:nth-of-type(2) > .webkit-css-property';
const FIRST_PROPERTY_VALUE_SELECTOR = '.tree-outline li:nth-of-type(1) > .value';
const RULE1_SELECTOR = '.rule1';
const RULE2_SELECTOR = '.rule2';
const LAYER_SEPARATOR_SELECTOR = '.layer-separator';
const SIDEBAR_SEPARATOR_SELECTOR = '.sidebar-separator';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const deletePropertyByBackspace = async (selector: string, root?: puppeteer.ElementHandle<Element>) => {
  const {frontend} = getBrowserAndPages();
  await click(selector, {root});
  await frontend.keyboard.press('Backspace');
  await frontend.keyboard.press('Tab');
  await waitFor('.tree-outline .child-editing', root);
};

const goToResourceAndWaitForStyleSection = async (path: string) => {
  await goToResource(path);
  await waitForElementsStyleSection();

  // Check to make sure we have the correct node selected after opening a file.
  await waitForPartialContentOfSelectedElementsNode('<body>\u200B');

  // FIXME(crbug/1112692): Refactor test to remove the timeout.
  await timeout(50);
};

describe('The Styles pane', async () => {
  it('can display the CSS properties of the selected element', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/simple-styled-page.html');

    // Select the H1 element by pressing down, since <body> is the default selected element.
    const onH1RuleAppeared = waitForStyleRule('h1');

    await frontend.keyboard.press('ArrowDown');
    await onH1RuleAppeared;

    const h1Rules = await getDisplayedStyleRules();
    // Checking the first h1 rule, that's the authored rule, right after the element style.
    assert.deepEqual(h1Rules[1], {selectorText: 'body h1', propertyNames: ['color']}, 'The correct rule is displayed');

    // Select the H2 element by pressing down.
    const onH2RuleAppeared = waitForStyleRule('h2');
    await frontend.keyboard.press('ArrowDown');
    await onH2RuleAppeared;

    const h2Rules = await getDisplayedStyleRules();
    // Checking the first h2 rule, that's the authored rule, right after the element style.
    assert.deepEqual(
        h2Rules[1], {selectorText: 'h2', propertyNames: ['background-color', 'color']},
        'The correct rule is displayed');
  });

  it('can jump to a CSS variable definition', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-variables.html');

    // Select div that we will inspect the CSS variables for
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"properties-to-inspect">\u200B</div>\u200B');

    const testElementRule = await getStyleRule(PROPERTIES_TO_INSPECT_SELECTOR);
    await click(FIRST_PROPERTY_VALUE_SELECTOR, {root: testElementRule});

    await waitForPropertyToHighlight('html', '--title-color');
  });

  it('can jump to an unexpanded CSS variable definition', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-variables-many.html');

    // Select div that we will inspect the CSS variables for
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"properties-to-inspect">\u200B</div>\u200B');

    const testElementRule = await getStyleRule(PROPERTIES_TO_INSPECT_SELECTOR);
    await click(FIRST_PROPERTY_VALUE_SELECTOR, {root: testElementRule});

    await waitForPropertyToHighlight('html', '--color56');
  });

  it('displays the correct value when editing CSS var() functions', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-variables.html');

    // Select div that we will inspect the CSS variables for
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"properties-to-inspect">\u200B</div>\u200B');

    const propertiesSection = await getStyleRule(PROPERTIES_TO_INSPECT_SELECTOR);

    const propertyValue = await waitFor(FIRST_PROPERTY_VALUE_SELECTOR, propertiesSection);
    // Specifying 10px from the left of the value to click on the word var rather than in the middle which would jump to
    // the property definition.
    await click(propertyValue, {maxPixelsFromLeft: 10});
    const editedValueText = await propertyValue.evaluate(node => node.textContent);
    assert.strictEqual(editedValueText, 'var(--title-color)', 'The value is incorrect when being edited');
  });

  it('generates links inside var() functions for defined properties', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-variables.html');

    // Select div that we will inspect the CSS variables for
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"properties-to-inspect">\u200B</div>\u200B');

    const propertiesSection = await getStyleRule(PROPERTIES_TO_INSPECT_SELECTOR);
    const propertyValue = await waitFor(FIRST_PROPERTY_VALUE_SELECTOR, propertiesSection);
    const link = await $$('.css-var-link', propertyValue);
    assert.strictEqual(link.length, 1, 'The expected var link was not created');
  });

  it('renders computed CSS variables in @keyframes rules', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-variables.html');

    // Select div that we will inspect the CSS variables for
    await frontend.keyboard.press('ArrowRight');
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"keyframes-rule">\u200B</div>\u200B');

    const propertiesSection = await getStyleRule(KEYFRAMES_100_PERCENT_RULE_SELECTOR);
    const propertyValue = await waitFor(FIRST_PROPERTY_VALUE_SELECTOR, propertiesSection);
    const propertyValueText = await propertyValue.evaluate(node => node.textContent);
    assert.strictEqual(
        propertyValueText, 'var( --move-final-width)', 'CSS variable in @keyframes rule is not correctly rendered');
  });

  it('can remove a CSS property when its name or value is deleted', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/style-pane-properties.html');

    // Select div that we will remove the CSS properties from
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div id=\u200B"properties-to-delete">\u200B</div>\u200B');

    const propertiesSection = await getStyleRule(PROPERTIES_TO_DELETE_SELECTOR);
    {
      const displayedNames = await getDisplayedCSSPropertyNames(propertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'height',
            'width',
          ],
          'incorrectly displayed style after initialization');
    }

    // select second property's name and delete
    await deletePropertyByBackspace(SECOND_PROPERTY_NAME_SELECTOR, propertiesSection);

    // verify the second CSS property entry has been removed
    {
      const displayedNames = await getDisplayedCSSPropertyNames(propertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'height',
          ],
          'incorrectly displayed style after removing second property\'s value');
    }

    // select first property's name and delete
    await deletePropertyByBackspace(FIRST_PROPERTY_NAME_SELECTOR, propertiesSection);

    // verify the first CSS property entry has been removed
    {
      const displayedValues = await getDisplayedCSSPropertyNames(propertiesSection);
      assert.deepEqual(displayedValues, [], 'incorrectly displayed style after removing first property\'s name');
    }
  });

  it('can display the source names for stylesheets', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/stylesheets-with-various-sources.html');

    // Select the div element by pressing down, since <body> is the default selected element.
    const onDivRuleAppeared = waitForStyleRule('div');

    await frontend.keyboard.press('ArrowDown');
    await onDivRuleAppeared;

    const subtitles = await getStyleSectionSubtitles();
    assert.deepEqual(
        subtitles,
        [
          '',
          'css-module.css:1',
          'constructed stylesheet',
          'stylesheets…ces.html:10',
          'stylesheets…rces.html:7',
          'user agent stylesheet',
        ],
        'incorrectly displayed style sources');

    const divRules = await getDisplayedStyleRules();
    assert.deepEqual(
        divRules,
        [
          {selectorText: 'element.style', propertyNames: []},
          {selectorText: '#properties-to-inspect', propertyNames: ['height']},
          {selectorText: '#properties-to-inspect', propertyNames: ['color']},
          {selectorText: '#properties-to-inspect', propertyNames: ['text-align']},
          {selectorText: '#properties-to-inspect', propertyNames: ['width']},
          {selectorText: 'div', propertyNames: ['display']},
        ],
        'The correct rule is displayed');
  });

  // Flaky on mac after introducing pooled frontend instances.
  it.skipOnPlatforms(['mac'], '[crbug.com/1297458] can edit multiple constructed stylesheets', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/multiple-constructed-stylesheets.html');

    // Select div that we will remove a CSS property from.
    await frontend.keyboard.press('ArrowRight');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1 rule2">\u200B</div>\u200B');

    // Verify that initial CSS properties correspond to the ones in the test file.
    const rule1PropertiesSection = await getStyleRule(RULE1_SELECTOR);
    const rule2PropertiesSection = await getStyleRule(RULE2_SELECTOR);
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule1PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'background-color',
          ],
          'incorrectly displayed style after initialization');
    }
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule2PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'background-color',
            'color',
          ],
          'incorrectly displayed style after initialization');
    }

    // Select the first property's name of .rule2 (background-color) and delete.
    await deletePropertyByBackspace(FIRST_PROPERTY_NAME_SELECTOR, rule2PropertiesSection);

    // Verify that .rule1 has background-color.
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule1PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'background-color',
          ],
          'incorrectly displayed style after property removal');
    }

    // Verify that .rule2 has background-color removed and only color remains.
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule2PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'color',
          ],
          'incorrectly displayed style after property removal');
    }

    // Verify that computed styles correspond to the changes made.
    const computedStyles = [
      await getComputedStylesForDomNode(RULE1_SELECTOR, 'color'),
      await getComputedStylesForDomNode(RULE1_SELECTOR, 'background-color'),
    ];
    assert.deepEqual(computedStyles, ['rgb(255, 0, 0)', 'rgb(255, 0, 0)'], 'Styles are not correct after the update');
  });

  it('can display and edit container queries', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-container-queries.html');

    // Select the child that has container queries.
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1 rule2">\u200B</div>\u200B');

    // Verify that initial CSS properties correspond to the ones in the test file.
    const rule1PropertiesSection = await getStyleRule(RULE1_SELECTOR);
    const rule2PropertiesSection = await getStyleRule(RULE2_SELECTOR);
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule1PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'width',
          ],
          'incorrectly displayed style after initialization');
    }
    {
      const displayedNames = await getDisplayedCSSPropertyNames(rule2PropertiesSection);
      assert.deepEqual(
          displayedNames,
          [
            'height',
          ],
          'incorrectly displayed style after initialization');
    }

    await editQueryRuleText(rule1PropertiesSection, '(min-width: 300px)');
    await editQueryRuleText(rule2PropertiesSection, '(max-width: 300px)');

    // Verify that computed styles correspond to the changes made.
    const computedStyles = [
      await getComputedStylesForDomNode(RULE1_SELECTOR, 'width'),
      await getComputedStylesForDomNode(RULE2_SELECTOR, 'height'),
    ];
    assert.deepEqual(computedStyles, ['0px', '10px'], 'Styles are not correct after the update');
  });

  it('can display container link', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-container-queries.html');

    // Select the child that has container queries.
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1 rule2">\u200B</div>\u200B');

    const rule1PropertiesSection = await getStyleRule(RULE1_SELECTOR);
    const containerLink = await waitFor('.container-link', rule1PropertiesSection);
    const nodeLabelName = await waitFor('.node-label-name', containerLink);
    const nodeLabelNameContent = await nodeLabelName.evaluate(node => node.textContent as string);
    assert.strictEqual(nodeLabelNameContent, 'body', 'container link name does not match');
    containerLink.hover();
    const queriedSizeDetails = await waitFor('.queried-size-details');
    const queriedSizeDetailsContent =
        await queriedSizeDetails.evaluate(node => (node as HTMLElement).innerText as string);
    assert.strictEqual(
        queriedSizeDetailsContent, '(size) width: 200px height: 0px', 'container queried details does not match');
  });

  it('can display @supports at-rules', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-supports.html');

    // Select the child that has @supports rules.
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1">\u200B</div>\u200B');

    const rule1PropertiesSection = await getStyleRule(RULE1_SELECTOR);
    const supportsQuery = await waitFor('.query.editable', rule1PropertiesSection);
    const supportsQueryText = await supportsQuery.evaluate(node => (node as HTMLElement).innerText as string);
    assert.deepEqual(supportsQueryText, '@supports (width: 10px)', 'incorrectly displayed @supports rule');
  });

  it('can display @layer separators', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-layers.html');

    // Select the child that has @layer rules.
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1">\u200B</div>\u200B');

    const layerSeparators = await waitForFunction(async () => {
      const layers = await $$(LAYER_SEPARATOR_SELECTOR);
      return layers.length === 6 ? layers : null;
    });
    assertNotNullOrUndefined(layerSeparators);

    const layerText = await Promise.all(layerSeparators.map(element => element.evaluate(node => node.textContent)));
    assert.deepEqual(layerText, [
      'Layer<anonymous>',
      'Layerimportant',
      'Layeroverrule',
      'Layeroverrule.<anonymous>',
      'Layerbase',
      'Layer\xa0user\xa0agent\xa0stylesheet',
    ]);
  });

  it('can click @layer separators to open layer tree', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/css-layers.html');

    // Select the child that has @layer rules.
    await frontend.keyboard.press('ArrowDown');
    await waitForContentOfSelectedElementsNode('<div class=\u200B"rule1">\u200B</div>\u200B');

    const overruleButton = await waitFor('overrule[role="button"]', undefined, undefined, 'aria');
    await click(overruleButton);

    const treeElement = await waitFor('[data-node-key="2: overrule"]');
    assertNotNullOrUndefined(treeElement);
  });

  it('can display inherited CSS highlight pseudo styles', async () => {
    const {frontend} = getBrowserAndPages();
    await goToResourceAndWaitForStyleSection('elements/highlight-pseudo-inheritance.html');

    const onH1RuleAppeared = waitForStyleRule('h1');

    // Select the h1 for which we will inspect the pseudo styles
    await frontend.keyboard.press('ArrowRight');

    await onH1RuleAppeared;

    const h1Rules = await getDisplayedStyleRules();
    // The 6 rule blocks for the h1 are:
    // 1. Inline styles from the style attribute
    // 2. The h1's user agent styles
    // 3. The h1's own highlight pseudo
    // 4. The h1's inherited highlight pseudo
    // 5. The h1's own selection pseudo
    // 6. The h1's inherited selection pseudo
    // And there is no 6th block for the ::first-letter style, since only
    // highlight pseudos are inherited.
    assert.strictEqual(h1Rules.length, 6, 'The h1 should have 6 style rule blocks');
    assert.deepEqual(
        h1Rules[2], {selectorText: 'h1::highlight(foo)', propertyNames: ['background-color']},
        'The h1\'s own highlight pseudo is displayed');
    assert.deepEqual(
        h1Rules[3], {selectorText: 'body::highlight(bar)', propertyNames: ['color']},
        'The h1\'s inherited highlight pseudo is displayed');
    assert.deepEqual(
        h1Rules[4], {selectorText: 'h1::selection', propertyNames: ['background-color']},
        'The h1\'s own selection pseudo is displayed');
    assert.deepEqual(
        h1Rules[5], {selectorText: 'body::selection', propertyNames: ['text-shadow']},
        'The h1\'s inherited selection pseudo is displayed');

    const sidebarSeparators = await waitForFunction(async () => {
      const separators = await $$(SIDEBAR_SEPARATOR_SELECTOR);
      return separators.length === 4 ? separators : null;
    });
    assertNotNullOrUndefined(sidebarSeparators);

    const layerText = await Promise.all(sidebarSeparators.map(element => element.evaluate(node => node.textContent)));
    assert.deepEqual(layerText, [
      'Pseudo ::highlight element',
      'Inherited from ::highlight pseudo of ',
      'Pseudo ::selection element',
      'Inherited from ::selection pseudo of ',
    ]);
  });
});
