// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotNullOrUndefined, getBrowserAndPages, goToResource} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {
  ensureResourceSectionIsExpanded,
  expandIssue,
  getIssueByTitle,
  getResourcesElement,
  navigateToIssuesTab,
  waitForTableFromResourceSectionContents,
} from '../helpers/issues-helpers.js';

describe('Deprecation Issues', async () => {
  beforeEach(async () => {
    await goToResource('empty.html');
  });

  it('evaluation works', async () => {
    await navigateToIssuesTab();
    const {frontend} = getBrowserAndPages();
    frontend.evaluate(() => {
      const issue = {
        code: 'DeprecationIssue',
        details: {
          deprecationIssueDetails: {
            sourceCodeLocation: {
              url: 'empty.html',
              lineNumber: 1,
              columnNumber: 1,
            },
            type: 'DeprecationExample',
          },
        },
      };
      // @ts-ignore
      window.addIssueForTest(issue);
    });

    await expandIssue();
    const issueElement = await getIssueByTitle('Deprecated Feature Used');
    assertNotNullOrUndefined(issueElement);
    const section = await getResourcesElement('1 source', issueElement, '.affected-resource-label');
    await ensureResourceSectionIsExpanded(section);
    const expectedTableRows = [
      ['empty.html:2'],
    ];
    await waitForTableFromResourceSectionContents(section.content, expectedTableRows);
  });
});
