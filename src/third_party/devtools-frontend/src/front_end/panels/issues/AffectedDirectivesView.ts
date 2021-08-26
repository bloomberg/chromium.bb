// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable rulesdir/no_underscored_properties */

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import type * as Platform from '../../core/platform/platform.js';
import type * as Protocol from '../../generated/protocol.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as IssuesManager from '../../models/issues_manager/issues_manager.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as ElementsComponents from '../elements/components/components.js';

import {AffectedItem, AffectedResourcesView} from './AffectedResourcesView.js';

import type {AggregatedIssue} from './IssueAggregator.js';
import type {IssueView} from './IssueView.js';

const UIStrings = {
  /**
  *@description Singular or plural label for number of affected CSP (content security policy,
  * see https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP) directives in issue view.
  */
  nDirectives: '{n, plural, =1 {# directive} other {# directives}}',
  /**
  *@description Indicates that a CSP error should be treated as a warning
  */
  reportonly: 'report-only',
  /**
  *@description The kind of resolution for a mixed content issue
  */
  blocked: 'blocked',
  /**
  *@description Tooltip for button linking to the Elements panel
  */
  clickToRevealTheViolatingDomNode: 'Click to reveal the violating DOM node in the Elements panel',
  /**
  *@description Header for the section listing affected directives
  */
  directiveC: 'Directive',
  /**
  *@description Label for the column in the element list in the CSS Overview report
  */
  element: 'Element',
  /**
  *@description Header for the source location column
  */
  sourceLocation: 'Source Location',
  /**
  *@description Text for the status of something
  */
  status: 'Status',
  /**
  *@description Text that refers to the resources of the web page
  */
  resourceC: 'Resource',
};

const str_ = i18n.i18n.registerUIStrings('panels/issues/AffectedDirectivesView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export class AffectedDirectivesView extends AffectedResourcesView {
  _issue: AggregatedIssue;
  constructor(parent: IssueView, issue: AggregatedIssue) {
    super(parent);
    this._issue = issue;
  }

  _appendStatus(element: Element, isReportOnly: boolean): void {
    const status = document.createElement('td');
    if (isReportOnly) {
      status.classList.add('affected-resource-report-only-status');
      status.textContent = i18nString(UIStrings.reportonly);
    } else {
      status.classList.add('affected-resource-blocked-status');
      status.textContent = i18nString(UIStrings.blocked);
    }
    element.appendChild(status);
  }

  protected getResourceNameWithCount(count: number): Platform.UIString.LocalizedString {
    return i18nString(UIStrings.nDirectives, {n: count});
  }

  _appendViolatedDirective(element: Element, directive: string): void {
    const violatedDirective = document.createElement('td');
    violatedDirective.textContent = directive;
    element.appendChild(violatedDirective);
  }

  _appendBlockedURL(element: Element, url: string): void {
    const info = document.createElement('td');
    info.classList.add('affected-resource-directive-info');
    info.textContent = url;
    element.appendChild(info);
  }

  _appendBlockedElement(
      element: Element, nodeId: Protocol.DOM.BackendNodeId|undefined, model: SDK.IssuesModel.IssuesModel): void {
    const elementsPanelLinkComponent = new ElementsComponents.ElementsPanelLink.ElementsPanelLink();
    if (nodeId) {
      const violatingNodeId = nodeId;
      UI.Tooltip.Tooltip.install(elementsPanelLinkComponent, i18nString(UIStrings.clickToRevealTheViolatingDomNode));

      const onElementRevealIconClick: (arg0?: Event|undefined) => void = (): void => {
        const target = model.getTargetIfNotDisposed();
        if (target) {
          Host.userMetrics.issuesPanelResourceOpened(this._issue.getCategory(), AffectedItem.Element);
          const deferredDOMNode = new SDK.DOMModel.DeferredDOMNode(target, violatingNodeId);
          Common.Revealer.reveal(deferredDOMNode);
        }
      };

      const onElementRevealIconMouseEnter: (arg0?: Event|undefined) => void = (): void => {
        const target = model.getTargetIfNotDisposed();
        if (target) {
          const deferredDOMNode = new SDK.DOMModel.DeferredDOMNode(target, violatingNodeId);
          if (deferredDOMNode) {
            deferredDOMNode.highlight();
          }
        }
      };

      const onElementRevealIconMouseLeave: (arg0?: Event|undefined) => void = (): void => {
        SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
      };

      elementsPanelLinkComponent
          .data = {onElementRevealIconClick, onElementRevealIconMouseEnter, onElementRevealIconMouseLeave};
    }

    const violatingNode = document.createElement('td');
    violatingNode.classList.add('affected-resource-csp-info-node');
    violatingNode.appendChild(elementsPanelLinkComponent);
    element.appendChild(violatingNode);
  }

  _appendAffectedContentSecurityPolicyDetails(
      cspIssues: Iterable<IssuesManager.ContentSecurityPolicyIssue.ContentSecurityPolicyIssue>): void {
    const header = document.createElement('tr');
    if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.inlineViolationCode) {
      this.appendColumnTitle(header, i18nString(UIStrings.directiveC));
      this.appendColumnTitle(header, i18nString(UIStrings.element));
      this.appendColumnTitle(header, i18nString(UIStrings.sourceLocation));
      this.appendColumnTitle(header, i18nString(UIStrings.status));
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.urlViolationCode) {
      this.appendColumnTitle(header, i18nString(UIStrings.resourceC), 'affected-resource-directive-info-header');
      this.appendColumnTitle(header, i18nString(UIStrings.status));
      this.appendColumnTitle(header, i18nString(UIStrings.directiveC));
      this.appendColumnTitle(header, i18nString(UIStrings.sourceLocation));
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.evalViolationCode) {
      this.appendColumnTitle(header, i18nString(UIStrings.sourceLocation));
      this.appendColumnTitle(header, i18nString(UIStrings.directiveC));
      this.appendColumnTitle(header, i18nString(UIStrings.status));
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.trustedTypesSinkViolationCode) {
      this.appendColumnTitle(header, i18nString(UIStrings.sourceLocation));
      this.appendColumnTitle(header, i18nString(UIStrings.status));
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.trustedTypesPolicyViolationCode) {
      this.appendColumnTitle(header, i18nString(UIStrings.sourceLocation));
      this.appendColumnTitle(header, i18nString(UIStrings.directiveC));
      this.appendColumnTitle(header, i18nString(UIStrings.status));
    } else {
      this.updateAffectedResourceCount(0);
      return;
    }
    this.affectedResources.appendChild(header);
    let count = 0;
    for (const cspIssue of cspIssues) {
      count++;
      this._appendAffectedContentSecurityPolicyDetail(cspIssue);
    }
    this.updateAffectedResourceCount(count);
  }

  _appendAffectedContentSecurityPolicyDetail(
      cspIssue: IssuesManager.ContentSecurityPolicyIssue.ContentSecurityPolicyIssue): void {
    const element = document.createElement('tr');
    element.classList.add('affected-resource-directive');

    const cspIssueDetails = cspIssue.details();
    const location = IssuesManager.Issue.toZeroBasedLocation(cspIssueDetails.sourceCodeLocation);
    const model = cspIssue.model();
    const maybeTarget = cspIssue.model()?.getTargetIfNotDisposed();
    if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.inlineViolationCode && model) {
      this._appendViolatedDirective(element, cspIssueDetails.violatedDirective);
      this._appendBlockedElement(element, cspIssueDetails.violatingNodeId, model);
      this.appendSourceLocation(element, location, maybeTarget);
      this._appendStatus(element, cspIssueDetails.isReportOnly);
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.urlViolationCode) {
      const url = cspIssueDetails.blockedURL ? cspIssueDetails.blockedURL : '';
      this._appendBlockedURL(element, url);
      this._appendStatus(element, cspIssueDetails.isReportOnly);
      this._appendViolatedDirective(element, cspIssueDetails.violatedDirective);
      this.appendSourceLocation(element, location, maybeTarget);
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.evalViolationCode) {
      this.appendSourceLocation(element, location, maybeTarget);
      this._appendViolatedDirective(element, cspIssueDetails.violatedDirective);
      this._appendStatus(element, cspIssueDetails.isReportOnly);
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.trustedTypesSinkViolationCode) {
      this.appendSourceLocation(element, location, maybeTarget);
      this._appendStatus(element, cspIssueDetails.isReportOnly);
    } else if (this._issue.code() === IssuesManager.ContentSecurityPolicyIssue.trustedTypesPolicyViolationCode) {
      this.appendSourceLocation(element, location, maybeTarget);
      this._appendViolatedDirective(element, cspIssueDetails.violatedDirective);
      this._appendStatus(element, cspIssueDetails.isReportOnly);
    } else {
      return;
    }

    this.affectedResources.appendChild(element);
  }

  update(): void {
    this.clear();
    this._appendAffectedContentSecurityPolicyDetails(this._issue.getCspIssues());
  }
}
