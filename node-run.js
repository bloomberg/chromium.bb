const fg = require("fast-glob");
const fs = require("fs");
const parseArgs = require("minimist");
const path = require("path");
const process = require("process");

function die() {
  console.error(new Error());
  console.error("");
  console.error("Usage:");
  console.error("  node node-run src/cts --run");
  console.error("  node node-run src/cts --generate-listing=out/cts/listing.json");
  process.exit(1);
}

if (!fs.existsSync("src/")) {
  console.error("Must be run from repository root");
  die();
}

const args = parseArgs(process.argv.slice(2), { boolean: true });
let shouldGenerate = args.hasOwnProperty("generate-listing");
let outFile;
if (shouldGenerate) {
  try {
    outFile = path.normalize(args["generate-listing"]);
  } catch (e) {
    console.error(e);
    die();
  }
}
let shouldRun = args.hasOwnProperty("run");
if (!(shouldGenerate ^ shouldRun)) {
  die();
}
if (args._.length !== 1) {
  die();
}
const specDir = path.normalize(args._[0] + "/"); // always ends in /

const specSuffix = ".spec.ts";
const specFiles = fg.sync(specDir + "**/{README.txt,*" + specSuffix + "}", {
  onlyFiles: false,
  markDirectories: true,
});

// Redirect imports of .js files to .ts files
{
  const Module = require("module");
  const resolveFilename = Module._resolveFilename;
  Module._resolveFilename = (request, parentModule, isMain) => {
    if (parentModule.filename.endsWith(".ts") && request.endsWith(".js")) {
      request = request.substring(0, request.length - ".ts".length) + ".ts";
    }
    return resolveFilename.call(this, request, parentModule, isMain);
  };
}

const listing = [];
const modules = {};
for (const spec of specFiles) {
  const file = (typeof spec === "string") ? spec : spec.path;
  const f = file.substring(specDir.length - 1);
  if (f.endsWith(specSuffix)) {
    const mod = require("./" + file);
    const testPath = f.substring(0, f.length - specSuffix.length);
    modules[testPath] = mod;
    listing.push({
      path: testPath,
      description: mod.description.trim(),
    });
  } else if (path.basename(file) === "README.txt") {
    const readme = file;
    if (fs.existsSync(readme)) {
      const path = f.substring(0, f.length - "README.txt".length);
      const description = fs.readFileSync(readme, "utf8").trim();
      listing.push({
        path,
        description,
      });
    }
    // ignore
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
  console.error("** Tests **");
  console.error(JSON.stringify(listing, undefined, 2));

  const { Logger } = require("./src/framework");
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
}
