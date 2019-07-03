import { TestLoader } from '../framework/loader.js';
import { Logger } from '../framework/logger.js';
import { makeQueryString } from '../framework/url_query.js';

declare interface WptTestObject {
  step(f: () => void): void;
  done(): void;
}
declare function async_test(f: (this: WptTestObject) => Promise<void>, name: string): void;

(async () => {
  const loader = new TestLoader();
  const listing = await loader.loadTestsFromQuery(window.location.search);

  const log = new Logger();
  const running = [];
  const entries = await Promise.all(
    Array.from(listing, ({ suite, path, spec }) => spec.then(s => ({ suite, path, spec: s })))
  );

  for (const entry of entries) {
    const { path, spec } = entry;
    if (!spec.g) {
      continue;
    }

    const [rec] = log.record(path);
    // TODO: don't run all tests all at once
    for (const t of spec.g.iterate(rec)) {
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
      }, makeQueryString(entry, t.id));
    }
  }

  await Promise.all(running);
  const resultsElem = document.getElementById('results') as HTMLElement;
  resultsElem.textContent = log.asJSON(2);
})();
