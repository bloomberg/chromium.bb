// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {validateRecordConfig} from '../controller/validate_config';
import {
  AggregationAttrs,
  PivotAttrs,
  SubQueryAttrs,
  TableAttrs
} from './pivot_table_common';

/**
 * A plain js object, holding objects of type |Class| keyed by string id.
 * We use this instead of using |Map| object since it is simpler and faster to
 * serialize for use in postMessage.
 */
export interface ObjectById<Class extends{id: string}> { [id: string]: Class; }

export type Timestamped<T> = {
  [P in keyof T]: T[P];
}&{lastUpdate: number};

export type OmniboxMode = 'SEARCH'|'COMMAND';

export type OmniboxState = Timestamped<{omnibox: string; mode: OmniboxMode}>;

export type VisibleState =
    Timestamped<{startSec: number; endSec: number; resolution: number;}>;

export interface AreaSelection {
  kind: 'AREA';
  areaId: string;
  // When an area is marked it will be assigned a unique note id and saved as
  // an AreaNote for the user to return to later. id = 0 is the special id that
  // is overwritten when a new area is marked. Any other id is a persistent
  // marking that will not be overwritten.
  // When not set, the area selection will be replaced with any
  // new area selection (i.e. not saved anywhere).
  noteId?: string;
}

export type AreaById = Area&{id: string};

export interface Area {
  startSec: number;
  endSec: number;
  tracks: string[];
}

export const MAX_TIME = 180;

// 3: TrackKindPriority and related sorting changes.
// 5: Move a large number of items off frontendLocalState and onto state.
// 6: Common PivotTableConfig and pivot table specific PivotTableState.
// 7: Split Chrome categories in two and add 'symbolize ksyms' flag.
// 8: Rename several variables
// 9: Add a field to track last loaded recording profile name
// "[...]HeapProfileFlamegraph[...]" -> "[...]Flamegraph[...]".
export const STATE_VERSION = 9;

export const SCROLLING_TRACK_GROUP = 'ScrollingTracks';

export type EngineMode = 'WASM'|'HTTP_RPC';

export type NewEngineMode = 'USE_HTTP_RPC_IF_AVAILABLE'|'FORCE_BUILTIN_WASM';

export enum TrackKindPriority {
  'MAIN_THREAD' = 0,
  'RENDER_THREAD' = 1,
  'GPU_COMPLETION' = 2,
  'ORDINARY' = 3
}

export type FlamegraphStateViewingOption =
    'SPACE'|'ALLOC_SPACE'|'OBJECTS'|'ALLOC_OBJECTS'|'PERF_SAMPLES';

export interface CallsiteInfo {
  id: number;
  parentId: number;
  depth: number;
  name?: string;
  totalSize: number;
  selfSize: number;
  mapping: string;
  merged: boolean;
  highlighted: boolean;
  location?: string;
}

export interface TraceFileSource {
  type: 'FILE';
  file: File;
}

export interface TraceArrayBufferSource {
  type: 'ARRAY_BUFFER';
  buffer: ArrayBuffer;
  title: string;
  url?: string;
  fileName?: string;

  // |uuid| is set only when loading from the cache via ?trace_id=123. When set,
  // this matches global.state.traceUuid, with the exception of the following
  // time window: When a trace T1 is loaded and the user loads another trace T2,
  // this |uuid| will be == T2, but the globals.state.traceUuid will be
  // temporarily == T1 until T2 has been loaded (consistently to what happens
  // with all other state fields).
  uuid?: string;
  // if |localOnly| is true then the trace should not be shared or downloaded.
  localOnly?: boolean;
}

export interface TraceUrlSource {
  type: 'URL';
  url: string;
}

export interface TraceHttpRpcSource {
  type: 'HTTP_RPC';
}

export type TraceSource =
    TraceFileSource|TraceArrayBufferSource|TraceUrlSource|TraceHttpRpcSource;

export interface TrackState {
  id: string;
  engineId: string;
  kind: string;
  name: string;
  trackKindPriority: TrackKindPriority;
  trackGroup?: string;
  config: {};
}

export interface TrackGroupState {
  id: string;
  engineId: string;
  name: string;
  collapsed: boolean;
  tracks: string[];  // Child track ids.
}

export interface EngineConfig {
  id: string;
  mode?: EngineMode;  // Is undefined until |ready| is true.
  ready: boolean;
  failed?: string;  // If defined the engine has crashed with the given message.
  source: TraceSource;
}

export interface QueryConfig {
  id: string;
  engineId: string;
  query: string;
}

export interface PermalinkConfig {
  requestId?: string;  // Set by the frontend to request a new permalink.
  hash?: string;       // Set by the controller when the link has been created.
  isRecordingConfig?:
      boolean;  // this permalink request is for a recording config only
}

export interface TraceTime {
  startSec: number;
  endSec: number;
}

export interface FrontendLocalState {
  omniboxState: OmniboxState;
  visibleState: VisibleState;
}

export interface Status {
  msg: string;
  timestamp: number;  // Epoch in seconds (Date.now() / 1000).
}

export interface Note {
  noteType: 'DEFAULT';
  id: string;
  timestamp: number;
  color: string;
  text: string;
}

export interface AreaNote {
  noteType: 'AREA';
  id: string;
  areaId: string;
  color: string;
  text: string;
}

export interface NoteSelection {
  kind: 'NOTE';
  id: string;
}

export interface SliceSelection {
  kind: 'SLICE';
  id: number;
}

export interface CounterSelection {
  kind: 'COUNTER';
  leftTs: number;
  rightTs: number;
  id: number;
}

export interface HeapProfileSelection {
  kind: 'HEAP_PROFILE';
  id: number;
  upid: number;
  ts: number;
  type: string;
}

export interface PerfSamplesSelection {
  kind: 'PERF_SAMPLES';
  id: number;
  upid: number;
  ts: number;
  type: string;
}

export interface FlamegraphState {
  kind: 'FLAMEGRAPH_STATE';
  id: number;
  upid: number;
  ts: number;
  type: string;
  viewingOption: FlamegraphStateViewingOption;
  focusRegex: string;
  expandedCallsite?: CallsiteInfo;
}

export interface CpuProfileSampleSelection {
  kind: 'CPU_PROFILE_SAMPLE';
  id: number;
  utid: number;
  ts: number;
}

export interface ChromeSliceSelection {
  kind: 'CHROME_SLICE';
  id: number;
  table: string;
}

export interface ThreadStateSelection {
  kind: 'THREAD_STATE';
  id: number;
}

type Selection =
    (NoteSelection|SliceSelection|CounterSelection|HeapProfileSelection|
     CpuProfileSampleSelection|ChromeSliceSelection|ThreadStateSelection|
     AreaSelection|PerfSamplesSelection)&{trackId?: string};

export interface LogsPagination {
  offset: number;
  count: number;
}

export interface RecordingTarget {
  name: string;
  os: TargetOs;
}

export interface AdbRecordingTarget extends RecordingTarget {
  serial: string;
}

export interface Sorting {
  column: string;
  direction: 'DESC'|'ASC';
}

export interface AggregationState {
  id: string;
  sorting?: Sorting;
}

export interface MetricsState {
  availableMetrics?: string[];  // Undefined until list is loaded.
  selectedIndex?: number;
  requestedMetric?: string;  // Unset after metric request is handled.
}

export interface PivotTableConfig {
  availableColumns?: TableAttrs[];   // Undefined until list is loaded.
  availableAggregations?: string[];  // Undefined until list is loaded.
}

export interface PivotTableState {
  id: string;
  name: string;
  selectedPivots: PivotAttrs[];
  selectedAggregations: AggregationAttrs[];
  requestedAction?:  // Unset after pivot table column request is handled.
      {action: string, attrs?: SubQueryAttrs};
  isLoadingQuery: boolean;
  traceTime?: TraceTime;
  selectedTrackIds?: number[];
}

export interface State {
  // tslint:disable-next-line:no-any
  [key: string]: any;
  version: number;
  nextId: number;
  nextNoteId: number;
  nextAreaId: number;

  /**
   * State of the ConfigEditor.
   */
  recordConfig: RecordConfig;
  displayConfigAsPbtxt: boolean;
  lastLoadedConfigTitle: string|null;

  /**
   * Open traces.
   */
  newEngineMode: NewEngineMode;
  engines: ObjectById<EngineConfig>;
  traceTime: TraceTime;
  traceUuid?: string;
  trackGroups: ObjectById<TrackGroupState>;
  tracks: ObjectById<TrackState>;
  areas: ObjectById<AreaById>;
  aggregatePreferences: ObjectById<AggregationState>;
  visibleTracks: string[];
  scrollingTracks: string[];
  pinnedTracks: string[];
  debugTrackId?: string;
  lastTrackReloadRequest?: number;
  queries: ObjectById<QueryConfig>;
  metrics: MetricsState;
  permalink: PermalinkConfig;
  notes: ObjectById<Note|AreaNote>;
  status: Status;
  currentSelection: Selection|null;
  currentFlamegraphState: FlamegraphState|null;
  logsPagination: LogsPagination;
  traceConversionInProgress: boolean;
  pivotTableConfig: PivotTableConfig;
  pivotTable: ObjectById<PivotTableState>;

  /**
   * This state is updated on the frontend at 60Hz and eventually syncronised to
   * the controller at 10Hz. When the controller sends state updates to the
   * frontend the frontend has special logic to pick whichever version of this
   * key is most up to date.
   */
  frontendLocalState: FrontendLocalState;

  // Show track perf debugging overlay
  perfDebug: boolean;

  // Show the sidebar extended
  sidebarVisible: boolean;

  // Hovered and focused events
  hoveredUtid: number;
  hoveredPid: number;
  hoveredLogsTimestamp: number;
  hoveredNoteTimestamp: number;
  highlightedSliceId: number;
  focusedFlowIdLeft: number;
  focusedFlowIdRight: number;

  searchIndex: number;
  currentTab?: string;

  /**
   * Trace recording
   */
  recordingInProgress: boolean;
  recordingCancelled: boolean;
  extensionInstalled: boolean;
  recordingTarget: RecordingTarget;
  availableAdbDevices: AdbRecordingTarget[];
  lastRecordingError?: string;
  recordingStatus?: string;

  updateChromeCategories: boolean;
  chromeCategories: string[]|undefined;
  analyzePageQuery?: string;
}

export const defaultTraceTime = {
  startSec: 0,
  endSec: 10,
};

export declare type RecordMode =
    'STOP_WHEN_FULL' | 'RING_BUFFER' | 'LONG_TRACE';

// 'Q','P','O' for Android, 'L' for Linux, 'C' for Chrome.
export declare type TargetOs = 'Q' | 'P' | 'O' | 'C' | 'L' | 'CrOS';

export function isAndroidP(target: RecordingTarget) {
  return target.os === 'P';
}

export function isAndroidTarget(target: RecordingTarget) {
  return ['Q', 'P', 'O'].includes(target.os);
}

export function isChromeTarget(target: RecordingTarget) {
  return ['C', 'CrOS'].includes(target.os);
}

export function isCrOSTarget(target: RecordingTarget) {
  return target.os === 'CrOS';
}

export function isLinuxTarget(target: RecordingTarget) {
  return target.os === 'L';
}

export function isAdbTarget(target: RecordingTarget):
    target is AdbRecordingTarget {
  return !!(target as AdbRecordingTarget).serial;
}

export function hasActiveProbes(config: RecordConfig) {
  const fieldsWithEmptyResult = new Set<string>(['hpBlockClient']);
  let key: keyof RecordConfig;
  for (key in config) {
    if (typeof (config[key]) === 'boolean' && config[key] === true &&
        !fieldsWithEmptyResult.has(key)) {
      return true;
    }
  }
  if (config.chromeCategoriesSelected.length > 0) {
    return true;
  }
  if (config.chromeHighOverheadCategoriesSelected.length > 0) {
    return true;
  }
  return false;
}

export interface RecordConfig {
  // Global settings
  mode: RecordMode;
  durationMs: number;
  bufferSizeMb: number;
  maxFileSizeMb: number;      // Only for mode == 'LONG_TRACE'.
  fileWritePeriodMs: number;  // Only for mode == 'LONG_TRACE'.

  cpuSched: boolean;
  cpuFreq: boolean;
  cpuCoarse: boolean;
  cpuCoarsePollMs: number;
  cpuSyscall: boolean;

  gpuFreq: boolean;
  gpuMemTotal: boolean;

  ftrace: boolean;
  atrace: boolean;
  ftraceEvents: string[];
  ftraceExtraEvents: string;
  atraceCats: string[];
  atraceApps: string;
  ftraceBufferSizeKb: number;
  ftraceDrainPeriodMs: number;
  androidLogs: boolean;
  androidLogBuffers: string[];
  androidFrameTimeline: boolean;

  batteryDrain: boolean;
  batteryDrainPollMs: number;

  boardSensors: boolean;

  memHiFreq: boolean;
  memLmk: boolean;
  meminfo: boolean;
  meminfoPeriodMs: number;
  meminfoCounters: string[];
  vmstat: boolean;
  vmstatPeriodMs: number;
  vmstatCounters: string[];

  heapProfiling: boolean;
  hpSamplingIntervalBytes: number;
  hpProcesses: string;
  hpContinuousDumpsPhase: number;
  hpContinuousDumpsInterval: number;
  hpSharedMemoryBuffer: number;
  hpBlockClient: boolean;
  hpAllHeaps: boolean;

  javaHeapDump: boolean;
  jpProcesses: string;
  jpContinuousDumpsPhase: number;
  jpContinuousDumpsInterval: number;

  procStats: boolean;
  procStatsPeriodMs: number;

  chromeCategoriesSelected: string[];
  chromeHighOverheadCategoriesSelected: string[];

  chromeLogs: boolean;
  taskScheduling: boolean;
  ipcFlows: boolean;
  jsExecution: boolean;
  webContentRendering: boolean;
  uiRendering: boolean;
  inputEvents: boolean;
  navigationAndLoading: boolean;

  symbolizeKsyms: boolean;
}

export function createEmptyRecordConfig(): RecordConfig {
  return validateRecordConfig({});
}

export function getDefaultRecordingTargets(): RecordingTarget[] {
  return [
    {os: 'Q', name: 'Android Q+'},
    {os: 'P', name: 'Android P'},
    {os: 'O', name: 'Android O-'},
    {os: 'C', name: 'Chrome'},
    {os: 'CrOS', name: 'Chrome OS (system trace)'},
    {os: 'L', name: 'Linux desktop'}
  ];
}

export function getBuiltinChromeCategoryList(): string[] {
  // List of static Chrome categories, last updated at 2021-09-09 from HEAD of
  // Chromium's //base/trace_event/builtin_categories.h.
  return [
    'accessibility',
    'AccountFetcherService',
    'android_webview',
    'aogh',
    'audio',
    'base',
    'benchmark',
    'blink',
    'blink.animations',
    'blink.bindings',
    'blink.console',
    'blink.net',
    'blink.resource',
    'blink.user_timing',
    'blink.worker',
    'blink_gc',
    'blink_style',
    'Blob',
    'browser',
    'browsing_data',
    'CacheStorage',
    'Calculators',
    'CameraStream',
    'camera',
    'cast_app',
    'cast_perf_test',
    'cast.mdns',
    'cast.mdns.socket',
    'cast.stream',
    'cc',
    'cc.debug',
    'cdp.perf',
    'chromeos',
    'cma',
    'compositor',
    'content',
    'content_capture',
    'device',
    'devtools',
    'devtools.contrast',
    'devtools.timeline',
    'disk_cache',
    'download',
    'download_service',
    'drm',
    'drmcursor',
    'dwrite',
    'DXVA_Decoding',
    'evdev',
    'event',
    'exo',
    'extensions',
    'explore_sites',
    'FileSystem',
    'file_system_provider',
    'fonts',
    'GAMEPAD',
    'gpu',
    'gpu.angle',
    'gpu.capture',
    'headless',
    'hwoverlays',
    'identity',
    'ime',
    'IndexedDB',
    'input',
    'io',
    'ipc',
    'Java',
    'jni',
    'jpeg',
    'latency',
    'latencyInfo',
    'leveldb',
    'loading',
    'log',
    'login',
    'media',
    'media_router',
    'memory',
    'midi',
    'mojom',
    'mus',
    'native',
    'navigation',
    'net',
    'netlog',
    'offline_pages',
    'omnibox',
    'oobe',
    'ozone',
    'partition_alloc',
    'passwords',
    'p2p',
    'page-serialization',
    'paint_preview',
    'pepper',
    'PlatformMalloc',
    'power',
    'ppapi',
    'ppapi_proxy',
    'print',
    'rail',
    'renderer',
    'renderer_host',
    'renderer.scheduler',
    'RLZ',
    'safe_browsing',
    'screenlock_monitor',
    'segmentation_platform',
    'sequence_manager',
    'service_manager',
    'ServiceWorker',
    'sharing',
    'shell',
    'shortcut_viewer',
    'shutdown',
    'SiteEngagement',
    'skia',
    'sql',
    'stadia_media',
    'stadia_rtc',
    'startup',
    'sync',
    'system_apps',
    'test_gpu',
    'thread_pool',
    'toplevel',
    'toplevel.flow',
    'ui',
    'v8',
    'v8.execute',
    'v8.wasm',
    'ValueStoreFrontend::Backend',
    'views',
    'views.frame',
    'viz',
    'vk',
    'wayland',
    'webaudio',
    'weblayer',
    'WebCore',
    'webrtc',
    'xr',
    'disabled-by-default-animation-worklet',
    'disabled-by-default-audio',
    'disabled-by-default-audio-worklet',
    'disabled-by-default-base',
    'disabled-by-default-blink.debug',
    'disabled-by-default-blink.debug.display_lock',
    'disabled-by-default-blink.debug.layout',
    'disabled-by-default-blink.debug.layout.trees',
    'disabled-by-default-blink.feature_usage',
    'disabled-by-default-blink_gc',
    'disabled-by-default-blink.image_decoding',
    'disabled-by-default-blink.invalidation',
    'disabled-by-default-cc',
    'disabled-by-default-cc.debug',
    'disabled-by-default-cc.debug.cdp-perf',
    'disabled-by-default-cc.debug.display_items',
    'disabled-by-default-cc.debug.picture',
    'disabled-by-default-cc.debug.scheduler',
    'disabled-by-default-cc.debug.scheduler.frames',
    'disabled-by-default-cc.debug.scheduler.now',
    'disabled-by-default-content.verbose',
    'disabled-by-default-cpu_profiler',
    'disabled-by-default-cpu_profiler.debug',
    'disabled-by-default-devtools.screenshot',
    'disabled-by-default-devtools.timeline',
    'disabled-by-default-devtools.timeline.frame',
    'disabled-by-default-devtools.timeline.inputs',
    'disabled-by-default-devtools.timeline.invalidationTracking',
    'disabled-by-default-devtools.timeline.layers',
    'disabled-by-default-devtools.timeline.picture',
    'disabled-by-default-file',
    'disabled-by-default-fonts',
    'disabled-by-default-gpu_cmd_queue',
    'disabled-by-default-gpu.dawn',
    'disabled-by-default-gpu.debug',
    'disabled-by-default-gpu.decoder',
    'disabled-by-default-gpu.device',
    'disabled-by-default-gpu.service',
    'disabled-by-default-gpu.vulkan.vma',
    'disabled-by-default-histogram_samples',
    'disabled-by-default-java-heap-profiler',
    'disabled-by-default-layer-element',
    'disabled-by-default-layout_shift.debug',
    'disabled-by-default-lifecycles',
    'disabled-by-default-loading',
    'disabled-by-default-mediastream',
    'disabled-by-default-memory-infra',
    'disabled-by-default-memory-infra.v8.code_stats',
    'disabled-by-default-mojom',
    'disabled-by-default-net',
    'disabled-by-default-network',
    'disabled-by-default-paint-worklet',
    'disabled-by-default-power',
    'disabled-by-default-renderer.scheduler',
    'disabled-by-default-renderer.scheduler.debug',
    'disabled-by-default-sandbox',
    'disabled-by-default-sequence_manager',
    'disabled-by-default-sequence_manager.debug',
    'disabled-by-default-sequence_manager.verbose_snapshots',
    'disabled-by-default-skia',
    'disabled-by-default-skia.gpu',
    'disabled-by-default-skia.gpu.cache',
    'disabled-by-default-skia.shaders',
    'disabled-by-default-SyncFileSystem',
    'disabled-by-default-system_stats',
    'disabled-by-default-thread_pool_diagnostics',
    'disabled-by-default-toplevel.ipc',
    'disabled-by-default-user_action_samples',
    'disabled-by-default-v8.compile',
    'disabled-by-default-v8.cpu_profiler',
    'disabled-by-default-v8.gc',
    'disabled-by-default-v8.gc_stats',
    'disabled-by-default-v8.ic_stats',
    'disabled-by-default-v8.runtime',
    'disabled-by-default-v8.runtime_stats',
    'disabled-by-default-v8.runtime_stats_sampling',
    'disabled-by-default-v8.stack_trace',
    'disabled-by-default-v8.turbofan',
    'disabled-by-default-v8.wasm.detailed',
    'disabled-by-default-v8.wasm.turbofan',
    'disabled-by-default-video_and_image_capture',
    'disabled-by-default-viz.gpu_composite_time',
    'disabled-by-default-viz.debug.overlay_planes',
    'disabled-by-default-viz.hit_testing_flow',
    'disabled-by-default-viz.overdraw',
    'disabled-by-default-viz.quads',
    'disabled-by-default-viz.surface_id_flow',
    'disabled-by-default-viz.surface_lifetime',
    'disabled-by-default-viz.triangles',
    'disabled-by-default-webaudio.audionode',
    'disabled-by-default-webrtc',
    'disabled-by-default-worker.scheduler',
    'disabled-by-default-xr.debug',
  ];
}

export function createEmptyState(): State {
  return {
    version: STATE_VERSION,
    nextId: 0,
    nextNoteId: 1,  // 0 is reserved for ephemeral area marking.
    nextAreaId: 0,
    newEngineMode: 'USE_HTTP_RPC_IF_AVAILABLE',
    engines: {},
    traceTime: {...defaultTraceTime},
    tracks: {},
    aggregatePreferences: {},
    trackGroups: {},
    visibleTracks: [],
    pinnedTracks: [],
    scrollingTracks: [],
    areas: {},
    queries: {},
    metrics: {},
    permalink: {},
    notes: {},
    pivotTableConfig: {},
    pivotTable: {},

    recordConfig: createEmptyRecordConfig(),
    displayConfigAsPbtxt: false,
    lastLoadedConfigTitle: null,

    frontendLocalState: {
      omniboxState: {
        lastUpdate: 0,
        omnibox: '',
        mode: 'SEARCH',
      },

      visibleState: {
        ...defaultTraceTime,
        lastUpdate: 0,
        resolution: 0,
      },
    },

    logsPagination: {
      offset: 0,
      count: 0,
    },

    status: {msg: '', timestamp: 0},
    currentSelection: null,
    currentFlamegraphState: null,
    traceConversionInProgress: false,

    perfDebug: false,
    sidebarVisible: true,
    hoveredUtid: -1,
    hoveredPid: -1,
    hoveredLogsTimestamp: -1,
    hoveredNoteTimestamp: -1,
    highlightedSliceId: -1,
    focusedFlowIdLeft: -1,
    focusedFlowIdRight: -1,
    searchIndex: -1,

    recordingInProgress: false,
    recordingCancelled: false,
    extensionInstalled: false,
    recordingTarget: getDefaultRecordingTargets()[0],
    availableAdbDevices: [],

    updateChromeCategories: false,
    chromeCategories: undefined,
  };
}

export function getContainingTrackId(state: State, trackId: string): null|
    string {
  const track = state.tracks[trackId];
  if (!track) {
    return null;
  }
  const parentId = track.trackGroup;
  if (!parentId) {
    return null;
  }
  return parentId;
}
