import { TestLoader } from '../framework/loader.js';
import { Logger } from '../framework/logger.js';
import { makeQueryString } from '../framework/url_query.js';

declare interface WptTestObject {
  step(f: () => void): void;
  done(): void;
}
// Implements the wpt-embedded test runner (see also: wpt/cts.html).

declare function async_test(f: (this: WptTestObject) => Promise<void>, name: string): void;

(async () => {
  const loader = new TestLoader();
  const listing = await loader.loadTestsFromQuery(window.location.search);

  const log = new Logger();
  const running = [];
  const queryResults = await Promise.all(
    Array.from(listing, ({ id, spec }) => spec.then(s => ({ id, spec: s })))
  );

  for (const qr of queryResults) {
    if (!('g' in qr.spec)) {
      continue;
    }

    const [rec] = log.record(qr.id);
    // TODO: don't run all tests all at once
    for (const t of qr.spec.g.iterate(rec)) {
      const run = t.run();
      running.push(run);
      // Note: apparently, async_tests must ALL be added within the same task.
      async_test(async function(this: WptTestObject) {
        const r = await run;
        this.step(() => {
          if (r.status === 'fail') {
            throw (r.logs || []).join('\n');
          }
        });
        this.done();
      }, makeQueryString(qr.id, t.id));
    }
  }

  await Promise.all(running);
  const resultsElem = document.getElementById('results') as HTMLElement;
  resultsElem.textContent = log.asJSON(2);
})();
