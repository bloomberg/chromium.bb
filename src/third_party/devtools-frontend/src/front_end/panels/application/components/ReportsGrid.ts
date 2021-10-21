// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../../../core/i18n/i18n.js';
import * as DataGrid from '../../../ui/components/data_grid/data_grid.js';
import * as ComponentHelpers from '../../../ui/components/helpers/helpers.js';
import * as LitHtml from '../../../ui/lit-html/lit-html.js';

import type * as Protocol from '../../../generated/protocol.js';
import * as Root from '../../../core/root/root.js';

import reportingApiGridStyles from './reportingApiGrid.css.js';

const UIStrings = {
  /**
  *@description Placeholder text when there are no Reporting API reports.
  *(https://developers.google.com/web/updates/2018/09/reportingapi#sending)
  */
  noReportsToDisplay: 'No reports to display',
};
const str_ = i18n.i18n.registerUIStrings('panels/application/components/ReportsGrid.ts', UIStrings);
export const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

const {render, html} = LitHtml;

export interface ReportsGridData {
  reports: Protocol.Network.ReportingApiReport[];
}

export class ReportsGrid extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-resources-reports-grid`;

  private readonly shadow = this.attachShadow({mode: 'open'});
  private reports: Protocol.Network.ReportingApiReport[] = [];
  private protocolMonitorExperimentEnabled = false;

  connectedCallback(): void {
    this.shadow.adoptedStyleSheets = [reportingApiGridStyles];
    this.protocolMonitorExperimentEnabled = Root.Runtime.experiments.isEnabled('protocolMonitor');
    this.render();
  }

  set data(data: ReportsGridData) {
    this.reports = data.reports;
    this.render();
  }

  private render(): void {
    const reportsGridData: DataGrid.DataGridController.DataGridControllerData = {
      columns: [
        {
          id: 'url',
          title: i18n.i18n.lockedString('URL'),
          widthWeighting: 30,
          hideable: false,
          visible: true,
        },
        {
          id: 'type',
          title: i18n.i18n.lockedString('Type'),
          widthWeighting: 10,
          hideable: false,
          visible: true,
        },
        {
          id: 'status',
          title: i18n.i18n.lockedString('Status'),
          widthWeighting: 10,
          hideable: false,
          visible: true,
        },
        {
          id: 'destination',
          title: i18n.i18n.lockedString('Destination'),
          widthWeighting: 10,
          hideable: false,
          visible: true,
        },
        {
          id: 'timestamp',
          title: i18n.i18n.lockedString('Timestamp'),
          widthWeighting: 20,
          hideable: false,
          visible: true,
        },
        {
          id: 'body',
          title: i18n.i18n.lockedString('Body'),
          widthWeighting: 20,
          hideable: false,
          visible: true,
        },
      ],
      rows: this.buildReportRows(),
    };

    if (this.protocolMonitorExperimentEnabled) {
      reportsGridData.columns.unshift(
          {id: 'id', title: 'ID', widthWeighting: 30, hideable: false, visible: true},
      );
    }

    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    render(html`
      <div class="reporting-container">
        <div class="reporting-header">Reports</div>
        ${this.reports.length > 0 ? html`
          <${DataGrid.DataGridController.DataGridController.litTagName} .data=${
              reportsGridData as DataGrid.DataGridController.DataGridControllerData}>
          </${DataGrid.DataGridController.DataGridController.litTagName}>
        ` : html`
          <div class="reporting-placeholder">
            <div>${i18nString(UIStrings.noReportsToDisplay)}</div>
          </div>
        `}
      </div>
    `, this.shadow);
    // clang-format on
  }

  private buildReportRows(): DataGrid.DataGridUtils.Row[] {
    return this.reports.map(report => ({
                              cells: [
                                {columnId: 'id', value: report.id},
                                {columnId: 'url', value: report.initiatorUrl},
                                {columnId: 'type', value: report.type},
                                {columnId: 'status', value: report.status},
                                {columnId: 'destination', value: report.destination},
                                {columnId: 'timestamp', value: new Date(report.timestamp * 1000).toLocaleString()},
                                {columnId: 'body', value: JSON.stringify(report.body)},
                              ],
                            }));
  }
}

ComponentHelpers.CustomElements.defineComponent('devtools-resources-reports-grid', ReportsGrid);

declare global {
  interface HTMLElementTagNameMap {
    'devtools-resources-reports-grid': ReportsGrid;
  }
}
