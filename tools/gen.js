const fg = require("fast-glob");
const fs = require("fs");
const path = require("path");
const process = require("process");
global.fetch = require("node-fetch");

function die() {
  console.error(new Error());
  console.error("");
  console.error("Usage:");
  console.error("  node tools/gen [SUITES...]");
  console.error("  node tools/gen cts unittests demos");
  process.exit(1);
}

if (!fs.existsSync("src/")) {
  console.error("Must be run from repository root");
  die();
}

for (const suite of process.argv.slice(2)) {
  const outFile = path.normalize(`out/${suite}/listing.json`);
  const specDir = path.normalize(`src/${suite}/`); // always ends in /

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
    const f = file.substring(specDir.length);
    if (f.endsWith(specSuffix)) {
      const mod = require("../" + file);
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

  fs.writeFileSync(outFile, JSON.stringify(listing, undefined, 2),
      (err) => { if (err) { throw err; } });
}
