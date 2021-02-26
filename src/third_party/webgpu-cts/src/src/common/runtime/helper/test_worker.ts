import { LogMessageWithStack } from '../../framework/logging/log_message.js';
import { TransferredTestCaseResult, LiveTestCaseResult } from '../../framework/logging/result.js';
import { TestCaseRecorder } from '../../framework/logging/test_case_recorder.js';

export class TestWorker {
  private readonly debug: boolean;
  private readonly worker: Worker;
  private readonly resolvers = new Map<string, (result: LiveTestCaseResult) => void>();

  constructor(debug: boolean) {
    this.debug = debug;

    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    this.worker = new Worker(workerPath, { type: 'module' });
    this.worker.onmessage = ev => {
      const query: string = ev.data.query;
      const result: TransferredTestCaseResult = ev.data.result;
      if (result.logs) {
        for (const l of result.logs) {
          Object.setPrototypeOf(l, LogMessageWithStack.prototype);
        }
      }
      this.resolvers.get(query)!(result as LiveTestCaseResult);

      // TODO(kainino0x): update the Logger with this result (or don't have a logger and update the
      // entire results JSON somehow at some point).
    };
  }

  async run(rec: TestCaseRecorder, query: string): Promise<void> {
    this.worker.postMessage({ query, debug: this.debug });
    const workerResult = await new Promise<LiveTestCaseResult>(resolve => {
      this.resolvers.set(query, resolve);
    });
    rec.injectResult(workerResult);
  }
}
