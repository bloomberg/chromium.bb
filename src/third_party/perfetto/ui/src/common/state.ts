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

/**
 * A plain js object, holding objects of type |Class| keyed by string id.
 * We use this instead of using |Map| object since it is simpler and faster to
 * serialize for use in postMessage.
 */
export interface ObjectById<Class extends{id: string}> { [id: string]: Class; }

export const SCROLLING_TRACK_GROUP = 'ScrollingTracks';

export interface TrackState {
  id: string;
  engineId: string;
  kind: string;
  name: string;
  trackGroup?: string;
  dataReq?: TrackDataRequest;
  config: {};
}

export interface TrackGroupState {
  id: string;
  engineId: string;
  name: string;
  collapsed: boolean;
  tracks: string[];  // Child track ids.
  summaryTrackId: string;
}

export interface TrackDataRequest {
  start: number;
  end: number;
  resolution: number;
}

export interface EngineConfig {
  id: string;
  ready: boolean;
  source: string|File;
}

export interface QueryConfig {
  id: string;
  engineId: string;
  query: string;
}

export interface PermalinkConfig {
  requestId?: string;  // Set by the frontend to request a new permalink.
  hash?: string;       // Set by the controller when the link has been created.
}

export interface RecordConfig {
  [key: string]: null|number|boolean|string|string[];

  // Global settings
  durationSeconds: number;
  writeIntoFile: boolean;
  fileWritePeriodMs: number|null;

  // Buffer setup
  bufferSizeMb: number;

  // Ftrace
  ftrace: boolean;
  ftraceEvents: string[];
  atraceCategories: string[];
  atraceApps: string[];
  ftraceDrainPeriodMs: number|null;
  ftraceBufferSizeKb: number|null;

  // Ps
  processMetadata: boolean;
  scanAllProcessesOnStart: boolean;
  procStatusPeriodMs: number|null;

  // SysStats
  sysStats: boolean;
  meminfoPeriodMs: number|null;
  meminfoCounters: string[];
  vmstatPeriodMs: number|null;
  vmstatCounters: string[];
  statPeriodMs: number|null;
  statCounters: string[];
}

export interface TraceTime {
  startSec: number;
  endSec: number;
  lastUpdate: number;  // Epoch in seconds (Date.now() / 1000).
}

export interface Status {
  msg: string;
  timestamp: number;  // Epoch in seconds (Date.now() / 1000).
}

export interface State {
  route: string|null;
  nextId: number;

  /**
   * State of the ConfigEditor.
   */
  recordConfig: RecordConfig;
  displayConfigAsPbtxt: boolean;

  /**
   * Open traces.
   */
  engines: ObjectById<EngineConfig>;
  traceTime: TraceTime;
  visibleTraceTime: TraceTime;
  trackGroups: ObjectById<TrackGroupState>;
  tracks: ObjectById<TrackState>;
  scrollingTracks: string[];
  pinnedTracks: string[];
  queries: ObjectById<QueryConfig>;
  permalink: PermalinkConfig;
  status: Status;
}

export const defaultTraceTime = {
  startSec: 0,
  endSec: 10,
  lastUpdate: 0
};

export function createEmptyState(): State {
  return {
    route: null,
    nextId: 0,
    engines: {},
    traceTime: {...defaultTraceTime},
    visibleTraceTime: {...defaultTraceTime},
    tracks: {},
    trackGroups: {},
    pinnedTracks: [],
    scrollingTracks: [],
    queries: {},
    permalink: {},

    recordConfig: createEmptyRecordConfig(),
    displayConfigAsPbtxt: false,

    status: {msg: '', timestamp: 0},
  };
}

export function createEmptyRecordConfig(): RecordConfig {
  return {
    durationSeconds: 10.0,
    writeIntoFile: false,
    fileWritePeriodMs: null,
    bufferSizeMb: 10.0,

    ftrace: false,
    ftraceEvents: [],
    atraceApps: [],
    atraceCategories: [],
    ftraceDrainPeriodMs: null,
    ftraceBufferSizeKb: null,

    processMetadata: false,
    scanAllProcessesOnStart: false,
    procStatusPeriodMs: null,

    sysStats: false,
    meminfoPeriodMs: null,
    meminfoCounters: [],
    vmstatPeriodMs: null,
    vmstatCounters: [],
    statPeriodMs: null,
    statCounters: [],
  };
}
