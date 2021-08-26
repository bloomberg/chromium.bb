// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable rulesdir/no_underscored_properties */

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import type * as Platform from '../../core/platform/platform.js';
import * as Protocol from '../../generated/protocol.js';
import * as IssuesManager from '../../models/issues_manager/issues_manager.js';
import * as IconButton from '../../ui/components/icon_button/icon_button.js';
import * as IssueCounter from '../../ui/components/issue_counter/issue_counter.js';
import * as MarkdownView from '../../ui/components/markdown_view/markdown_view.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Adorners from '../../ui/components/adorners/adorners.js';
import * as NetworkForward from '../../panels/network/forward/forward.js';
import * as Root from '../../core/root/root.js';
import * as Components from './components/components.js';

import {AffectedDirectivesView} from './AffectedDirectivesView.js';
import {AffectedBlockedByResponseView} from './AffectedBlockedByResponseView.js';
import {AffectedCookiesView, AffectedRawCookieLinesView} from './AffectedCookiesView.js';
import {AffectedDocumentsInQuirksModeView} from './AffectedDocumentsInQuirksModeView.js';
import {AffectedElementsView} from './AffectedElementsView.js';
import {AffectedElementsWithLowContrastView} from './AffectedElementsWithLowContrastView.js';
import {AffectedHeavyAdView} from './AffectedHeavyAdView.js';
import {AffectedItem, AffectedResourcesView, extractShortPath} from './AffectedResourcesView.js';
import {AffectedSharedArrayBufferIssueDetailsView} from './AffectedSharedArrayBufferIssueDetailsView.js';
import {AffectedSourcesView} from './AffectedSourcesView.js';
import {AffectedTrustedWebActivityIssueDetailsView} from './AffectedTrustedWebActivityIssueDetailsView.js';
import {CorsIssueDetailsView} from './CorsIssueDetailsView.js';
import {WasmCrossOriginModuleSharingAffectedResourcesView} from './WasmCrossOriginModuleSharingAffectedResourcesView.js';
import {AttributionReportingIssueDetailsView} from './AttributionReportingIssueDetailsView.js';

import type {AggregatedIssue} from './IssueAggregator.js';
import type {HiddenIssuesMenuData} from './components/HideIssuesMenu.js';

const UIStrings = {
  /**
  *@description Noun, singular. Label for a column or field containing the name of an entity.
  */
  name: 'Name',
  /**
  *@description The kind of resolution for a mixed content issue
  */
  blocked: 'blocked',
  /**
  *@description Label for a type of issue that can appear in the Issues view. Noun for singular or plural number of network requests.
  */
  nRequests: '{n, plural, =1 {# request} other {# requests}}',
  /**
  *@description Label for singular or plural number of affected resources in issue view
  */
  nResources: '{n, plural, =1 {# resource} other {# resources}}',
  /**
  *@description Label for mixed content issue's restriction status
  */
  restrictionStatus: 'Restriction Status',
  /**
  * @description When there is a Heavy Ad, the browser can choose to deal with it in different ways.
  * This string indicates that the ad was only warned, and not removed.
  */
  warned: 'Warned',
  /**
  *@description Header for the section listing affected resources
  */
  affectedResources: 'Affected Resources',
  /**
  *@description Title for a link to further information in issue view
  *@example {SameSite Cookies Explained} PH1
  */
  learnMoreS: 'Learn more: {PH1}',
  /**
 *@description The kind of resolution for a mixed content issue
 */
  automaticallyUpgraded: 'automatically upgraded',
};
const str_ = i18n.i18n.registerUIStrings('panels/issues/IssueView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

class AffectedRequestsView extends AffectedResourcesView {
  _issue: IssuesManager.Issue.Issue;
  constructor(parent: IssueView, issue: IssuesManager.Issue.Issue) {
    super(parent);
    this._issue = issue;
  }

  _appendAffectedRequests(affectedRequests: Iterable<Protocol.Audits.AffectedRequest>): void {
    let count = 0;
    for (const affectedRequest of affectedRequests) {
      const element = document.createElement('tr');
      element.classList.add('affected-resource-request');
      const category = this._issue.getCategory();
      const tab = issueTypeToNetworkHeaderMap.get(category) || NetworkForward.UIRequestLocation.UIRequestTabs.Headers;
      element.appendChild(this.createRequestCell(affectedRequest, {
        networkTab: tab,
        additionalOnClickAction() {
          Host.userMetrics.issuesPanelResourceOpened(category, AffectedItem.Request);
        },
      }));
      this.affectedResources.appendChild(element);
      count++;
    }
    this.updateAffectedResourceCount(count);
  }

  protected getResourceNameWithCount(count: number): Platform.UIString.LocalizedString {
    return i18nString(UIStrings.nRequests, {n: count});
  }

  update(): void {
    this.clear();
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    for (const unused of this._issue.getBlockedByResponseDetails()) {
      // If the issue has blockedByResponseDetails, the corresponding AffectedBlockedByResponseView
      // will take care of displaying the request.
      this.updateAffectedResourceCount(0);
      return;
    }
    if (this._issue.getCategory() === IssuesManager.Issue.IssueCategory.MixedContent) {
      // The AffectedMixedContentView takes care of displaying the resources.
      this.updateAffectedResourceCount(0);
      return;
    }
    this._appendAffectedRequests(this._issue.requests());
  }
}

const issueTypeToNetworkHeaderMap =
    new Map<IssuesManager.Issue.IssueCategory, NetworkForward.UIRequestLocation.UIRequestTabs>([
      [
        IssuesManager.Issue.IssueCategory.SameSiteCookie,
        NetworkForward.UIRequestLocation.UIRequestTabs.Cookies,
      ],
      [
        IssuesManager.Issue.IssueCategory.CrossOriginEmbedderPolicy,
        NetworkForward.UIRequestLocation.UIRequestTabs.Headers,
      ],
      [
        IssuesManager.Issue.IssueCategory.MixedContent,
        NetworkForward.UIRequestLocation.UIRequestTabs.Headers,
      ],
    ]);

class AffectedMixedContentView extends AffectedResourcesView {
  _issue: AggregatedIssue;
  constructor(parent: IssueView, issue: AggregatedIssue) {
    super(parent);
    this._issue = issue;
  }

  _appendAffectedMixedContentDetails(mixedContentIssues: Iterable<IssuesManager.MixedContentIssue.MixedContentIssue>):
      void {
    const header = document.createElement('tr');
    this.appendColumnTitle(header, i18nString(UIStrings.name));
    this.appendColumnTitle(header, i18nString(UIStrings.restrictionStatus));

    this.affectedResources.appendChild(header);

    let count = 0;
    for (const issue of mixedContentIssues) {
      const details = issue.getDetails();
      this.appendAffectedMixedContent(details);
      count++;
    }
    this.updateAffectedResourceCount(count);
  }

  protected getResourceNameWithCount(count: number): Platform.UIString.LocalizedString {
    return i18nString(UIStrings.nResources, {n: count});
  }

  appendAffectedMixedContent(mixedContent: Protocol.Audits.MixedContentIssueDetails): void {
    const element = document.createElement('tr');
    element.classList.add('affected-resource-mixed-content');

    if (mixedContent.request) {
      const networkTab = issueTypeToNetworkHeaderMap.get(this._issue.getCategory()) ||
          NetworkForward.UIRequestLocation.UIRequestTabs.Headers;
      element.appendChild(this.createRequestCell(mixedContent.request, {
        networkTab,
        additionalOnClickAction() {
          Host.userMetrics.issuesPanelResourceOpened(
              IssuesManager.Issue.IssueCategory.MixedContent, AffectedItem.Request);
        },
      }));
    } else {
      const filename = extractShortPath(mixedContent.insecureURL);
      const cell = this.appendIssueDetailCell(element, filename, 'affected-resource-mixed-content-info');
      UI.Tooltip.Tooltip.install(cell, mixedContent.insecureURL);
    }

    this.appendIssueDetailCell(
        element, AffectedMixedContentView.translateStatus(mixedContent.resolutionStatus),
        'affected-resource-mixed-content-info');
    this.affectedResources.appendChild(element);
  }

  private static translateStatus(resolutionStatus: Protocol.Audits.MixedContentResolutionStatus):
      Platform.UIString.LocalizedString {
    switch (resolutionStatus) {
      case Protocol.Audits.MixedContentResolutionStatus.MixedContentBlocked:
        return i18nString(UIStrings.blocked);
      case Protocol.Audits.MixedContentResolutionStatus.MixedContentAutomaticallyUpgraded:
        return i18nString(UIStrings.automaticallyUpgraded);
      case Protocol.Audits.MixedContentResolutionStatus.MixedContentWarning:
        return i18nString(UIStrings.warned);
    }
  }

  update(): void {
    this.clear();
    this._appendAffectedMixedContentDetails(this._issue.getMixedContentIssues());
  }
}

export class IssueView extends UI.TreeOutline.TreeElement {
  _issue: AggregatedIssue;
  _description: IssuesManager.MarkdownIssueDescription.IssueDescription;
  toggleOnClick: boolean;
  affectedResources: UI.TreeOutline.TreeElement;
  _affectedResourceViews: AffectedResourcesView[];
  _aggregatedIssuesCount: HTMLElement|null;
  private issueKindIcon: IconButton.Icon.Icon|null = null;
  _hasBeenExpandedBefore: boolean;
  private throttle: Common.Throttler.Throttler;
  private needsUpdateOnExpand = true;
  private hiddenIssuesMenu: Components.HideIssuesMenu.HideIssuesMenu;
  private contentCreated: boolean = false;

  constructor(issue: AggregatedIssue, description: IssuesManager.MarkdownIssueDescription.IssueDescription) {
    super();
    this._issue = issue;
    this._description = description;
    this.throttle = new Common.Throttler.Throttler(250);

    this.toggleOnClick = true;
    this.listItemElement.classList.add('issue');
    this.childrenListElement.classList.add('body');
    this.childrenListElement.classList.add(IssueView.getBodyCSSClass(this._issue.getKind()));

    this.affectedResources = this._createAffectedResources();
    this._affectedResourceViews = [
      new AffectedCookiesView(this, this._issue),
      new AffectedElementsView(this, this._issue),
      new AffectedRequestsView(this, this._issue),
      new AffectedMixedContentView(this, this._issue),
      new AffectedSourcesView(this, this._issue),
      new AffectedHeavyAdView(this, this._issue),
      new AffectedDirectivesView(this, this._issue),
      new AffectedBlockedByResponseView(this, this._issue),
      new AffectedSharedArrayBufferIssueDetailsView(this, this._issue),
      new AffectedElementsWithLowContrastView(this, this._issue),
      new AffectedTrustedWebActivityIssueDetailsView(this, this._issue),
      new CorsIssueDetailsView(this, this._issue),
      new AffectedDocumentsInQuirksModeView(this, this._issue),
      new AttributionReportingIssueDetailsView(this, this._issue),
      new WasmCrossOriginModuleSharingAffectedResourcesView(this, this._issue),
      new AffectedRawCookieLinesView(this, this._issue),
    ];

    this.hiddenIssuesMenu = new Components.HideIssuesMenu.HideIssuesMenu();
    this._aggregatedIssuesCount = null;
    this._hasBeenExpandedBefore = false;
  }

  private static getBodyCSSClass(issueKind: IssuesManager.Issue.IssueKind): string {
    switch (issueKind) {
      case IssuesManager.Issue.IssueKind.BreakingChange:
        return 'issue-kind-breaking-change';
      case IssuesManager.Issue.IssueKind.PageError:
        return 'issue-kind-page-error';
      case IssuesManager.Issue.IssueKind.Improvement:
        return 'issue-kind-improvement';
    }
  }

  getIssueTitle(): string {
    return this._description.title;
  }

  onattach(): void {
    if (!this.contentCreated) {
      this.createContent();
      return;
    }
    this.update();
  }

  createContent(): void {
    this._appendHeader();
    this._createBody();
    this.appendChild(this.affectedResources);
    for (const view of this._affectedResourceViews) {
      this.appendAffectedResource(view);
      view.update();
    }

    this._createReadMoreLinks();
    this.updateAffectedResourceVisibility();
    this.contentCreated = true;
  }

  appendAffectedResource(resource: UI.TreeOutline.TreeElement): void {
    this.affectedResources.appendChild(resource);
  }

  _appendHeader(): void {
    const header = document.createElement('div');
    if (Root.Runtime.experiments.isEnabled('hideIssuesFeature')) {
      header.addEventListener('mouseenter', this.showHiddenIssuesMenu.bind(this));
      header.addEventListener('mouseleave', this.hideHiddenIssuesMenu.bind(this));
    }
    header.classList.add('header');
    this.issueKindIcon = new IconButton.Icon.Icon();
    const kind = this._issue.getKind();
    this.issueKindIcon.data = IssueCounter.IssueCounter.getIssueKindIconData(kind);
    this.issueKindIcon.classList.add('leading-issue-icon');
    this._aggregatedIssuesCount = document.createElement('span');
    const countAdorner = new Adorners.Adorner.Adorner();
    countAdorner.data = {
      name: 'countWrapper',
      content: this._aggregatedIssuesCount,
    };
    countAdorner.classList.add('aggregated-issues-count');
    this._aggregatedIssuesCount.textContent = `${this._issue.getAggregatedIssuesCount()}`;
    header.appendChild(this.issueKindIcon);
    UI.Tooltip.Tooltip.install(this.issueKindIcon, IssuesManager.Issue.getIssueKindDescription(kind));
    header.appendChild(countAdorner);

    const title = document.createElement('div');
    title.classList.add('title');
    title.textContent = this._description.title;
    header.appendChild(title);
    if (Root.Runtime.experiments.isEnabled('hideIssuesFeature')) {
      header.appendChild(this.hiddenIssuesMenu);
      const data: HiddenIssuesMenuData = {
        issueCode: this._issue.code(),
        forHiddenIssue: this._issue.isHidden(),
      };
      this.hiddenIssuesMenu.data = data;
    }
    this.listItemElement.appendChild(header);
  }
  private showHiddenIssuesMenu(): void {
    this.hiddenIssuesMenu?.setVisible(true);
  }

  private hideHiddenIssuesMenu(): void {
    this.hiddenIssuesMenu?.setVisible(false);
  }

  onexpand(): void {
    Host.userMetrics.issuesPanelIssueExpanded(this._issue.getCategory());

    if (this.needsUpdateOnExpand) {
      this.doUpdate();
    }

    if (!this._hasBeenExpandedBefore) {
      this._hasBeenExpandedBefore = true;
      for (const view of this._affectedResourceViews) {
        view.expandIfOneResource();
      }
    }
  }

  private updateFromIssue(): void {
    if (this.issueKindIcon) {
      const kind = this._issue.getKind();
      this.issueKindIcon.data = IssueCounter.IssueCounter.getIssueKindIconData(kind);
    }
    if (this._aggregatedIssuesCount) {
      this._aggregatedIssuesCount.textContent = `${this._issue.getAggregatedIssuesCount()}`;
    }
  }

  updateAffectedResourceVisibility(): void {
    const noResources = this._affectedResourceViews.every(view => view.isEmpty());
    this.affectedResources.hidden = noResources;
  }

  _createAffectedResources(): UI.TreeOutline.TreeElement {
    const wrapper = new UI.TreeOutline.TreeElement();
    wrapper.setCollapsible(false);
    wrapper.setExpandable(true);
    wrapper.expand();
    wrapper.selectable = false;
    wrapper.listItemElement.classList.add('affected-resources-label');
    wrapper.listItemElement.textContent = i18nString(UIStrings.affectedResources);
    wrapper.childrenListElement.classList.add('affected-resources');
    return wrapper;
  }

  _createBody(): void {
    const messageElement = new UI.TreeOutline.TreeElement();
    messageElement.setCollapsible(false);
    messageElement.selectable = false;
    const markdownComponent = new MarkdownView.MarkdownView.MarkdownView();
    markdownComponent.data = {tokens: this._description.markdown};
    messageElement.listItemElement.appendChild(markdownComponent);
    this.appendChild(messageElement);
  }

  _createReadMoreLinks(): void {
    if (this._description.links.length === 0) {
      return;
    }

    const linkWrapper = new UI.TreeOutline.TreeElement();
    linkWrapper.setCollapsible(false);
    linkWrapper.listItemElement.classList.add('link-wrapper');

    const linkList = linkWrapper.listItemElement.createChild('ul', 'link-list');
    for (const description of this._description.links) {
      const link = UI.Fragment.html`<x-link class="link devtools-link" tabindex="0" href=${description.link}>${
                       i18nString(UIStrings.learnMoreS, {PH1: description.linkTitle})}</x-link>` as UI.XLink.XLink;
      const linkIcon = new IconButton.Icon.Icon();
      linkIcon.data = {iconName: 'link_icon', color: 'var(--color-link)', width: '16px', height: '16px'};
      linkIcon.classList.add('link-icon');
      link.prepend(linkIcon);
      link.addEventListener('x-link-invoke', () => {
        Host.userMetrics.issuesPanelResourceOpened(this._issue.getCategory(), AffectedItem.LearnMore);
      });

      const linkListItem = linkList.createChild('li');
      linkListItem.appendChild(link);
    }
    this.appendChild(linkWrapper);
  }

  private doUpdate(): void {
    if (this.expanded) {
      this._affectedResourceViews.forEach(view => view.update());
      this.updateAffectedResourceVisibility();
    }
    const data: HiddenIssuesMenuData = {
      issueCode: this._issue.code(),
      forHiddenIssue: this._issue.isHidden(),
    };
    this.listItemElement.classList.toggle('hidden-issue', this._issue.isHidden());
    this.hiddenIssuesMenu.data = data;
    this.needsUpdateOnExpand = !this.expanded;
    this.updateFromIssue();
  }

  update(): void {
    this.throttle.schedule(async () => this.doUpdate());
  }

  isForHiddenIssue(): boolean {
    return this._issue.isHidden();
  }

  toggle(expand?: boolean): void {
    if (expand || (expand === undefined && !this.expanded)) {
      this.expand();
    } else {
      this.collapse();
    }
  }
}
