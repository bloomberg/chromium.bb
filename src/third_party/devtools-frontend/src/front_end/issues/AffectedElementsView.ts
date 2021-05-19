// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../core/common/common.js';
import * as Host from '../core/host/host.js';
import * as i18n from '../core/i18n/i18n.js';
import * as Platform from '../core/platform/platform.js';
import * as SDK from '../core/sdk/sdk.js';

import {AffectedItem, AffectedResourcesView} from './AffectedResourcesView.js';
import {IssueView} from './IssueView.js';

const UIStrings = {
  /**
  *@description Noun for singular or plural number of affected element resource indication in issue view.
  */
  nElements: '{n, plural, =1 { element} other { elements}}',
  /**
  *@description Singular label for number of affected element resource indication in issue view
  */
  element: 'element',
  /**
  *@description Plural label for number of affected element resource indication in issue view
  */
  elements: 'elements',
};
const str_ = i18n.i18n.registerUIStrings('issues/AffectedElementsView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
export class AffectedElementsView extends AffectedResourcesView {
  private issue: SDK.Issue.Issue;

  constructor(parent: IssueView, issue: SDK.Issue.Issue) {
    super(parent, {singular: i18nString(UIStrings.element), plural: i18nString(UIStrings.elements)});
    this.issue = issue;
  }

  private sendTelemetry(): void {
    Host.userMetrics.issuesPanelResourceOpened(this.issue.getCategory(), AffectedItem.Element);
  }

  private async appendAffectedElements(affectedElements: Iterable<SDK.Issue.AffectedElement>): Promise<void> {
    let count = 0;
    for (const element of affectedElements) {
      await this.appendAffectedElement(element);
      count++;
    }
    this.updateAffectedResourceCount(count);
  }

  protected getResourceName(count: number): Platform.UIString.LocalizedString {
    return i18nString(UIStrings.nElements, {n: count});
  }

  private async appendAffectedElement(element: SDK.Issue.AffectedElement): Promise<void> {
    const cellElement = await this.renderElementCell(element);
    const rowElement = document.createElement('tr');
    rowElement.appendChild(cellElement);
    this.affectedResources.appendChild(rowElement);
  }

  protected async renderElementCell({backendNodeId, nodeName}: SDK.Issue.AffectedElement): Promise<Element> {
    const mainTarget = SDK.SDKModel.TargetManager.instance().mainTarget() as SDK.SDKModel.Target;
    const deferredDOMNode = new SDK.DOMModel.DeferredDOMNode(mainTarget, backendNodeId);
    const anchorElement = (await Common.Linkifier.Linkifier.linkify(deferredDOMNode)) as HTMLElement;
    anchorElement.textContent = nodeName;
    anchorElement.addEventListener('click', () => this.sendTelemetry());
    anchorElement.addEventListener('keydown', (event: Event) => {
      if ((event as KeyboardEvent).key === 'Enter') {
        this.sendTelemetry();
      }
    });
    const cellElement = document.createElement('td');
    cellElement.classList.add('affected-resource-element', 'devtools-link');
    cellElement.appendChild(anchorElement);
    return cellElement;
  }

  update(): void {
    this.clear();
    this.appendAffectedElements(this.issue.elements());
  }
}
