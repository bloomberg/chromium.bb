// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import {assertNotNullOrUndefined, enableExperiment, getBrowserAndPages, goToResource, waitFor} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {getHiddenIssuesRow, getHiddenIssuesRowBody, getHideIssuesMenu, getHideIssuesMenuItem, getIssueHeaderByTitle, getUnhideAllIssuesBtn, getUnhideIssuesMenuItem, ISSUE, navigateToIssuesTab} from '../helpers/issues-helpers.js';

describe('Hide issues menu', async () => {
  it('should be appended to the issue header', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('issues/sab-issue.rawresponse');
    await navigateToIssuesTab();
    const issueTitle = 'SharedArrayBuffer usage is restricted to cross-origin isolated sites';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    const hideIssuesMenu = await getHideIssuesMenu();
    const classList = await hideIssuesMenu.evaluate(node => node.classList.toString());
    assert.include(classList, 'hide-issues-menu hidden');
  });

  it('should become visible on hovering over the issue header', async () => {
    await enableExperiment('hideIssuesFeature');
    const {frontend} = getBrowserAndPages();
    frontend.evaluate(() => {
      const issue = {
        'code': 'HeavyAdIssue',
        'details': {
          'heavyAdIssueDetails': {
            'resolution': 'HeavyAdBlocked',
            'reason': 'NetworkTotalLimit',
            'frame': {frameId: 'main'},
          },
        },
      };
      // @ts-ignore
      window.addIssueForTest(issue);
    });
    await navigateToIssuesTab();
    const issueTitle = 'An ad on your site has exceeded resource limits';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    const hideIssuesMenu = await getHideIssuesMenu();
    let classList = await hideIssuesMenu.evaluate(node => node.classList.toString());
    assert.include(classList, 'hide-issues-menu hidden');
    await issueHeader.hover();
    classList = await hideIssuesMenu.evaluate(node => node.classList.toString());
    assert.notInclude(classList, 'hidden');
  });

  it('should open a context menu upon clicking', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('empty.html');
    const {target} = getBrowserAndPages();
    await target.evaluate(async () => {
      try {
        const url = new URL('./issues/origin-wildcard.rawresponse', document.location.toString())
                        .toString()
                        .replace('localhost', 'devtools.oopif.test');
        await fetch(url, {credentials: 'include'});
      } catch (e) {
      }
    });
    await navigateToIssuesTab();
    const issueTitle = 'Ensure credentialed requests are not sent to CORS resources with origin wildcards';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    await issueHeader.hover();
    const hideIssuesMenu = await getHideIssuesMenu();
    await hideIssuesMenu.click();
    const menuItem = await getHideIssuesMenuItem();
    assertNotNullOrUndefined(menuItem);
    const content = await menuItem.evaluate(node => node.textContent);
    assertNotNullOrUndefined(content);
    assert.include(content, 'Hide issues like this');
  });

  it('should hide issue upon clicking the context menu entry', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('issues/sab-issue.rawresponse');
    await navigateToIssuesTab();

    const issueTitle = 'SharedArrayBuffer usage is restricted to cross-origin isolated sites';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    await issueHeader.hover();
    const hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    const menuItem = await getHideIssuesMenuItem();
    assertNotNullOrUndefined(menuItem);
    await waitFor(ISSUE);
    await menuItem.click();
    await waitFor('.hidden-issue');
  });

  it('should unhide all issues upon clicking unhide all issues button', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('issues/sab-issue.rawresponse');
    await navigateToIssuesTab();
    const issueTitle = 'SharedArrayBuffer usage is restricted to cross-origin isolated sites';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    await issueHeader.hover();
    const hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    const menuItem = await getHideIssuesMenuItem();
    assertNotNullOrUndefined(menuItem);
    await menuItem.click();
    await waitFor('.hidden-issue');
    const btn = await getUnhideAllIssuesBtn();
    await btn.click();
    await waitFor(ISSUE);
  });

  it('should contain unhide issues like this entry while hovering over a hidden issue', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('issues/sab-issue.rawresponse');
    await navigateToIssuesTab();
    const issueTitle = 'SharedArrayBuffer usage is restricted to cross-origin isolated sites';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    await issueHeader.hover();
    let hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    const menuItem = await getHideIssuesMenuItem();
    assertNotNullOrUndefined(menuItem);
    await menuItem.click();
    await waitFor('.hidden-issue');
    const row = await getHiddenIssuesRow();
    let isHidden = await row?.evaluate(node => node.classList.contains('hidden'));
    assert.isFalse(isHidden);
    await row?.click();
    const body = await getHiddenIssuesRowBody();
    isHidden = await body?.evaluate(node => node.classList.contains('hidden'));
    assert.isFalse(isHidden);
    const hiddenIssueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(hiddenIssueHeader);
    await hiddenIssueHeader.hover();
    hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    await getUnhideIssuesMenuItem();
  });

  it('should unhide issue after clicking the unhide issues like this entry', async () => {
    await enableExperiment('hideIssuesFeature');
    await goToResource('issues/sab-issue.rawresponse');
    await navigateToIssuesTab();
    const issueTitle = 'SharedArrayBuffer usage is restricted to cross-origin isolated sites';
    const issueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(issueHeader);
    await issueHeader.hover();
    let hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    const menuItem = await getHideIssuesMenuItem();
    assertNotNullOrUndefined(menuItem);
    await menuItem.click();
    await waitFor('.hidden-issue');
    const row = await getHiddenIssuesRow();
    let isHidden = await row?.evaluate(node => node.classList.contains('hidden'));
    assert.isFalse(isHidden);
    await row?.click();
    const body = await getHiddenIssuesRowBody();
    isHidden = await body?.evaluate(node => node.classList.contains('hidden'));
    assert.isFalse(isHidden);
    const hiddenIssueHeader = await getIssueHeaderByTitle(issueTitle);
    assertNotNullOrUndefined(hiddenIssueHeader);
    await hiddenIssueHeader.hover();
    hideIssuesMenuBtn = await getHideIssuesMenu();
    await hideIssuesMenuBtn.click();
    const unhideMenuItem = await getUnhideIssuesMenuItem();
    await unhideMenuItem?.click();
    await waitFor(ISSUE);
  });
});
