const fs = require('fs');
const process = require('process');
global.fetch = require('node-fetch');

const { loadTests } = require('../src/framework/listing');
const { Logger } = require('../src/framework/logger');
const { parseFilters } = require('../src/framework/url_query');

function usage(rc) {
  console.error("Usage:");
  console.error('  node tools/run [FILTERS...]');
  console.error('  node tools/run unittests: demos:params:');
  process.exit(rc);
}

function die() {
  console.error(new Error());
  console.error("");
  usage(1);
}

if (process.argv.length <= 2) {
  usage(0);
}

if (!fs.existsSync("src/")) {
  console.error("Must be run from repository root");
  die();
}

(async () => {
  const filters = parseFilters(process.argv.slice(1));
  const listing = await loadTests('./webgpu-cts/out', filters);

  const log = new Logger();
  const entries = await Promise.all(Array.from(listing,
      ({ suite, path, node }) => node.then(node => ({ suite, path, node }))));

  // TODO: don't run all tests all at once
  const running = [];
  for (const entry of entries) {
    const { path, node: { group } } = entry;
    if (!group) {
      continue;
    }

    const [, rec] = log.record(path);
    for (const t of group.iterate(rec)) {
      running.push(t.run());
    }
  }

  await Promise.all(running);
  console.log(JSON.stringify(log.results, undefined, 2));
})();




const failed = [];
const warned = [];
(async () => {
  const log = new Logger();

  for (const {path, description} of listing) {
    if (path.endsWith("/")) {
      continue;
    }

    const [tres, trec] = log.record(path);
    for (const {name, params, run} of modules[path].group.iterate(trec)) {
      const res = await run();
      if (res.status === "fail") {
        failed.push(res);
      }
      if (res.status === "warn") {
        warned.push(res);
      }
    }
  }
  console.error("** Results **");
  console.log(JSON.stringify(log.results, undefined, 2));

  if (warned.length) {
    console.error("");
    console.error("** Warnings **");
    for (const r of warned) {
      console.error(r);
    }
  }
  if (failed.length) {
    console.error("");
    console.error("** Failures **");
    for (const r of failed) {
      console.error(r);
    }
  }
})();
