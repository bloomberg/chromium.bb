import { DefaultTestFileLoader } from '../../framework/file_loader.js';
import { Logger } from '../../framework/logging/logger.js';
import { parseQuery } from '../../framework/query/parseQuery.js';
import { assert } from '../../framework/util/util.js';

/* eslint-disable-next-line @typescript-eslint/no-explicit-any */
declare const self: any; // should be DedicatedWorkerGlobalScope

const loader = new DefaultTestFileLoader();

self.onmessage = async (ev: MessageEvent) => {
  const query: string = ev.data.query;
  const debug: boolean = ev.data.debug;

  const log = new Logger(debug);

  const testcases = Array.from(await loader.loadCases(parseQuery(query)));
  assert(testcases.length === 1, 'worker query resulted in != 1 cases');

  const testcase = testcases[0];
  const [rec, result] = log.record(testcase.query.toString());
  await testcase.run(rec);

  self.postMessage({ query, result });
};
