const fs = require('fs');
const process = require('process');
global.fetch = require('node-fetch');
require("./js-to-ts.js");

const { loadTests } = require('../src/framework/listing');
const { Logger } = require('../src/framework/logger');
const { parseFilters } = require('../src/framework/url_query');

function usage(rc) {
  console.error('Usage:');
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

class ListingFetcherFS {
  constructor() {
    this.suites = new Map();
  }

  async get(outDir, suite) {
    let listing = this.suites.get(suite);
    if (listing) {
      return listing;
    }
    const listingPath = `${outDir}/${suite}/listing.json`;
    const fetched = await fs.promises.readFile(listingPath);
    const groups = JSON.parse(fetched);

    listing = { suite, groups };
    this.suites.set(suite, listing);
    return listing;
  }
}


(async () => {
  const filters = parseFilters(process.argv.slice(2));
  const listing = await loadTests('./out', filters, ListingFetcherFS);

  const log = new Logger();
  const entries = await Promise.all(Array.from(listing,
      ({ suite, path, node }) => node.then(node => ({ suite, path, node }))));

  const failed = [];
  const warned = [];

  // TODO: don't run all tests all at once
  const running = [];
  for (const entry of entries) {
    const { path, node: { group } } = entry;
    if (!group) {
      continue;
    }

    const [, rec] = log.record(path);
    for (const t of group.iterate(rec)) {
      running.push((async () => {
        const res = await t.run();
        if (res.status === "fail") {
          failed.push(res);
        }
        if (res.status === "warn") {
          warned.push(res);
        }
      })());
    }
  }

  await Promise.all(running);

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

  console.error("** Summary **");

  const total = running.length;
  const passed = total - warned.length - failed.length;
  function pct(x) {
    return (100 * x / total).toFixed(2);
  }
  function rpt(x) {
    const xs = x.toString().padStart(1 + Math.log10(total), ' ');
    return `${xs} / ${total} = ${pct(x).padStart(6, ' ')}%`;
  }
  console.error(`
Passed  w/o warnings = ${rpt(passed)}
Passed with warnings = ${rpt(warned.length)}
Failed               = ${rpt(failed.length)}`);

  if (failed.length || warned.length) {
    process.exit(1);
  }
})();
