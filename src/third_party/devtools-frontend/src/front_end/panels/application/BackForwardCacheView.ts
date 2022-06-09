// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Platform from '../../core/platform/platform.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Buttons from '../../ui/components/buttons/buttons.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as LitHtml from '../../ui/lit-html/lit-html.js';
import * as ReportView from '../../ui/components/report_view/report_view.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Protocol from '../../generated/protocol.js';
import * as IconButton from '../../ui/components/icon_button/icon_button.js';

import {NotRestoredReasonDescription} from './BackForwardCacheStrings.js';
import backForwardCacheViewStyles from './backForwardCacheView.css.js';

const UIStrings = {
  /**
   * @description Title text in back/forward cache view of the Application panel
   */
  mainFrame: 'Main Frame',
  /**
   * @description Title text in back/forward cache view of the Application panel
   */
  backForwardCacheTitle: 'Back/forward cache',
  /**
   * @description Status text for the status of the main frame
   */
  unavailable: 'unavailable',
  /**
   * @description Entry name text in the back/forward cache view of the Application panel
   */
  url: 'URL:',
  /**
   * @description Status text for the status of the back/forward cache status
   */
  unknown: 'Unknown Status',
  /**
   * @description Status text for the status of the back/forward cache status indicating that
   * the back/forward cache was not used and a normal navigation occured instead.
   */
  normalNavigation:
      'Not served from back/forward cache: to trigger back/forward cache, use Chrome\'s back/forward buttons, or use the test button below to automatically navigate away and back.',
  /**
   * @description Status text for the status of the back/forward cache status indicating that
   * the back/forward cache was used to restore the page instead of reloading it.
   */
  restoredFromBFCache: 'Successfully served from back/forward cache.',
  /**
   * @description Label for a list of reasons which prevent the page from being eligible for
   * back/forward cache. These reasons are actionable i.e. they can be cleaned up to make the
   * page eligible for back/forward cache.
   */
  pageSupportNeeded: 'Actionable',
  /**
   * @description Explanation for actionable items which prevent the page from being eligible
   * for back/forward cache.
   */
  pageSupportNeededExplanation:
      'These reasons are actionable i.e. they can be cleaned up to make the page eligible for back/forward cache.',
  /**
   * @description Label for a list of reasons which prevent the page from being eligible for
   * back/forward cache. These reasons are circumstantial / not actionable i.e. they cannot be
   * cleaned up by developers to make the page eligible for back/forward cache.
   */
  circumstantial: 'Not Actionable',
  /**
   * @description Explanation for circumstantial/non-actionable items which prevent the page from being eligible
   * for back/forward cache.
   */
  circumstantialExplanation:
      'These reasons are not actionable i.e. caching was prevented by something outside of the direct control of the page.',
  /**
   * @description Label for a list of reasons which prevent the page from being eligible for
   * back/forward cache. These reasons are pending support by chrome i.e. in a future version
   * of chrome they will not prevent back/forward cache usage anymore.
   */
  supportPending: 'Pending Support',
  /**
   * @description Label for the button to test whether BFCache is available for the page
   */
  runTest: 'Test back/forward cache',
  /**
   * @description Label for the disabled button while the test is running
   */
  runningTest: 'Running test',
  /**
   * @description Link Text about explanation of back/forward cache
   */
  learnMore: 'Learn more: back/forward cache eligibility',
  /**
   * @description Explanation for 'pending support' items which prevent the page from being eligible
   * for back/forward cache.
   */
  supportPendingExplanation:
      'Chrome support for these reasons is pending i.e. they will not prevent the page from being eligible for back/forward cache in a future version of Chrome.',
};
const str_ = i18n.i18n.registerUIStrings('panels/application/BackForwardCacheView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const enum ScreenStatusType {
  Running = 'Running',
  Result = 'Result',
}

export class BackForwardCacheView extends UI.ThrottledWidget.ThrottledWidget {
  constructor() {
    super(true, 1000);
    this.getMainResourceTreeModel()?.addEventListener(
        SDK.ResourceTreeModel.Events.MainFrameNavigated, this.onBackForwardCacheUpdate, this);
    this.getMainResourceTreeModel()?.addEventListener(
        SDK.ResourceTreeModel.Events.BackForwardCacheDetailsUpdated, this.onBackForwardCacheUpdate, this);
    this.update();
    this.screenStatus = ScreenStatusType.Result;
  }

  private screenStatus: ScreenStatusType;

  wasShown(): void {
    super.wasShown();
    this.registerCSSFiles([backForwardCacheViewStyles]);
  }

  private onBackForwardCacheUpdate(): void {
    this.update();
  }

  async doUpdate(): Promise<void> {
    const data = {reportTitle: i18nString(UIStrings.backForwardCacheTitle)};
    const html = LitHtml.html`
      <${ReportView.ReportView.Report.litTagName} .data=${data as ReportView.ReportView.ReportData}>
        ${this.renderMainFrameInformation(this.getMainFrame())}
      </${ReportView.ReportView.Report.litTagName}>
    `;
    LitHtml.render(html, this.contentElement, {host: this});
  }

  private getMainResourceTreeModel(): SDK.ResourceTreeModel.ResourceTreeModel|null {
    const mainTarget = SDK.TargetManager.TargetManager.instance().mainTarget();
    return mainTarget?.model(SDK.ResourceTreeModel.ResourceTreeModel) || null;
  }

  private getMainFrame(): SDK.ResourceTreeModel.ResourceTreeFrame|null {
    return this.getMainResourceTreeModel()?.mainFrame || null;
  }

  private renderBackForwardCacheTestResult(): void {
    SDK.TargetManager.TargetManager.instance().removeModelListener(
        SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated,
        this.renderBackForwardCacheTestResult, this);
    this.screenStatus = ScreenStatusType.Result;
    this.update();
  }

  private async goBackOneHistoryEntry(): Promise<void> {
    SDK.TargetManager.TargetManager.instance().removeModelListener(
        SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated,
        this.goBackOneHistoryEntry, this);
    this.screenStatus = ScreenStatusType.Running;
    this.update();
    const mainTarget = SDK.TargetManager.TargetManager.instance().mainTarget();
    if (!mainTarget) {
      return;
    }
    const resourceTreeModel = mainTarget.model(SDK.ResourceTreeModel.ResourceTreeModel);
    if (!resourceTreeModel) {
      return;
    }
    const historyResults = await resourceTreeModel.navigationHistory();
    if (!historyResults) {
      return;
    }
    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated,
        this.renderBackForwardCacheTestResult, this);
    resourceTreeModel.navigateToHistoryEntry(historyResults.entries[historyResults.currentIndex - 1]);
  }

  private async navigateAwayAndBack(): Promise<void> {
    // Checking BFCache Compatibility

    const mainTarget = SDK.TargetManager.TargetManager.instance().mainTarget();
    if (!mainTarget) {
      return;
    }
    const resourceTreeModel = mainTarget.model(SDK.ResourceTreeModel.ResourceTreeModel);

    if (resourceTreeModel) {
      // This event is removed by inside of goBackOneHistoryEntry().
      SDK.TargetManager.TargetManager.instance().addModelListener(
          SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated,
          this.goBackOneHistoryEntry, this);

      // We can know whether the current page can use BFCache
      // as the browser navigates to another unrelated page and goes back to the current page.
      // We chose "chrome://terms" because it must be cross-site.
      // Ideally, We want to have our own testing page like "chrome: //bfcache-test".
      resourceTreeModel.navigate('chrome://terms');
    }
  }

  private renderMainFrameInformation(mainFrame: SDK.ResourceTreeModel.ResourceTreeFrame|null): LitHtml.TemplateResult {
    if (!mainFrame) {
      // clang-format off
      return LitHtml.html`
        <${ReportView.ReportView.ReportKey.litTagName}>
          ${i18nString(UIStrings.mainFrame)}
        </${ReportView.ReportView.ReportKey.litTagName}>
        <${ReportView.ReportView.ReportValue.litTagName}>
          ${i18nString(UIStrings.unavailable)}
        </${ReportView.ReportView.ReportValue.litTagName}>
      `;
      // clang-format on
    }
    const isDisabled = this.screenStatus === ScreenStatusType.Running;
    // clang-format off
    return LitHtml.html`
      ${this.renderBackForwardCacheStatus(mainFrame.backForwardCacheDetails.restoredFromCache)}
      <div class='url'>
        <div class='url-key'>
          ${i18nString(UIStrings.url)}
        </div>
        <div class='url-value'>
          ${mainFrame.url}
        </div>
      </div>
      <${ReportView.ReportView.ReportSection.litTagName}>
        <${Buttons.Button.Button.litTagName}
          .disabled=${isDisabled}
          .spinner=${isDisabled}
          .variant=${Buttons.Button.Variant.PRIMARY}
          @click=${this.navigateAwayAndBack}>
          ${isDisabled ? LitHtml.html`
            ${i18nString(UIStrings.runningTest)}`:`
            ${i18nString(UIStrings.runTest)}
          `}
        </${Buttons.Button.Button.litTagName}>
      </${ReportView.ReportView.ReportSection.litTagName}>
      <${ReportView.ReportView.ReportSectionDivider.litTagName}>
      </${ReportView.ReportView.ReportSectionDivider.litTagName}>
        ${this.maybeRenderExplanations(mainFrame.backForwardCacheDetails.explanations)}
      <${ReportView.ReportView.ReportSection.litTagName}>
        <x-link href="https://web.dev/bfcache/" class="link">
          ${i18nString(UIStrings.learnMore)}
        </x-link>
      </${ReportView.ReportView.ReportSection.litTagName}>
    `;
    // clang-format on
  }

  private renderBackForwardCacheStatus(status: boolean|undefined): LitHtml.TemplateResult {
    switch (status) {
      case true:
        // clang-format off
        return LitHtml.html`
          <${ReportView.ReportView.ReportSection.litTagName}>
            <div class='status'>
              <${IconButton.Icon.Icon.litTagName} class="inline-icon" .data=${{
                iconName: 'ic_checkmark_16x16',
                color: 'green',
                width: '16px',
                height: '16px',
                } as IconButton.Icon.IconData}>
              </${IconButton.Icon.Icon.litTagName}>
            </div>
            ${i18nString(UIStrings.restoredFromBFCache)}
          </${ReportView.ReportView.ReportSection.litTagName}>
        `;
        // clang-format on
      case false:
        // clang-format off
        return LitHtml.html`
          <${ReportView.ReportView.ReportSection.litTagName}>
            <div class='status'>
              <${IconButton.Icon.Icon.litTagName} class="inline-icon" .data=${{
                  iconName: 'circled_backslash_icon',
                  color: 'var(--color-text-secondary)',
                  width: '16px',
                  height: '16px',
                  } as IconButton.Icon.IconData}>
              </${IconButton.Icon.Icon.litTagName}>
            </div>
            ${i18nString(UIStrings.normalNavigation)}
          </${ReportView.ReportView.ReportSection.litTagName}>
        `;
        // clang-format on
    }
    // clang-format off
    return LitHtml.html`
    <${ReportView.ReportView.ReportSection.litTagName}>
      ${i18nString(UIStrings.unknown)}
    </${ReportView.ReportView.ReportSection.litTagName}>
    `;
    // clang-format on
  }

  private maybeRenderExplanations(explanations: Protocol.Page.BackForwardCacheNotRestoredExplanation[]):
      LitHtml.TemplateResult|{} {
    if (explanations.length === 0) {
      return LitHtml.nothing;
    }

    const pageSupportNeeded = explanations.filter(
        explanation => explanation.type === Protocol.Page.BackForwardCacheNotRestoredReasonType.PageSupportNeeded);
    const supportPending = explanations.filter(
        explanation => explanation.type === Protocol.Page.BackForwardCacheNotRestoredReasonType.SupportPending);
    const circumstantial = explanations.filter(
        explanation => explanation.type === Protocol.Page.BackForwardCacheNotRestoredReasonType.Circumstantial);

    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    return LitHtml.html`
      ${this.renderExplanations(i18nString(UIStrings.pageSupportNeeded), i18nString(UIStrings.pageSupportNeededExplanation), pageSupportNeeded)}
      ${this.renderExplanations(i18nString(UIStrings.supportPending), i18nString(UIStrings.supportPendingExplanation), supportPending)}
      ${this.renderExplanations(i18nString(UIStrings.circumstantial), i18nString(UIStrings.circumstantialExplanation), circumstantial)}
    `;
    // clang-format on
  }

  private renderExplanations(
      category: Platform.UIString.LocalizedString, explainerText: Platform.UIString.LocalizedString,
      explanations: Protocol.Page.BackForwardCacheNotRestoredExplanation[]): LitHtml.TemplateResult {
    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    return LitHtml.html`
      ${explanations.length > 0 ? LitHtml.html`
        <${ReportView.ReportView.ReportSectionHeader.litTagName}>
          ${category}
          <div class='help-outline-icon'>
            <${IconButton.Icon.Icon.litTagName} class="inline-icon" .data=${{
              iconName: 'help_outline',
              color: 'var(--color-text-secondary)',
              width: '16px',
              height: '16px',
              } as IconButton.Icon.IconData} title=${explainerText}>
            </${IconButton.Icon.Icon.litTagName}>
          </div>
        </${ReportView.ReportView.ReportSectionHeader.litTagName}>
        ${explanations.map(explanation => this.renderReason(explanation))}
      ` : LitHtml.nothing}
    `;
    // clang-format on
  }

  private renderReason(explanation: Protocol.Page.BackForwardCacheNotRestoredExplanation): LitHtml.TemplateResult {
    // clang-format off
    return LitHtml.html`
      <${ReportView.ReportView.ReportSection.litTagName}>
        ${(explanation.reason in NotRestoredReasonDescription) ?
          LitHtml.html`
            <div class='circled-exclamation-icon'>
              <${IconButton.Icon.Icon.litTagName} class="inline-icon" .data=${{
                iconName: 'circled_exclamation_icon',
                color: 'orange',
                width: '16px',
                height: '16px',
              } as IconButton.Icon.IconData}>
              </${IconButton.Icon.Icon.litTagName}>
            </div>
            ${NotRestoredReasonDescription[explanation.reason].name()}` :
            LitHtml.nothing}
      </${ReportView.ReportView.ReportSection.litTagName}>
      <div class='gray-text'>
        ${explanation.reason}
      </div>
    `;
    // clang-format on
  }
}
