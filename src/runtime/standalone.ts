import { TestLoader } from '../framework/loader.js';
import { Logger } from '../framework/logger.js';
import { makeQueryString } from '../framework/url_query.js';
import { RunCase } from '../framework/index.js';

const log = new Logger();

type runButtonCallback = () => Promise<void>;
const runButtonCallbacks = new Map<HTMLElement, runButtonCallback>();

const resultsJSON = document.getElementById('resultsJSON') as HTMLElement;
const resultsVis = document.getElementById('resultsVis') as HTMLElement;
function makeTest(path: string, description: string): HTMLElement {
  const test = $('<div>')
    .addClass('test')
    .appendTo(resultsVis);

  const testrun = $('<button>')
    .addClass('testrun')
    .html('&#9654;')
    .appendTo(test);

  $('<div>')
    .addClass('testname')
    .text(path)
    .appendTo(test);

  $('<div>')
    .addClass('testdesc')
    .text(description)
    .appendTo(test);

  const testcases = $('<div>')
    .addClass('testcases')
    .appendTo(test);

  testrun.on('click', async () => {
    for (const el of test.find('.caserun')) {
      const rc = runButtonCallbacks.get(el) as runButtonCallback;
      await rc();
    }
    resultsJSON.textContent = log.asJSON(2);
  });

  return testcases[0];
}

function mkCase(testcasesVis: HTMLElement, query: string, t: RunCase) {
  const testcase = $('<div>')
    .addClass('testcase')
    .appendTo(testcasesVis);
  const casehead = $('<div>')
    .addClass('casehead')
    .appendTo(testcase);

  const caserun = $('<button>')
    .addClass('caserun')
    .html('&#9654;')
    .appendTo(casehead);
  $('<div>')
    .addClass('casename')
    .appendTo(casehead)
    .text(query);
  const casetime = $('<div>')
    .addClass('casetime')
    .appendTo(casehead);

  const caselogs = $('<div>', { class: 'caselogs' }).appendTo(testcase);

  const runCase = async () => {
    const res = await t.run();

    casetime.text(res.timems.toFixed(4) + ' ms');

    testcase.removeClass('pass');
    testcase.removeClass('warn');
    testcase.removeClass('fail');
    testcase.addClass(res.status);

    if (res.logs) {
      caselogs.empty();
      for (const l of res.logs) {
        $('<div>', { class: 'caselog' })
          .appendTo(caselogs)
          .text(l);
      }
    }
  };
  runButtonCallbacks.set(caserun[0], runCase);
  caserun.on('click', async () => {
    await runCase();
    resultsJSON.textContent = log.asJSON(2);
  });
  return runCase;
}

(async () => {
  const url = new URL(window.location.toString());
  const runnow = url.searchParams.get('runnow') === '1';

  const loader = new TestLoader();
  const listing = await loader.loadTestsFromQuery(window.location.search);
  const entries = await Promise.all(
    Array.from(listing, ({ suite, path, spec }) => spec.then(s => ({ suite, path, spec: s })))
  );
  // TODO: convert listing to tree so it can be displayed as a tree?

  const runCaseList = [];
  for (const entry of entries) {
    const { path, spec } = entry;
    const testcasesVis = makeTest(path, spec.description);

    if (!spec.g) {
      continue;
    }

    const [tRec] = log.record(path);
    for (const t of spec.g.iterate(tRec)) {
      const query = makeQueryString(entry, t.id);
      const runCase = mkCase(testcasesVis, query, t);
      runCaseList.push(runCase);
    }
  }

  if (runnow) {
    for (const runCase of runCaseList) {
      await runCase();
    }
    resultsJSON.textContent = log.asJSON(2);
  }
})();
