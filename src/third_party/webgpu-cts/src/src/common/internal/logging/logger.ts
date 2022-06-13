import { version } from '../version.js';

import { LiveTestCaseResult } from './result.js';
import { TestCaseRecorder } from './test_case_recorder.js';

export type LogResults = Map<string, LiveTestCaseResult>;

export class Logger {
  static globalDebugMode: boolean = false;

  readonly overriddenDebugMode: boolean | undefined;
  readonly results: LogResults = new Map();

  constructor({ overrideDebugMode }: { overrideDebugMode?: boolean } = {}) {
    this.overriddenDebugMode = overrideDebugMode;
  }

  record(name: string): [TestCaseRecorder, LiveTestCaseResult] {
    const result: LiveTestCaseResult = { status: 'running', timems: -1 };
    this.results.set(name, result);
    return [
      new TestCaseRecorder(result, this.overriddenDebugMode ?? Logger.globalDebugMode),
      result,
    ];
  }

  asJSON(space?: number): string {
    return JSON.stringify({ version, results: Array.from(this.results) }, undefined, space);
  }
}
