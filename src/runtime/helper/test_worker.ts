import { LiveTestCaseResult } from '../../framework/logger';

export class TestWorker {
  private worker: Worker;
  private resolvers = new Map<string, (result: LiveTestCaseResult) => void>();

  constructor() {
    this.worker = new Worker('out/runtime/helper/test_worker.worker.js', { type: 'module' });
    this.worker.onmessage = ev => {
      const { query, result } = ev.data;
      this.resolvers.get(query)!(result);

      // TODO(kainino0x): update the Logger with this result (or don't have a logger and update the
      // entire results JSON somehow at some point).
    };
  }

  run(query: string, debug: boolean = false): Promise<LiveTestCaseResult> {
    this.worker.postMessage({ query, debug });
    return new Promise(resolve => {
      this.resolvers.set(query, resolve);
    });
  }
}
