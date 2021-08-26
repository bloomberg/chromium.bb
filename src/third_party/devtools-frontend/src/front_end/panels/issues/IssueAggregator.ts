// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as IssuesManager from '../../models/issues_manager/issues_manager.js';
import type * as Protocol from '../../generated/protocol.js';

/**
 * An `AggregatedIssue` representes a number of `IssuesManager.Issue.Issue` objects that are displayed together.
 * Currently only grouping by issue code, is supported. The class provides helpers to support displaying
 * of all resources that are affected by the aggregated issues.
 */
export class AggregatedIssue extends IssuesManager.Issue.Issue {
  private affectedCookies = new Map<string, {
    cookie: Protocol.Audits.AffectedCookie,
    hasRequest: boolean,
  }>();
  private affectedRawCookieLines = new Map<string, {rawCookieLine: string, hasRequest: boolean}>();
  private affectedRequests = new Map<string, Protocol.Audits.AffectedRequest>();
  private affectedLocations = new Map<string, Protocol.Audits.SourceCodeLocation>();
  private heavyAdIssues = new Set<IssuesManager.HeavyAdIssue.HeavyAdIssue>();
  private blockedByResponseDetails = new Map<string, Protocol.Audits.BlockedByResponseIssueDetails>();
  private corsIssues = new Set<IssuesManager.CorsIssue.CorsIssue>();
  private cspIssues = new Set<IssuesManager.ContentSecurityPolicyIssue.ContentSecurityPolicyIssue>();
  private issueKind = IssuesManager.Issue.IssueKind.Improvement;
  private lowContrastIssues = new Set<IssuesManager.LowTextContrastIssue.LowTextContrastIssue>();
  private mixedContentIssues = new Set<IssuesManager.MixedContentIssue.MixedContentIssue>();
  private sharedArrayBufferIssues = new Set<IssuesManager.SharedArrayBufferIssue.SharedArrayBufferIssue>();
  private trustedWebActivityIssues = new Set<IssuesManager.TrustedWebActivityIssue.TrustedWebActivityIssue>();
  private quirksModeIssues = new Set<IssuesManager.QuirksModeIssue.QuirksModeIssue>();
  private attributionReportingIssues = new Set<IssuesManager.AttributionReportingIssue.AttributionReportingIssue>();
  private wasmCrossOriginModuleSharingIssues =
      new Set<IssuesManager.WasmCrossOriginModuleSharingIssue.WasmCrossOriginModuleSharingIssue>();
  private representative?: IssuesManager.Issue.Issue;
  private aggregatedIssuesCount = 0;

  primaryKey(): string {
    throw new Error('This should never be called');
  }

  getBlockedByResponseDetails(): Iterable<Protocol.Audits.BlockedByResponseIssueDetails> {
    return this.blockedByResponseDetails.values();
  }

  cookies(): Iterable<Protocol.Audits.AffectedCookie> {
    return Array.from(this.affectedCookies.values()).map(x => x.cookie);
  }

  getRawCookieLines(): Iterable<{rawCookieLine: string, hasRequest: boolean}> {
    return this.affectedRawCookieLines.values();
  }

  sources(): Iterable<Protocol.Audits.SourceCodeLocation> {
    return this.affectedLocations.values();
  }

  cookiesWithRequestIndicator(): Iterable<{
    cookie: Protocol.Audits.AffectedCookie,
    hasRequest: boolean,
  }> {
    return this.affectedCookies.values();
  }

  getHeavyAdIssues(): Iterable<IssuesManager.HeavyAdIssue.HeavyAdIssue> {
    return this.heavyAdIssues;
  }

  getMixedContentIssues(): Iterable<IssuesManager.MixedContentIssue.MixedContentIssue> {
    return this.mixedContentIssues;
  }

  getTrustedWebActivityIssues(): Iterable<IssuesManager.TrustedWebActivityIssue.TrustedWebActivityIssue> {
    return this.trustedWebActivityIssues;
  }

  getCorsIssues(): Set<IssuesManager.CorsIssue.CorsIssue> {
    return this.corsIssues;
  }

  getCspIssues(): Iterable<IssuesManager.ContentSecurityPolicyIssue.ContentSecurityPolicyIssue> {
    return this.cspIssues;
  }

  getLowContrastIssues(): Iterable<IssuesManager.LowTextContrastIssue.LowTextContrastIssue> {
    return this.lowContrastIssues;
  }

  requests(): Iterable<Protocol.Audits.AffectedRequest> {
    return this.affectedRequests.values();
  }

  getSharedArrayBufferIssues(): Iterable<IssuesManager.SharedArrayBufferIssue.SharedArrayBufferIssue> {
    return this.sharedArrayBufferIssues;
  }

  getQuirksModeIssues(): Iterable<IssuesManager.QuirksModeIssue.QuirksModeIssue> {
    return this.quirksModeIssues;
  }

  getAttributionReportingIssues(): ReadonlySet<IssuesManager.AttributionReportingIssue.AttributionReportingIssue> {
    return this.attributionReportingIssues;
  }

  getWasmCrossOriginModuleSharingIssue():
      ReadonlySet<IssuesManager.WasmCrossOriginModuleSharingIssue.WasmCrossOriginModuleSharingIssue> {
    return this.wasmCrossOriginModuleSharingIssues;
  }

  getDescription(): IssuesManager.MarkdownIssueDescription.MarkdownIssueDescription|null {
    if (this.representative) {
      return this.representative.getDescription();
    }
    return null;
  }

  getCategory(): IssuesManager.Issue.IssueCategory {
    if (this.representative) {
      return this.representative.getCategory();
    }
    return IssuesManager.Issue.IssueCategory.Other;
  }

  getAggregatedIssuesCount(): number {
    return this.aggregatedIssuesCount;
  }

  /**
   * Produces a primary key for a cookie. Use this instead of `JSON.stringify` in
   * case new fields are added to `AffectedCookie`.
   */
  private keyForCookie(cookie: Protocol.Audits.AffectedCookie): string {
    const {domain, path, name} = cookie;
    return `${domain};${path};${name}`;
  }

  addInstance(issue: IssuesManager.Issue.Issue): void {
    this.aggregatedIssuesCount++;
    if (!this.representative) {
      this.representative = issue;
    }
    this.issueKind = IssuesManager.Issue.unionIssueKind(this.issueKind, issue.getKind());
    let hasRequest = false;
    for (const request of issue.requests()) {
      hasRequest = true;
      if (!this.affectedRequests.has(request.requestId)) {
        this.affectedRequests.set(request.requestId, request);
      }
    }
    for (const cookie of issue.cookies()) {
      const key = this.keyForCookie(cookie);
      if (!this.affectedCookies.has(key)) {
        this.affectedCookies.set(key, {cookie, hasRequest});
      }
    }
    for (const rawCookieLine of issue.rawCookieLines()) {
      if (!this.affectedRawCookieLines.has(rawCookieLine)) {
        this.affectedRawCookieLines.set(rawCookieLine, {rawCookieLine, hasRequest});
      }
    }
    for (const location of issue.sources()) {
      const key = JSON.stringify(location);
      if (!this.affectedLocations.has(key)) {
        this.affectedLocations.set(key, location);
      }
    }
    if (issue instanceof IssuesManager.MixedContentIssue.MixedContentIssue) {
      this.mixedContentIssues.add(issue);
    }
    if (issue instanceof IssuesManager.HeavyAdIssue.HeavyAdIssue) {
      this.heavyAdIssues.add(issue);
    }
    for (const details of issue.getBlockedByResponseDetails()) {
      const key = JSON.stringify(details, ['parentFrame', 'blockedFrame', 'requestId', 'frameId', 'reason', 'request']);
      this.blockedByResponseDetails.set(key, details);
    }
    if (issue instanceof IssuesManager.TrustedWebActivityIssue.TrustedWebActivityIssue) {
      this.trustedWebActivityIssues.add(issue);
    }
    if (issue instanceof IssuesManager.ContentSecurityPolicyIssue.ContentSecurityPolicyIssue) {
      this.cspIssues.add(issue);
    }
    if (issue instanceof IssuesManager.SharedArrayBufferIssue.SharedArrayBufferIssue) {
      this.sharedArrayBufferIssues.add(issue);
    }
    if (issue instanceof IssuesManager.LowTextContrastIssue.LowTextContrastIssue) {
      this.lowContrastIssues.add(issue);
    }
    if (issue instanceof IssuesManager.CorsIssue.CorsIssue) {
      this.corsIssues.add(issue);
    }
    if (issue instanceof IssuesManager.QuirksModeIssue.QuirksModeIssue) {
      this.quirksModeIssues.add(issue);
    }
    if (issue instanceof IssuesManager.AttributionReportingIssue.AttributionReportingIssue) {
      this.attributionReportingIssues.add(issue);
    }
    if (issue instanceof IssuesManager.WasmCrossOriginModuleSharingIssue.WasmCrossOriginModuleSharingIssue) {
      this.wasmCrossOriginModuleSharingIssues.add(issue);
    }
  }

  getKind(): IssuesManager.Issue.IssueKind {
    return this.issueKind;
  }

  isHidden(): boolean {
    return this.representative?.isHidden() || false;
  }

  setHidden(_value: boolean): void {
    throw new Error('Should not call setHidden on aggregatedIssue');
  }
}

export class IssueAggregator extends Common.ObjectWrapper.ObjectWrapper<EventTypes> {
  private readonly aggregatedIssuesByCode = new Map<string, AggregatedIssue>();
  private readonly hiddenAggregatedIssuesByCode = new Map<string, AggregatedIssue>();
  constructor(private readonly issuesManager: IssuesManager.IssuesManager.IssuesManager) {
    super();
    this.issuesManager.addEventListener(IssuesManager.IssuesManager.Events.IssueAdded, this.onIssueAdded, this);
    this.issuesManager.addEventListener(
        IssuesManager.IssuesManager.Events.FullUpdateRequired, this.onFullUpdateRequired, this);
    for (const issue of this.issuesManager.issues()) {
      this.aggregateIssue(issue);
    }
  }

  private onIssueAdded(event: Common.EventTarget.EventTargetEvent<IssuesManager.IssuesManager.IssueAddedEvent>): void {
    this.aggregateIssue(event.data.issue);
  }

  private onFullUpdateRequired(): void {
    this.aggregatedIssuesByCode.clear();
    this.hiddenAggregatedIssuesByCode.clear();
    for (const issue of this.issuesManager.issues()) {
      this.aggregateIssue(issue);
    }
    this.dispatchEventToListeners(Events.FullUpdateRequired);
  }

  private aggregateIssue(issue: IssuesManager.Issue.Issue): AggregatedIssue {
    const map = issue.isHidden() ? this.hiddenAggregatedIssuesByCode : this.aggregatedIssuesByCode;
    const aggregatedIssue = this.aggregateIssueByStatus(map, issue);
    this.dispatchEventToListeners(Events.AggregatedIssueUpdated, aggregatedIssue);
    return aggregatedIssue;
  }

  private aggregateIssueByStatus(aggregatedIssuesMap: Map<string, AggregatedIssue>, issue: IssuesManager.Issue.Issue):
      AggregatedIssue {
    let aggregatedIssue = aggregatedIssuesMap.get(issue.code());
    if (!aggregatedIssue) {
      aggregatedIssue = new AggregatedIssue(issue.code());
      aggregatedIssuesMap.set(issue.code(), aggregatedIssue);
    }
    aggregatedIssue.addInstance(issue);
    return aggregatedIssue;
  }

  aggregatedIssues(): Iterable<AggregatedIssue> {
    return this.aggregatedIssuesByCode.values();
  }

  hiddenAggregatedIssues(): Iterable<AggregatedIssue> {
    return this.hiddenAggregatedIssuesByCode.values();
  }

  aggregatedIssueCodes(): Set<string> {
    return new Set(this.aggregatedIssuesByCode.keys());
  }

  aggregatedIssueCategories(): Set<IssuesManager.Issue.IssueCategory> {
    const result = new Set<IssuesManager.Issue.IssueCategory>();
    for (const issue of this.aggregatedIssuesByCode.values()) {
      result.add(issue.getCategory());
    }
    return result;
  }

  numberOfAggregatedIssues(): number {
    return this.aggregatedIssuesByCode.size;
  }

  numberOfHiddenAggregatedIssues(): number {
    return this.hiddenAggregatedIssuesByCode.size;
  }
}

export const enum Events {
  AggregatedIssueUpdated = 'AggregatedIssueUpdated',
  FullUpdateRequired = 'FullUpdateRequired',
}

export type EventTypes = {
  [Events.AggregatedIssueUpdated]: AggregatedIssue,
  [Events.FullUpdateRequired]: void,
};
