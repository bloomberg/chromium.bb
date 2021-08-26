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

import * as protos from '../gen/protos';

// Aliases protos to avoid the super nested namespaces.
// See https://www.typescriptlang.org/docs/handbook/namespaces.html#aliases
import AndroidLogConfig = protos.perfetto.protos.AndroidLogConfig;
import AndroidPowerConfig = protos.perfetto.protos.AndroidPowerConfig;
import AndroidLogId = protos.perfetto.protos.AndroidLogId;
import BatteryCounters =
    protos.perfetto.protos.AndroidPowerConfig.BatteryCounters;
import BufferConfig = protos.perfetto.protos.TraceConfig.BufferConfig;
import ChromeConfig = protos.perfetto.protos.ChromeConfig;
import ConsumerPort = protos.perfetto.protos.ConsumerPort;
import NativeContinuousDumpConfig =
    protos.perfetto.protos.HeapprofdConfig.ContinuousDumpConfig;
import JavaContinuousDumpConfig =
    protos.perfetto.protos.JavaHprofConfig.ContinuousDumpConfig;
import DataSourceConfig = protos.perfetto.protos.DataSourceConfig;
import FtraceConfig = protos.perfetto.protos.FtraceConfig;
import HeapprofdConfig = protos.perfetto.protos.HeapprofdConfig;
import JavaHprofConfig = protos.perfetto.protos.JavaHprofConfig;
import IAndroidPowerConfig = protos.perfetto.protos.IAndroidPowerConfig;
import IBufferConfig = protos.perfetto.protos.TraceConfig.IBufferConfig;
import IProcessStatsConfig = protos.perfetto.protos.IProcessStatsConfig;
import ISysStatsConfig = protos.perfetto.protos.ISysStatsConfig;
import ITraceConfig = protos.perfetto.protos.ITraceConfig;
import MeminfoCounters = protos.perfetto.protos.MeminfoCounters;
import ProcessStatsConfig = protos.perfetto.protos.ProcessStatsConfig;
import StatCounters = protos.perfetto.protos.SysStatsConfig.StatCounters;
import SysStatsConfig = protos.perfetto.protos.SysStatsConfig;
import TraceConfig = protos.perfetto.protos.TraceConfig;
import VmstatCounters = protos.perfetto.protos.VmstatCounters;

// Trace Processor protos.
import QueryArgs = protos.perfetto.protos.QueryArgs;
import StatusResult = protos.perfetto.protos.StatusResult;
import ComputeMetricArgs = protos.perfetto.protos.ComputeMetricArgs;
import ComputeMetricResult = protos.perfetto.protos.ComputeMetricResult;

export {
  AndroidLogConfig,
  AndroidLogId,
  AndroidPowerConfig,
  BatteryCounters,
  BufferConfig,
  ChromeConfig,
  ConsumerPort,
  ComputeMetricArgs,
  ComputeMetricResult,
  DataSourceConfig,
  FtraceConfig,
  HeapprofdConfig,
  IAndroidPowerConfig,
  IBufferConfig,
  IProcessStatsConfig,
  ISysStatsConfig,
  ITraceConfig,
  JavaContinuousDumpConfig,
  JavaHprofConfig,
  MeminfoCounters,
  NativeContinuousDumpConfig,
  ProcessStatsConfig,
  QueryArgs,
  StatCounters,
  StatusResult,
  SysStatsConfig,
  TraceConfig,
  VmstatCounters,
};
