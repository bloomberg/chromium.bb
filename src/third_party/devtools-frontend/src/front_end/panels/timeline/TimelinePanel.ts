// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as Extensions from '../../models/extensions/extensions.js';
import * as TimelineModel from '../../models/timeline_model/timeline_model.js';
import * as PerfUI from '../../ui/legacy/components/perf_ui/perf_ui.js';
import * as UI from '../../ui/legacy/legacy.js';

import historyToolbarButtonStyles from './historyToolbarButton.css.js';
import timelinePanelStyles from './timelinePanel.css.js';
import timelineStatusDialogStyles from './timelineStatusDialog.css.js';

import type * as Coverage from '../coverage/coverage.js';
import * as MobileThrottling from '../mobile_throttling/mobile_throttling.js';

import type {WindowChangedEvent} from './PerformanceModel.js';
import {Events, PerformanceModel} from './PerformanceModel.js';
import type {Client} from './TimelineController.js';
import {TimelineController} from './TimelineController.js';
import type {TimelineEventOverview} from './TimelineEventOverview.js';
import {TimelineEventOverviewCoverage, TimelineEventOverviewCPUActivity, TimelineEventOverviewFrames, TimelineEventOverviewInput, TimelineEventOverviewMemory, TimelineEventOverviewNetwork, TimelineEventOverviewResponsiveness, TimelineFilmStripOverview} from './TimelineEventOverview.js';
import {TimelineFlameChartView} from './TimelineFlameChartView.js';
import {TimelineHistoryManager} from './TimelineHistoryManager.js';
import {TimelineLoader} from './TimelineLoader.js';
import {TimelineUIUtils} from './TimelineUIUtils.js';
import {UIDevtoolsController} from './UIDevtoolsController.js';
import {UIDevtoolsUtils} from './UIDevtoolsUtils.js';

const UIStrings = {
  /**
  *@description Text that appears when user drag and drop something (for example, a file) in Timeline Panel of the Performance panel
  */
  dropTimelineFileOrUrlHere: 'Drop timeline file or URL here',
  /**
  *@description Title of disable capture jsprofile setting in timeline panel of the performance panel
  */
  disableJavascriptSamples: 'Disable JavaScript samples',
  /**
  *@description Title of capture layers and pictures setting in timeline panel of the performance panel
  */
  enableAdvancedPaint: 'Enable advanced paint instrumentation (slow)',
  /**
  *@description Title of show screenshots setting in timeline panel of the performance panel
  */
  screenshots: 'Screenshots',
  /**
  *@description Title of the 'Coverage' tool in the bottom drawer
  */
  coverage: 'Coverage',
  /**
  *@description Text for the memory of the page
  */
  memory: 'Memory',
  /**
  *@description Text in Timeline for the Web Vitals lane
  */
  webVitals: 'Web Vitals',
  /**
  *@description Text to clear content
  */
  clear: 'Clear',
  /**
  *@description Tooltip text that appears when hovering over the largeicon load button
  */
  loadProfile: 'Load profile…',
  /**
  *@description Tooltip text that appears when hovering over the largeicon download button
  */
  saveProfile: 'Save profile…',
  /**
  *@description Text to take screenshots
  */
  captureScreenshots: 'Capture screenshots',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  showMemoryTimeline: 'Show memory timeline',
  /**
  *@description Text in Timeline for the Web Vitals lane checkbox
  */
  showWebVitals: 'Show Web Vitals',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  recordCoverageWithPerformance: 'Record coverage with performance trace',
  /**
  *@description Tooltip text that appears when hovering over the largeicon settings gear in show settings pane setting in timeline panel of the performance panel
  */
  captureSettings: 'Capture settings',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  disablesJavascriptSampling: 'Disables JavaScript sampling, reduces overhead when running against mobile devices',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  capturesAdvancedPaint: 'Captures advanced paint instrumentation, introduces significant performance overhead',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  network: 'Network:',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  cpu: 'CPU:',
  /**
  *@description Title of the 'Network conditions' tool in the bottom drawer
  */
  networkConditions: 'Network conditions',
  /**
  *@description Text in Timeline Panel of the Performance panel
  *@example {wrong format} PH1
  *@example {ERROR_FILE_NOT_FOUND} PH2
  *@example {2} PH3
  */
  failedToSaveTimelineSSS: 'Failed to save timeline: {PH1} ({PH2}, {PH3})',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  CpuThrottlingIsEnabled: '- CPU throttling is enabled',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  NetworkThrottlingIsEnabled: '- Network throttling is enabled',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  SignificantOverheadDueToPaint: '- Significant overhead due to paint instrumentation',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  JavascriptSamplingIsDisabled: '- JavaScript sampling is disabled',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  stoppingTimeline: 'Stopping timeline…',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  received: 'Received',
  /**
  *@description Text to close something
  */
  close: 'Close',
  /**
  *@description Status text to indicate the recording has failed in the Performance panel
  */
  recordingFailed: 'Recording failed',
  /**
  * @description Text to indicate the progress of a profile. Informs the user that we are currently
  * creating a peformance profile.
  */
  profiling: 'Profiling…',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  bufferUsage: 'Buffer usage',
  /**
  *@description Text for an option to learn more about something
  */
  learnmore: 'Learn more',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  wasd: 'WASD',
  /**
  *@description Text in Timeline Panel of the Performance panel
  *@example {record} PH1
  *@example {Ctrl + R} PH2
  */
  clickTheRecordButtonSOrHitSTo: 'Click the record button {PH1} or hit {PH2} to start a new recording.',
  /**
  * @description Text in Timeline Panel of the Performance panel
  * @example {reload button} PH1
  * @example {Ctrl + R} PH2
  */
  clickTheReloadButtonSOrHitSTo: 'Click the reload button {PH1} or hit {PH2} to record the page load.',
  /**
  *@description Text in Timeline Panel of the Performance panel
  *@example {Ctrl + U} PH1
  *@example {Learn more} PH2
  */
  afterRecordingSelectAnAreaOf:
      'After recording, select an area of interest in the overview by dragging. Then, zoom and pan the timeline with the mousewheel or {PH1} keys. {PH2}',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  loadingProfile: 'Loading profile…',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  processingProfile: 'Processing profile…',
  /**
  *@description Text in Timeline Panel of the Performance panel
  */
  initializingProfiler: 'Initializing profiler…',
  /**
  *@description Text for the status of something
  */
  status: 'Status',
  /**
  *@description Text that refers to the time
  */
  time: 'Time',
  /**
  *@description Text for the description of something
  */
  description: 'Description',
  /**
  *@description Text of an item that stops the running task
  */
  stop: 'Stop',
  /**
  *@description Time text content in Timeline Panel of the Performance panel
  *@example {2.12} PH1
  */
  ssec: '{PH1} sec',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/TimelinePanel.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
let timelinePanelInstance: TimelinePanel;

export class TimelinePanel extends UI.Panel.Panel implements Client, TimelineModeViewDelegate {
  private readonly dropTarget: UI.DropTarget.DropTarget;
  private readonly recordingOptionUIControls: UI.Toolbar.ToolbarItem[];
  private state: State;
  private recordingPageReload: boolean;
  private readonly millisecondsToRecordAfterLoadEvent: number;
  private readonly toggleRecordAction: UI.ActionRegistration.Action;
  private readonly recordReloadAction: UI.ActionRegistration.Action;
  private readonly historyManager: TimelineHistoryManager;
  private performanceModel: PerformanceModel|null;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly viewModeSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private disableCaptureJSProfileSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private readonly captureLayersAndPicturesSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private showScreenshotsSetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private startCoverage: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private showMemorySetting: Common.Settings.Setting<any>;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private showWebVitalsSetting: Common.Settings.Setting<any>;
  private readonly panelToolbar: UI.Toolbar.Toolbar;
  private readonly panelRightToolbar: UI.Toolbar.Toolbar;
  private readonly timelinePane: UI.Widget.VBox;
  private readonly overviewPane: PerfUI.TimelineOverviewPane.TimelineOverviewPane;
  private overviewControls: TimelineEventOverview[];
  private readonly statusPaneContainer: HTMLElement;
  private readonly flameChart: TimelineFlameChartView;
  private readonly searchableViewInternal: UI.SearchableView.SearchableView;
  private showSettingsPaneButton!: UI.Toolbar.ToolbarSettingToggle;
  private showSettingsPaneSetting!: Common.Settings.Setting<boolean>;
  private settingsPane!: UI.Widget.Widget;
  private controller!: TimelineController|null;
  private clearButton!: UI.Toolbar.ToolbarButton;
  private loadButton!: UI.Toolbar.ToolbarButton;
  private saveButton!: UI.Toolbar.ToolbarButton;
  private statusPane!: StatusPane|null;
  private landingPage!: UI.Widget.Widget;
  private loader?: TimelineLoader;
  private showScreenshotsToolbarCheckbox?: UI.Toolbar.ToolbarItem;
  private showMemoryToolbarCheckbox?: UI.Toolbar.ToolbarItem;
  private showWebVitalsToolbarCheckbox?: UI.Toolbar.ToolbarItem;
  private startCoverageCheckbox?: UI.Toolbar.ToolbarItem;
  private networkThrottlingSelect?: UI.Toolbar.ToolbarComboBox;
  private cpuThrottlingSelect?: UI.Toolbar.ToolbarComboBox;
  private fileSelectorElement?: HTMLInputElement;
  private selection?: TimelineSelection|null;
  constructor() {
    super('timeline');
    this.element.addEventListener('contextmenu', this.contextMenu.bind(this), false);
    this.dropTarget = new UI.DropTarget.DropTarget(
        this.element, [UI.DropTarget.Type.File, UI.DropTarget.Type.URI],
        i18nString(UIStrings.dropTimelineFileOrUrlHere), this.handleDrop.bind(this));

    this.recordingOptionUIControls = [];
    this.state = State.Idle;
    this.recordingPageReload = false;
    this.millisecondsToRecordAfterLoadEvent = 5000;
    this.toggleRecordAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('timeline.toggle-recording') as
         UI.ActionRegistration.Action);
    this.recordReloadAction =
        (UI.ActionRegistry.ActionRegistry.instance().action('timeline.record-reload') as UI.ActionRegistration.Action);

    this.historyManager = new TimelineHistoryManager();

    this.performanceModel = null;

    this.viewModeSetting = Common.Settings.Settings.instance().createSetting('timelineViewMode', ViewMode.FlameChart);

    this.disableCaptureJSProfileSetting =
        Common.Settings.Settings.instance().createSetting('timelineDisableJSSampling', false);
    this.disableCaptureJSProfileSetting.setTitle(i18nString(UIStrings.disableJavascriptSamples));
    this.captureLayersAndPicturesSetting =
        Common.Settings.Settings.instance().createSetting('timelineCaptureLayersAndPictures', false);
    this.captureLayersAndPicturesSetting.setTitle(i18nString(UIStrings.enableAdvancedPaint));

    this.showScreenshotsSetting = Common.Settings.Settings.instance().createSetting('timelineShowScreenshots', true);
    this.showScreenshotsSetting.setTitle(i18nString(UIStrings.screenshots));
    this.showScreenshotsSetting.addChangeListener(this.updateOverviewControls, this);

    this.startCoverage = Common.Settings.Settings.instance().createSetting('timelineStartCoverage', false);
    this.startCoverage.setTitle(i18nString(UIStrings.coverage));

    if (!Root.Runtime.experiments.isEnabled('recordCoverageWithPerformanceTracing')) {
      this.startCoverage.set(false);
    }

    this.showMemorySetting = Common.Settings.Settings.instance().createSetting('timelineShowMemory', false);
    this.showMemorySetting.setTitle(i18nString(UIStrings.memory));
    this.showMemorySetting.addChangeListener(this.onModeChanged, this);

    this.showWebVitalsSetting = Common.Settings.Settings.instance().createSetting('timelineWebVitals', false);
    this.showWebVitalsSetting.setTitle(i18nString(UIStrings.webVitals));
    this.showWebVitalsSetting.addChangeListener(this.onWebVitalsChanged, this);

    const timelineToolbarContainer = this.element.createChild('div', 'timeline-toolbar-container');
    this.panelToolbar = new UI.Toolbar.Toolbar('timeline-main-toolbar', timelineToolbarContainer);
    this.panelToolbar.makeWrappable(true);
    this.panelRightToolbar = new UI.Toolbar.Toolbar('', timelineToolbarContainer);
    this.createSettingsPane();
    this.updateShowSettingsToolbarButton();

    this.timelinePane = new UI.Widget.VBox();
    this.timelinePane.show(this.element);
    const topPaneElement = this.timelinePane.element.createChild('div', 'hbox');
    topPaneElement.id = 'timeline-overview-panel';

    // Create top overview component.
    this.overviewPane = new PerfUI.TimelineOverviewPane.TimelineOverviewPane('timeline');
    this.overviewPane.addEventListener(
        PerfUI.TimelineOverviewPane.Events.WindowChanged, this.onOverviewWindowChanged.bind(this));
    this.overviewPane.show(topPaneElement);
    this.overviewControls = [];

    this.statusPaneContainer = this.timelinePane.element.createChild('div', 'status-pane-container fill');

    this.createFileSelector();

    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.ResourceTreeModel.ResourceTreeModel, SDK.ResourceTreeModel.Events.Load, this.loadEventFired, this);

    this.flameChart = new TimelineFlameChartView(this);
    this.searchableViewInternal = new UI.SearchableView.SearchableView(this.flameChart, null);
    this.searchableViewInternal.setMinimumSize(0, 100);
    this.searchableViewInternal.element.classList.add('searchable-view');
    this.searchableViewInternal.show(this.timelinePane.element);
    this.flameChart.show(this.searchableViewInternal.element);
    this.flameChart.setSearchableView(this.searchableViewInternal);
    this.searchableViewInternal.hideWidget();

    this.onModeChanged();
    this.onWebVitalsChanged();
    this.populateToolbar();
    this.showLandingPage();
    this.updateTimelineControls();

    Extensions.ExtensionServer.ExtensionServer.instance().addEventListener(
        Extensions.ExtensionServer.Events.TraceProviderAdded, this.appendExtensionsToToolbar, this);
    SDK.TargetManager.TargetManager.instance().addEventListener(
        SDK.TargetManager.Events.SuspendStateChanged, this.onSuspendStateChanged, this);
  }

  static instance(opts: {
    forceNew: boolean|null,
  }|undefined = {forceNew: null}): TimelinePanel {
    const {forceNew} = opts;
    if (!timelinePanelInstance || forceNew) {
      timelinePanelInstance = new TimelinePanel();
    }

    return timelinePanelInstance;
  }

  searchableView(): UI.SearchableView.SearchableView|null {
    return this.searchableViewInternal;
  }

  wasShown(): void {
    super.wasShown();
    UI.Context.Context.instance().setFlavor(TimelinePanel, this);
    this.registerCSSFiles([timelinePanelStyles]);
    // Record the performance tool load time.
    Host.userMetrics.panelLoaded('timeline', 'DevTools.Launch.Timeline');
  }

  willHide(): void {
    UI.Context.Context.instance().setFlavor(TimelinePanel, null);
    this.historyManager.cancelIfShowing();
  }

  loadFromEvents(events: SDK.TracingManager.EventPayload[]): void {
    if (this.state !== State.Idle) {
      return;
    }
    this.prepareToLoadTimeline();
    this.loader = TimelineLoader.loadFromEvents(events, this);
  }

  private onOverviewWindowChanged(
      event: Common.EventTarget.EventTargetEvent<PerfUI.TimelineOverviewPane.WindowChangedEvent>): void {
    if (!this.performanceModel) {
      return;
    }
    const left = event.data.startTime;
    const right = event.data.endTime;
    this.performanceModel.setWindow({left, right}, /* animate */ true);
  }

  private onModelWindowChanged(event: Common.EventTarget.EventTargetEvent<WindowChangedEvent>): void {
    const window = event.data.window;
    this.overviewPane.setWindowTimes(window.left, window.right);
  }

  private setState(state: State): void {
    this.state = state;
    this.updateTimelineControls();
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private createSettingCheckbox(setting: Common.Settings.Setting<any>, tooltip: string): UI.Toolbar.ToolbarItem {
    const checkboxItem = new UI.Toolbar.ToolbarSettingCheckbox(setting, tooltip);
    this.recordingOptionUIControls.push(checkboxItem);
    return checkboxItem;
  }

  private populateToolbar(): void {
    // Record
    this.panelToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.toggleRecordAction));
    this.panelToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButton(this.recordReloadAction));
    this.clearButton = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.clear), 'largeicon-clear');
    this.clearButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, () => this.onClearButton());
    this.panelToolbar.appendToolbarItem(this.clearButton);

    // Load / Save
    this.loadButton = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.loadProfile), 'largeicon-load');
    this.loadButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, () => {
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.PerfPanelTraceImported);
      this.selectFileToLoad();
    });
    this.saveButton = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.saveProfile), 'largeicon-download');
    this.saveButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, _event => {
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.PerfPanelTraceExported);
      void this.saveToFile();
    });
    this.panelToolbar.appendSeparator();
    this.panelToolbar.appendToolbarItem(this.loadButton);
    this.panelToolbar.appendToolbarItem(this.saveButton);

    // History
    this.panelToolbar.appendSeparator();
    this.panelToolbar.appendToolbarItem(this.historyManager.button());
    this.panelToolbar.registerCSSFiles([historyToolbarButtonStyles]);
    this.panelToolbar.appendSeparator();

    // View
    this.panelToolbar.appendSeparator();
    this.showScreenshotsToolbarCheckbox =
        this.createSettingCheckbox(this.showScreenshotsSetting, i18nString(UIStrings.captureScreenshots));
    this.panelToolbar.appendToolbarItem(this.showScreenshotsToolbarCheckbox);

    this.showMemoryToolbarCheckbox =
        this.createSettingCheckbox(this.showMemorySetting, i18nString(UIStrings.showMemoryTimeline));
    this.panelToolbar.appendToolbarItem(this.showMemoryToolbarCheckbox);

    this.showWebVitalsToolbarCheckbox =
        this.createSettingCheckbox(this.showWebVitalsSetting, i18nString(UIStrings.showWebVitals));
    this.panelToolbar.appendToolbarItem(this.showWebVitalsToolbarCheckbox);

    if (Root.Runtime.experiments.isEnabled('recordCoverageWithPerformanceTracing')) {
      this.startCoverageCheckbox =
          this.createSettingCheckbox(this.startCoverage, i18nString(UIStrings.recordCoverageWithPerformance));
      this.panelToolbar.appendToolbarItem(this.startCoverageCheckbox);
    }

    // GC
    this.panelToolbar.appendToolbarItem(UI.Toolbar.Toolbar.createActionButtonForId('components.collect-garbage'));

    // Settings
    this.panelRightToolbar.appendSeparator();
    this.panelRightToolbar.appendToolbarItem(this.showSettingsPaneButton);
  }

  private createSettingsPane(): void {
    this.showSettingsPaneSetting =
        Common.Settings.Settings.instance().createSetting('timelineShowSettingsToolbar', false);
    this.showSettingsPaneButton = new UI.Toolbar.ToolbarSettingToggle(
        this.showSettingsPaneSetting, 'largeicon-settings-gear', i18nString(UIStrings.captureSettings));
    SDK.NetworkManager.MultitargetNetworkManager.instance().addEventListener(
        SDK.NetworkManager.MultitargetNetworkManager.Events.ConditionsChanged, this.updateShowSettingsToolbarButton,
        this);
    SDK.CPUThrottlingManager.CPUThrottlingManager.instance().addEventListener(
        SDK.CPUThrottlingManager.Events.RateChanged, this.updateShowSettingsToolbarButton, this);
    this.disableCaptureJSProfileSetting.addChangeListener(this.updateShowSettingsToolbarButton, this);
    this.captureLayersAndPicturesSetting.addChangeListener(this.updateShowSettingsToolbarButton, this);

    this.settingsPane = new UI.Widget.HBox();
    this.settingsPane.element.classList.add('timeline-settings-pane');
    this.settingsPane.show(this.element);

    const captureToolbar = new UI.Toolbar.Toolbar('', this.settingsPane.element);
    captureToolbar.element.classList.add('flex-auto');
    captureToolbar.makeVertical();
    captureToolbar.appendToolbarItem(this.createSettingCheckbox(
        this.disableCaptureJSProfileSetting, i18nString(UIStrings.disablesJavascriptSampling)));
    captureToolbar.appendToolbarItem(
        this.createSettingCheckbox(this.captureLayersAndPicturesSetting, i18nString(UIStrings.capturesAdvancedPaint)));

    const throttlingPane = new UI.Widget.VBox();
    throttlingPane.element.classList.add('flex-auto');
    throttlingPane.show(this.settingsPane.element);

    const networkThrottlingToolbar = new UI.Toolbar.Toolbar('', throttlingPane.element);
    networkThrottlingToolbar.appendText(i18nString(UIStrings.network));
    this.networkThrottlingSelect = this.createNetworkConditionsSelect();
    networkThrottlingToolbar.appendToolbarItem(this.networkThrottlingSelect);

    const cpuThrottlingToolbar = new UI.Toolbar.Toolbar('', throttlingPane.element);
    cpuThrottlingToolbar.appendText(i18nString(UIStrings.cpu));
    this.cpuThrottlingSelect = MobileThrottling.ThrottlingManager.throttlingManager().createCPUThrottlingSelector();
    cpuThrottlingToolbar.appendToolbarItem(this.cpuThrottlingSelect);

    this.showSettingsPaneSetting.addChangeListener(this.updateSettingsPaneVisibility.bind(this));
    this.updateSettingsPaneVisibility();
  }

  private appendExtensionsToToolbar(
      event: Common.EventTarget.EventTargetEvent<Extensions.ExtensionTraceProvider.ExtensionTraceProvider>): void {
    const provider = event.data;
    const setting = TimelinePanel.settingForTraceProvider(provider);
    const checkbox = this.createSettingCheckbox(setting, provider.longDisplayName());
    this.panelToolbar.appendToolbarItem(checkbox);
  }

  private static settingForTraceProvider(traceProvider: Extensions.ExtensionTraceProvider.ExtensionTraceProvider):
      Common.Settings.Setting<boolean> {
    let setting = traceProviderToSetting.get(traceProvider);
    if (!setting) {
      const providerId = traceProvider.persistentIdentifier();
      setting = Common.Settings.Settings.instance().createSetting(providerId, false);
      setting.setTitle(traceProvider.shortDisplayName());
      traceProviderToSetting.set(traceProvider, setting);
    }
    return setting;
  }

  private createNetworkConditionsSelect(): UI.Toolbar.ToolbarComboBox {
    const toolbarItem = new UI.Toolbar.ToolbarComboBox(null, i18nString(UIStrings.networkConditions));
    toolbarItem.setMaxWidth(140);
    MobileThrottling.ThrottlingManager.throttlingManager().decorateSelectWithNetworkThrottling(
        toolbarItem.selectElement());
    return toolbarItem;
  }

  private prepareToLoadTimeline(): void {
    console.assert(this.state === State.Idle);
    this.setState(State.Loading);
    if (this.performanceModel) {
      this.performanceModel.dispose();
      this.performanceModel = null;
    }
  }

  private createFileSelector(): void {
    if (this.fileSelectorElement) {
      this.fileSelectorElement.remove();
    }
    this.fileSelectorElement = UI.UIUtils.createFileSelectorElement(this.loadFromFile.bind(this));
    this.timelinePane.element.appendChild(this.fileSelectorElement);
  }

  private contextMenu(event: Event): void {
    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.appendItemsAtLocation('timelineMenu');
    void contextMenu.show();
  }
  async saveToFile(): Promise<void> {
    if (this.state !== State.Idle) {
      return;
    }
    const performanceModel = this.performanceModel;
    if (!performanceModel) {
      return;
    }

    const now = new Date();
    const fileName = 'Profile-' + Platform.DateUtilities.toISO8601Compact(now) + '.json';
    const stream = new Bindings.FileUtils.FileOutputStream();

    const accepted = await stream.open(fileName);
    if (!accepted) {
      return;
    }

    const error = (await performanceModel.save(stream) as {
      message: string,
      name: string,
      code: number,
    } | null);
    if (!error) {
      return;
    }
    Common.Console.Console.instance().error(
        i18nString(UIStrings.failedToSaveTimelineSSS, {PH1: error.message, PH2: error.name, PH3: error.code}));
  }

  async showHistory(): Promise<void> {
    const model = await this.historyManager.showHistoryDropDown();
    if (model && model !== this.performanceModel) {
      this.setModel(model);
    }
  }

  navigateHistory(direction: number): boolean {
    const model = this.historyManager.navigate(direction);
    if (model && model !== this.performanceModel) {
      this.setModel(model);
    }
    return true;
  }

  selectFileToLoad(): void {
    if (this.fileSelectorElement) {
      this.fileSelectorElement.click();
    }
  }

  private loadFromFile(file: File): void {
    if (this.state !== State.Idle) {
      return;
    }
    this.prepareToLoadTimeline();
    this.loader = TimelineLoader.loadFromFile(file, this);
    this.createFileSelector();
  }

  loadFromURL(url: string): void {
    if (this.state !== State.Idle) {
      return;
    }
    this.prepareToLoadTimeline();
    this.loader = TimelineLoader.loadFromURL(url, this);
  }

  private updateOverviewControls(): void {
    this.overviewControls = [];
    this.overviewControls.push(new TimelineEventOverviewResponsiveness());
    if (Root.Runtime.experiments.isEnabled('inputEventsOnTimelineOverview')) {
      this.overviewControls.push(new TimelineEventOverviewInput());
    }
    this.overviewControls.push(new TimelineEventOverviewFrames());
    this.overviewControls.push(new TimelineEventOverviewCPUActivity());
    this.overviewControls.push(new TimelineEventOverviewNetwork());
    if (this.showScreenshotsSetting.get() && this.performanceModel &&
        this.performanceModel.filmStripModel().frames().length) {
      this.overviewControls.push(new TimelineFilmStripOverview());
    }
    if (this.showMemorySetting.get()) {
      this.overviewControls.push(new TimelineEventOverviewMemory());
    }
    if (this.startCoverage.get()) {
      this.overviewControls.push(new TimelineEventOverviewCoverage());
    }
    for (const control of this.overviewControls) {
      control.setModel(this.performanceModel);
    }
    this.overviewPane.setOverviewControls(this.overviewControls);
  }

  private onModeChanged(): void {
    this.updateOverviewControls();
    this.doResize();
    this.select(null);
  }

  private onWebVitalsChanged(): void {
    this.flameChart.toggleWebVitalsLane();
  }

  private updateSettingsPaneVisibility(): void {
    if (this.showSettingsPaneSetting.get()) {
      this.settingsPane.showWidget();
    } else {
      this.settingsPane.hideWidget();
    }
  }

  private updateShowSettingsToolbarButton(): void {
    const messages: string[] = [];
    if (SDK.CPUThrottlingManager.CPUThrottlingManager.instance().cpuThrottlingRate() !== 1) {
      messages.push(i18nString(UIStrings.CpuThrottlingIsEnabled));
    }
    if (SDK.NetworkManager.MultitargetNetworkManager.instance().isThrottling()) {
      messages.push(i18nString(UIStrings.NetworkThrottlingIsEnabled));
    }
    if (this.captureLayersAndPicturesSetting.get()) {
      messages.push(i18nString(UIStrings.SignificantOverheadDueToPaint));
    }
    if (this.disableCaptureJSProfileSetting.get()) {
      messages.push(i18nString(UIStrings.JavascriptSamplingIsDisabled));
    }

    this.showSettingsPaneButton.setDefaultWithRedColor(messages.length > 0);
    this.showSettingsPaneButton.setToggleWithRedColor(messages.length > 0);

    if (messages.length) {
      const tooltipElement = document.createElement('div');
      messages.forEach(message => {
        tooltipElement.createChild('div').textContent = message;
      });
      this.showSettingsPaneButton.setTitle(tooltipElement.textContent || '');
    } else {
      this.showSettingsPaneButton.setTitle(i18nString(UIStrings.captureSettings));
    }
  }

  private setUIControlsEnabled(enabled: boolean): void {
    this.recordingOptionUIControls.forEach(control => control.setEnabled(enabled));
  }

  private async getCoverageViewWidget(): Promise<Coverage.CoverageView.CoverageView> {
    const view = (UI.ViewManager.ViewManager.instance().view('coverage') as UI.View.View);
    return await view.widget() as Coverage.CoverageView.CoverageView;
  }

  private async startRecording(): Promise<void> {
    console.assert(!this.statusPane, 'Status pane is already opened.');
    this.setState(State.StartPending);

    const recordingOptions = {
      enableJSSampling: !this.disableCaptureJSProfileSetting.get(),
      capturePictures: this.captureLayersAndPicturesSetting.get(),
      captureFilmStrip: this.showScreenshotsSetting.get(),
      startCoverage: this.startCoverage.get(),
    };

    if (recordingOptions.startCoverage) {
      await UI.ViewManager.ViewManager.instance()
          .showView('coverage')
          .then(() => this.getCoverageViewWidget())
          .then(widget => widget.ensureRecordingStarted());
    }

    this.showRecordingStarted();

    const enabledTraceProviders = Extensions.ExtensionServer.ExtensionServer.instance().traceProviders().filter(
        provider => TimelinePanel.settingForTraceProvider(provider).get());

    const mainTarget = (SDK.TargetManager.TargetManager.instance().mainTarget() as SDK.Target.Target);
    if (UIDevtoolsUtils.isUiDevTools()) {
      this.controller = new UIDevtoolsController(mainTarget, this);
    } else {
      this.controller = new TimelineController(mainTarget, this);
    }
    this.setUIControlsEnabled(false);
    this.hideLandingPage();
    try {
      const response = await this.controller.startRecording(recordingOptions, enabledTraceProviders);
      if (response.getError()) {
        throw new Error(response.getError());
      } else {
        this.recordingStarted();
      }
    } catch (e) {
      this.recordingFailed(e.message);
    }
  }

  private async stopRecording(): Promise<void> {
    if (this.statusPane) {
      this.statusPane.finish();
      this.statusPane.updateStatus(i18nString(UIStrings.stoppingTimeline));
      this.statusPane.updateProgressBar(i18nString(UIStrings.received), 0);
    }
    this.setState(State.StopPending);
    if (this.startCoverage.get()) {
      await UI.ViewManager.ViewManager.instance()
          .showView('coverage')
          .then(() => this.getCoverageViewWidget())
          .then(widget => widget.stopRecording());
    }
    if (this.controller) {
      const model = await this.controller.stopRecording();
      this.performanceModel = model;
      this.setUIControlsEnabled(true);
      this.controller.dispose();
      this.controller = null;
    }
  }

  private recordingFailed(error: string): void {
    if (this.statusPane) {
      this.statusPane.remove();
    }
    this.statusPane = new StatusPane(
        {
          description: error,
          buttonText: i18nString(UIStrings.close),
          buttonDisabled: false,
          showProgress: undefined,
          showTimer: undefined,
        },
        () => this.loadingComplete(null));
    this.statusPane.showPane(this.statusPaneContainer);
    this.statusPane.updateStatus(i18nString(UIStrings.recordingFailed));

    this.setState(State.RecordingFailed);
    this.performanceModel = null;
    this.setUIControlsEnabled(true);
    if (this.controller) {
      this.controller.dispose();
      this.controller = null;
    }
  }

  private onSuspendStateChanged(): void {
    this.updateTimelineControls();
  }

  private updateTimelineControls(): void {
    const state = State;
    this.toggleRecordAction.setToggled(this.state === state.Recording);
    this.toggleRecordAction.setEnabled(this.state === state.Recording || this.state === state.Idle);
    this.recordReloadAction.setEnabled(this.state === state.Idle);
    this.historyManager.setEnabled(this.state === state.Idle);
    this.clearButton.setEnabled(this.state === state.Idle);
    this.panelToolbar.setEnabled(this.state !== state.Loading);
    this.panelRightToolbar.setEnabled(this.state !== state.Loading);
    this.dropTarget.setEnabled(this.state === state.Idle);
    this.loadButton.setEnabled(this.state === state.Idle);
    this.saveButton.setEnabled(this.state === state.Idle && Boolean(this.performanceModel));
  }

  toggleRecording(): void {
    if (this.state === State.Idle) {
      this.recordingPageReload = false;
      void this.startRecording();
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.TimelineStarted);
    } else if (this.state === State.Recording) {
      void this.stopRecording();
    }
  }

  recordReload(): void {
    if (this.state !== State.Idle) {
      return;
    }
    this.recordingPageReload = true;
    void this.startRecording();
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.TimelinePageReloadStarted);
  }

  private onClearButton(): void {
    this.historyManager.clear();
    this.clear();
  }

  private clear(): void {
    this.showLandingPage();
    this.reset();
  }

  private reset(): void {
    PerfUI.LineLevelProfile.Performance.instance().reset();
    this.setModel(null);
  }

  private applyFilters(model: PerformanceModel): void {
    if (model.timelineModel().isGenericTrace() || Root.Runtime.experiments.isEnabled('timelineShowAllEvents')) {
      return;
    }
    model.setFilters([TimelineUIUtils.visibleEventsFilter()]);
  }

  private setModel(model: PerformanceModel|null): void {
    if (this.performanceModel) {
      this.performanceModel.removeEventListener(Events.WindowChanged, this.onModelWindowChanged, this);
    }
    this.performanceModel = model;
    if (model) {
      this.searchableViewInternal.showWidget();
      this.applyFilters(model);
    } else {
      this.searchableViewInternal.hideWidget();
    }
    this.flameChart.setModel(model);

    this.updateOverviewControls();
    this.overviewPane.reset();
    if (model && this.performanceModel) {
      this.performanceModel.addEventListener(Events.WindowChanged, this.onModelWindowChanged, this);
      this.overviewPane.setNavStartTimes(model.timelineModel().navStartTimes());
      this.overviewPane.setBounds(model.timelineModel().minimumRecordTime(), model.timelineModel().maximumRecordTime());
      PerfUI.LineLevelProfile.Performance.instance().reset();
      for (const profile of model.timelineModel().cpuProfiles()) {
        PerfUI.LineLevelProfile.Performance.instance().appendCPUProfile(profile);
      }
      this.setMarkers(model.timelineModel());
      this.flameChart.setSelection(null);
      this.overviewPane.setWindowTimes(model.window().left, model.window().right);
    }
    for (const control of this.overviewControls) {
      control.setModel(model);
    }
    if (this.flameChart) {
      this.flameChart.resizeToPreferredHeights();
    }
    this.updateTimelineControls();
  }

  private recordingStarted(): void {
    if (this.recordingPageReload && this.controller) {
      const target = this.controller.mainTarget();
      const resourceModel = target.model(SDK.ResourceTreeModel.ResourceTreeModel);
      if (resourceModel) {
        resourceModel.reloadPage();
      }
    }
    this.reset();
    this.setState(State.Recording);
    this.showRecordingStarted();
    if (this.statusPane) {
      this.statusPane.enableAndFocusButton();
      this.statusPane.updateStatus(i18nString(UIStrings.profiling));
      this.statusPane.updateProgressBar(i18nString(UIStrings.bufferUsage), 0);
      this.statusPane.startTimer();
    }
    this.hideLandingPage();
  }

  recordingProgress(usage: number): void {
    if (this.statusPane) {
      this.statusPane.updateProgressBar(i18nString(UIStrings.bufferUsage), usage * 100);
    }
  }

  private showLandingPage(): void {
    if (this.landingPage) {
      this.landingPage.show(this.statusPaneContainer);
      return;
    }

    function encloseWithTag(tagName: string, contents: string): HTMLElement {
      const e = document.createElement(tagName);
      e.textContent = contents;
      return e;
    }

    const learnMoreNode = UI.XLink.XLink.create(
        'https://developer.chrome.com/docs/devtools/evaluate-performance/', i18nString(UIStrings.learnmore));

    const recordKey = encloseWithTag(
        'b',
        UI.ShortcutRegistry.ShortcutRegistry.instance().shortcutsForAction('timeline.toggle-recording')[0].title());
    const reloadKey = encloseWithTag(
        'b', UI.ShortcutRegistry.ShortcutRegistry.instance().shortcutsForAction('timeline.record-reload')[0].title());
    const navigateNode = encloseWithTag('b', i18nString(UIStrings.wasd));

    this.landingPage = new UI.Widget.VBox();
    this.landingPage.contentElement.classList.add('timeline-landing-page', 'fill');
    const centered = this.landingPage.contentElement.createChild('div');

    const recordButton = UI.UIUtils.createInlineButton(UI.Toolbar.Toolbar.createActionButton(this.toggleRecordAction));
    const reloadButton =
        UI.UIUtils.createInlineButton(UI.Toolbar.Toolbar.createActionButtonForId('timeline.record-reload'));

    centered.createChild('p').appendChild(i18n.i18n.getFormatLocalizedString(
        str_, UIStrings.clickTheRecordButtonSOrHitSTo, {PH1: recordButton, PH2: recordKey}));

    centered.createChild('p').appendChild(i18n.i18n.getFormatLocalizedString(
        str_, UIStrings.clickTheReloadButtonSOrHitSTo, {PH1: reloadButton, PH2: reloadKey}));

    centered.createChild('p').appendChild(i18n.i18n.getFormatLocalizedString(
        str_, UIStrings.afterRecordingSelectAnAreaOf, {PH1: navigateNode, PH2: learnMoreNode}));

    this.landingPage.show(this.statusPaneContainer);
  }

  private hideLandingPage(): void {
    this.landingPage.detach();
  }

  loadingStarted(): void {
    this.hideLandingPage();

    if (this.statusPane) {
      this.statusPane.remove();
    }
    this.statusPane = new StatusPane(
        {
          showProgress: true,
          showTimer: undefined,
          buttonDisabled: undefined,
          buttonText: undefined,
          description: undefined,
        },
        () => this.cancelLoading());
    this.statusPane.showPane(this.statusPaneContainer);
    this.statusPane.updateStatus(i18nString(UIStrings.loadingProfile));
    // FIXME: make loading from backend cancelable as well.
    if (!this.loader) {
      this.statusPane.finish();
    }
    this.loadingProgress(0);
  }

  loadingProgress(progress?: number): void {
    if (typeof progress === 'number' && this.statusPane) {
      this.statusPane.updateProgressBar(i18nString(UIStrings.received), progress * 100);
    }
  }

  processingStarted(): void {
    if (this.statusPane) {
      this.statusPane.updateStatus(i18nString(UIStrings.processingProfile));
    }
  }

  loadingComplete(tracingModel: SDK.TracingModel.TracingModel|null): void {
    delete this.loader;
    this.setState(State.Idle);

    if (this.statusPane) {
      this.statusPane.remove();
    }
    this.statusPane = null;

    if (!tracingModel) {
      this.clear();
      return;
    }

    if (!this.performanceModel) {
      this.performanceModel = new PerformanceModel();
    }
    this.performanceModel.setTracingModel(tracingModel);
    this.setModel(this.performanceModel);
    this.historyManager.addRecording(this.performanceModel);

    if (this.startCoverage.get()) {
      void UI.ViewManager.ViewManager.instance()
          .showView('coverage')
          .then(() => this.getCoverageViewWidget())
          .then(widget => widget.processBacklog())
          .then(() => this.updateOverviewControls());
    }
  }

  private showRecordingStarted(): void {
    if (this.statusPane) {
      return;
    }
    this.statusPane = new StatusPane(
        {
          showTimer: true,
          showProgress: true,
          buttonDisabled: true,
          description: undefined,
          buttonText: undefined,
        },
        () => this.stopRecording());
    this.statusPane.showPane(this.statusPaneContainer);
    this.statusPane.updateStatus(i18nString(UIStrings.initializingProfiler));
  }

  private cancelLoading(): void {
    if (this.loader) {
      this.loader.cancel();
    }
  }

  private setMarkers(timelineModel: TimelineModel.TimelineModel.TimelineModelImpl): void {
    const markers = new Map<number, Element>();
    const recordTypes = TimelineModel.TimelineModel.RecordType;
    const zeroTime = timelineModel.minimumRecordTime();
    for (const event of timelineModel.timeMarkerEvents()) {
      if (event.name === recordTypes.TimeStamp || event.name === recordTypes.ConsoleTime) {
        continue;
      }
      markers.set(event.startTime, TimelineUIUtils.createEventDivider(event, zeroTime));
    }

    // Add markers for navigation start times.
    for (const navStartTimeEvent of timelineModel.navStartTimes().values()) {
      markers.set(navStartTimeEvent.startTime, TimelineUIUtils.createEventDivider(navStartTimeEvent, zeroTime));
    }
    this.overviewPane.setMarkers(markers);
  }

  private async loadEventFired(
      event: Common.EventTarget
          .EventTargetEvent<{resourceTreeModel: SDK.ResourceTreeModel.ResourceTreeModel, loadTime: number}>):
      Promise<void> {
    if (this.state !== State.Recording || !this.recordingPageReload || !this.controller ||
        this.controller.mainTarget() !== event.data.resourceTreeModel.target()) {
      return;
    }
    const controller = this.controller;
    await new Promise(r => window.setTimeout(r, this.millisecondsToRecordAfterLoadEvent));

    // Check if we're still in the same recording session.
    if (controller !== this.controller || this.state !== State.Recording) {
      return;
    }
    void this.stopRecording();
  }

  private frameForSelection(selection: TimelineSelection): TimelineModel.TimelineFrameModel.TimelineFrame|null {
    switch (selection.type()) {
      case TimelineSelection.Type.Frame:
        return selection.object() as TimelineModel.TimelineFrameModel.TimelineFrame;
      case TimelineSelection.Type.Range:
        return null;
      case TimelineSelection.Type.TraceEvent:
        if (!this.performanceModel) {
          return null;
        }
        return this.performanceModel.frameModel().getFramesWithinWindow(
            selection.endTimeInternal, selection.endTimeInternal)[0];
      default:
        console.assert(false, 'Should never be reached');
        return null;
    }
  }

  jumpToFrame(offset: number): true|undefined {
    const currentFrame = this.selection && this.frameForSelection(this.selection);
    if (!currentFrame || !this.performanceModel) {
      return;
    }
    const frames = this.performanceModel.frames();
    let index = frames.indexOf(currentFrame);
    console.assert(index >= 0, 'Can\'t find current frame in the frame list');
    index = Platform.NumberUtilities.clamp(index + offset, 0, frames.length - 1);
    const frame = frames[index];
    this.revealTimeRange(frame.startTime, frame.endTime);
    this.select(TimelineSelection.fromFrame(frame));
    return true;
  }

  select(selection: TimelineSelection|null): void {
    this.selection = selection;
    this.flameChart.setSelection(selection);
  }

  selectEntryAtTime(events: SDK.TracingModel.Event[]|null, time: number): void {
    if (!events) {
      return;
    }
    // Find best match, then backtrack to the first visible entry.
    for (let index = Platform.ArrayUtilities.upperBound(events, time, (time, event) => time - event.startTime) - 1;
         index >= 0; --index) {
      const event = events[index];
      const endTime = event.endTime || event.startTime;
      if (SDK.TracingModel.TracingModel.isTopLevelEvent(event) && endTime < time) {
        break;
      }
      if (this.performanceModel && this.performanceModel.isVisible(event) && endTime >= time) {
        this.select(TimelineSelection.fromTraceEvent(event));
        return;
      }
    }
    this.select(null);
  }

  highlightEvent(event: SDK.TracingModel.Event|null): void {
    this.flameChart.highlightEvent(event);
  }

  private revealTimeRange(startTime: number, endTime: number): void {
    if (!this.performanceModel) {
      return;
    }
    const window = this.performanceModel.window();
    let offset = 0;
    if (window.right < endTime) {
      offset = endTime - window.right;
    } else if (window.left > startTime) {
      offset = startTime - window.left;
    }
    this.performanceModel.setWindow({left: window.left + offset, right: window.right + offset}, /* animate */ true);
  }

  private handleDrop(dataTransfer: DataTransfer): void {
    const items = dataTransfer.items;
    if (!items.length) {
      return;
    }
    const item = items[0];
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.PerfPanelTraceImported);
    if (item.kind === 'string') {
      const url = dataTransfer.getData('text/uri-list');
      if (new Common.ParsedURL.ParsedURL(url).isValid) {
        this.loadFromURL(url);
      }
    } else if (item.kind === 'file') {
      const file = items[0].getAsFile();
      if (!file) {
        return;
      }
      this.loadFromFile(file);
    }
  }
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum State {
  Idle = 'Idle',
  StartPending = 'StartPending',
  Recording = 'Recording',
  StopPending = 'StopPending',
  Loading = 'Loading',
  RecordingFailed = 'RecordingFailed',
}

// TODO(crbug.com/1167717): Make this a const enum again
// eslint-disable-next-line rulesdir/const_enum
export enum ViewMode {
  FlameChart = 'FlameChart',
  BottomUp = 'BottomUp',
  CallTree = 'CallTree',
  EventLog = 'EventLog',
}

// Define row and header height, should be in sync with styles for timeline graphs.
export const rowHeight = 18;

export const headerHeight = 20;

export class TimelineSelection {
  private readonly typeInternal: string;
  private readonly startTimeInternal: number;
  readonly endTimeInternal: number;
  private readonly objectInternal: Object|null;

  constructor(type: string, startTime: number, endTime: number, object?: Object) {
    this.typeInternal = type;
    this.startTimeInternal = startTime;
    this.endTimeInternal = endTime;
    this.objectInternal = object || null;
  }

  static fromFrame(frame: TimelineModel.TimelineFrameModel.TimelineFrame): TimelineSelection {
    return new TimelineSelection(TimelineSelection.Type.Frame, frame.startTime, frame.endTime, frame);
  }

  static fromNetworkRequest(request: TimelineModel.TimelineModel.NetworkRequest): TimelineSelection {
    return new TimelineSelection(
        TimelineSelection.Type.NetworkRequest, request.startTime, request.endTime || request.startTime, request);
  }

  static fromTraceEvent(event: SDK.TracingModel.Event): TimelineSelection {
    return new TimelineSelection(
        TimelineSelection.Type.TraceEvent, event.startTime, event.endTime || (event.startTime + 1), event);
  }

  static fromRange(startTime: number, endTime: number): TimelineSelection {
    return new TimelineSelection(TimelineSelection.Type.Range, startTime, endTime);
  }

  type(): string {
    return this.typeInternal;
  }

  object(): Object|null {
    return this.objectInternal;
  }

  startTime(): number {
    return this.startTimeInternal;
  }

  endTime(): number {
    return this.endTimeInternal;
  }
}

export namespace TimelineSelection {
  // TODO(crbug.com/1167717): Make this a const enum again
  // eslint-disable-next-line rulesdir/const_enum
  export enum Type {
    Frame = 'Frame',
    NetworkRequest = 'NetworkRequest',
    TraceEvent = 'TraceEvent',
    Range = 'Range',
  }
}
export interface TimelineModeViewDelegate {
  select(selection: TimelineSelection|null): void;
  selectEntryAtTime(events: SDK.TracingModel.Event[]|null, time: number): void;
  highlightEvent(event: SDK.TracingModel.Event|null): void;
}

export class StatusPane extends UI.Widget.VBox {
  private status: HTMLElement;
  private time!: Element;
  private progressLabel!: Element;
  private progressBar!: Element;
  private readonly description: HTMLElement|undefined;
  private button: HTMLButtonElement;
  private startTime!: number;
  private timeUpdateTimer?: number;

  constructor(
      options: {
        showTimer?: boolean,
        showProgress?: boolean,
        description?: string,
        buttonText?: string,
        buttonDisabled?: boolean,
      },
      buttonCallback: () => (Promise<void>| void)) {
    super(true);

    this.contentElement.classList.add('timeline-status-dialog');

    const statusLine = this.contentElement.createChild('div', 'status-dialog-line status');
    statusLine.createChild('div', 'label').textContent = i18nString(UIStrings.status);
    this.status = statusLine.createChild('div', 'content');
    UI.ARIAUtils.markAsStatus(this.status);

    if (options.showTimer) {
      const timeLine = this.contentElement.createChild('div', 'status-dialog-line time');
      timeLine.createChild('div', 'label').textContent = i18nString(UIStrings.time);
      this.time = timeLine.createChild('div', 'content');
    }

    if (options.showProgress) {
      const progressLine = this.contentElement.createChild('div', 'status-dialog-line progress');
      this.progressLabel = progressLine.createChild('div', 'label');
      this.progressBar = progressLine.createChild('div', 'indicator-container').createChild('div', 'indicator');
      UI.ARIAUtils.markAsProgressBar(this.progressBar);
    }

    if (typeof options.description === 'string') {
      const descriptionLine = this.contentElement.createChild('div', 'status-dialog-line description');
      descriptionLine.createChild('div', 'label').textContent = i18nString(UIStrings.description);
      this.description = descriptionLine.createChild('div', 'content');
      this.description.innerText = options.description;
    }

    const buttonText = options.buttonText || i18nString(UIStrings.stop);
    this.button = UI.UIUtils.createTextButton(buttonText, buttonCallback, '', true);
    // Profiling can't be stopped during initialization.
    this.button.disabled = !options.buttonDisabled === false;
    this.contentElement.createChild('div', 'stop-button').appendChild(this.button);
  }

  finish(): void {
    this.stopTimer();
    this.button.disabled = true;
  }

  remove(): void {
    (this.element.parentNode as HTMLElement).classList.remove('tinted');
    this.arrangeDialog((this.element.parentNode as HTMLElement));
    this.stopTimer();
    this.element.remove();
  }

  showPane(parent: Element): void {
    this.arrangeDialog(parent);
    this.show(parent);
    parent.classList.add('tinted');
  }

  enableAndFocusButton(): void {
    this.button.disabled = false;
    this.button.focus();
  }

  updateStatus(text: string): void {
    this.status.textContent = text;
  }

  updateProgressBar(activity: string, percent: number): void {
    this.progressLabel.textContent = activity;
    (this.progressBar as HTMLElement).style.width = percent.toFixed(1) + '%';
    UI.ARIAUtils.setValueNow(this.progressBar, percent);
    this.updateTimer();
  }

  startTimer(): void {
    this.startTime = Date.now();
    this.timeUpdateTimer = window.setInterval(this.updateTimer.bind(this, false), 1000);
    this.updateTimer();
  }

  private stopTimer(): void {
    if (!this.timeUpdateTimer) {
      return;
    }
    clearInterval(this.timeUpdateTimer);
    this.updateTimer(true);
    delete this.timeUpdateTimer;
  }

  private updateTimer(precise?: boolean): void {
    this.arrangeDialog((this.element.parentNode as HTMLElement));
    if (!this.timeUpdateTimer) {
      return;
    }
    const elapsed = (Date.now() - this.startTime) / 1000;
    this.time.textContent = i18nString(UIStrings.ssec, {PH1: elapsed.toFixed(precise ? 1 : 0)});
  }

  private arrangeDialog(parent: Element): void {
    const isSmallDialog = parent.clientWidth < 325;
    this.element.classList.toggle('small-dialog', isSmallDialog);
    this.contentElement.classList.toggle('small-dialog', isSmallDialog);
  }
  wasShown(): void {
    super.wasShown();
    this.registerCSSFiles([timelineStatusDialogStyles]);
  }
}

let loadTimelineHandlerInstance: LoadTimelineHandler;

export class LoadTimelineHandler implements Common.QueryParamHandler.QueryParamHandler {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): LoadTimelineHandler {
    const {forceNew} = opts;
    if (!loadTimelineHandlerInstance || forceNew) {
      loadTimelineHandlerInstance = new LoadTimelineHandler();
    }

    return loadTimelineHandlerInstance;
  }

  handleQueryParam(value: string): void {
    void UI.ViewManager.ViewManager.instance().showView('timeline').then(() => {
      TimelinePanel.instance().loadFromURL(window.decodeURIComponent(value));
    });
  }
}

let actionDelegateInstance: ActionDelegate;

export class ActionDelegate implements UI.ActionRegistration.ActionDelegate {
  static instance(opts: {
    forceNew: boolean|null,
  }|undefined = {forceNew: null}): ActionDelegate {
    const {forceNew} = opts;
    if (!actionDelegateInstance || forceNew) {
      actionDelegateInstance = new ActionDelegate();
    }

    return actionDelegateInstance;
  }

  handleAction(context: UI.Context.Context, actionId: string): boolean {
    const panel = (UI.Context.Context.instance().flavor(TimelinePanel) as TimelinePanel);
    console.assert(panel && panel instanceof TimelinePanel);
    switch (actionId) {
      case 'timeline.toggle-recording':
        panel.toggleRecording();
        return true;
      case 'timeline.record-reload':
        panel.recordReload();
        return true;
      case 'timeline.save-to-file':
        void panel.saveToFile();
        return true;
      case 'timeline.load-from-file':
        panel.selectFileToLoad();
        return true;
      case 'timeline.jump-to-previous-frame':
        panel.jumpToFrame(-1);
        return true;
      case 'timeline.jump-to-next-frame':
        panel.jumpToFrame(1);
        return true;
      case 'timeline.show-history':
        void panel.showHistory();
        return true;
      case 'timeline.previous-recording':
        panel.navigateHistory(1);
        return true;
      case 'timeline.next-recording':
        panel.navigateHistory(-1);
        return true;
    }
    return false;
  }
}

const traceProviderToSetting =
    new WeakMap<Extensions.ExtensionTraceProvider.ExtensionTraceProvider, Common.Settings.Setting<boolean>>();
