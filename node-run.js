const fg = require("fast-glob");
const fs = require("fs");
const parseArgs = require("minimist");
const path = require("path");
const process = require("process");

function die() {
  console.error("");
  console.error("Usage:");
  console.error("  node node-run --run");
  console.error("  node node-run --generate-listing out-web/cts/listing.json");
  process.exit(1);
}

if (!fs.existsSync("out-node/")) {
  console.error("Must be run from repository root, after building out-node/");
  die();
}

const args = parseArgs(process.argv.slice(2));
let outFile;
if (args.hasOwnProperty("generate-listing")) {
  try {
    outFile = path.normalize(args["generate-listing"]);
  } catch (e) {
    console.error(e);
    die();
  }
}
let shouldRun = args.hasOwnProperty("run");
if (!outFile && !shouldRun) {
  die();
}

const prefix = "src/cts/";
const files = fg.sync(prefix + "**/*", {
  onlyFiles: false,
  markDirectories: true,
});

const listing = [];
const modules = {};
for (const entry of files) {
  const file = (typeof entry === "string") ? entry : entry.path;
  const f = file.substring(prefix.length);
  if (f.endsWith(".ts")) {
    const testPath = f.substring(0, f.length - 3);
    const mod = modules[testPath] = require("./out-node/cts/" + testPath);
    listing.push({
      path: testPath,
      description: mod.description.trim(),
    });
  } else if (f.endsWith("/README.txt")) {
    // ignore
  } else if (f.endsWith("/")) {
    const readme = file + "README.txt";
    if (fs.existsSync(readme)) {
      const description = fs.readFileSync(readme, "utf8").trim();
      listing.push({
        path: f,
        description,
      });
    }
  } else {
    console.error("Unrecognized file: " + file);
    process.exit(1);
  }
}

if (outFile) {
  fs.writeFileSync(outFile, JSON.stringify(listing, undefined, 2),
      (err) => { if (err) { throw err; } });
  process.exit(0);
}

if (shouldRun) {
  const { Logger } = require("./out-node/framework");
  const failed = [];
  const warned = [];
  (async () => {
    const log = new Logger();

    for (const {path, description} of listing) {
      console.error("");
      console.error(path, description);
      if (path.endsWith("/")) {
        continue;
      }

      const [tres, trec] = log.record(path);
      for (const {name, params, run} of modules[path].test.iterate(trec)) {
        const res = await run();
        if (res.status === "fail") {
          failed.push(res);
        }
        if (res.status === "warn") {
          warned.push(res);
        }
      }
    }
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
}
