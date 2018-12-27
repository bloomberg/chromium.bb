const fg = require("fast-glob");
const fs = require("fs");
const parseArgs = require("minimist");
const path = require("path");
const process = require("process");

function die() {
  console.error("");
  console.error("Usage:");
  console.error("  node node-run");
  console.error("  node node-run --generate-listing out-web/cts/listing.json");
  process.exit(1);
}

if (!fs.existsSync("out-node/")) {
  console.error("Must be run from repository root, after building out-node/");
  die();
}

const args = parseArgs(process.argv.slice(2));
let outFile;
if (args["generate-listing"]) {
  try {
    outFile = path.normalize(args["generate-listing"]);
  } catch (e) {
    console.error(e);
    die();
  }
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
    const description = mod.description.trim();
    listing.push({
      path: testPath,
      description
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

for (const {path, description} of listing) {
  console.log(path, description);
  if (!path.endsWith("/")) {
    console.log(modules[path].add);
  }
}

// TODO: implement --run
