// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

'use strict';

// This script takes care of:
// - The build process for the whole UI and the chrome extension.
// - The HTTP dev-server with live-reload capabilities.
// The reason why this is a hand-rolled script rather than a conventional build
// system is keeping incremental build fast and maintaining the set of
// dependencies contained.
// The only way to keep incremental build fast (i.e. O(seconds) for the
// edit-one-line -> reload html cycles) is to run both the TypeScript compiler
// and the rollup bundler in --watch mode. Any other attempt, leads to O(10s)
// incremental-build times.
// This script allows mixing build tools that support --watch mode (tsc and
// rollup) and auto-triggering-on-file-change rules via node-watch.
// When invoked without any argument (e.g., for production builds), this script
// just runs all the build tasks serially. It doesn't to do any mtime-based
// check, it always re-runs all the tasks.
// When invoked with --watch, it mounts a pipeline of tasks based on node-watch
// and runs them together with tsc --watch and rollup --watch.
// The output directory structure is carefully crafted so that any change to UI
// sources causes cascading triggers of the next steps.
// The overall build graph looks as follows:
// +----------------+      +-----------------------------+
// | protos/*.proto |----->| pbjs out/tsc/gen/protos.js  |--+
// +----------------+      +-----------------------------+  |
//                         +-----------------------------+  |
//                         | pbts out/tsc/gen/protos.d.ts|<-+
//                         +-----------------------------+
//                             |
//                             V      +-------------------------+
// +---------+              +-----+   |  out/tsc/frontend/*.js  |
// | ui/*.ts |------------->| tsc |-> +-------------------------+   +--------+
// +---------+              +-----+   | out/tsc/controller/*.js |-->| rollup |
//                            ^       +-------------------------+   +--------+
//                +------------+      |   out/tsc/engine/*.js   |       |
// +-----------+  |*.wasm.js   |      +-------------------------+       |
// |ninja *.cc |->|*.wasm.d.ts |                                        |
// +-----------+  |*.wasm      |-----------------+                      |
//                +------------+                 |                      |
//                                               V                      V
// +-----------+  +------+    +------------------------------------------------+
// | ui/*.scss |->| scss |--->|              Final out/dist/ dir               |
// +-----------+  +------+    +------------------------------------------------+
// +----------------------+   | +----------+ +---------+ +--------------------+|
// | src/assets/*.png     |   | | assets/  | |*.wasm.js| | frontend_bundle.js ||
// +----------------------+   | |  *.css   | |*.wasm   | +--------------------+|
// | buildtools/typefaces |-->| |  *.png   | +---------+ |controller_bundle.js||
// +----------------------+   | |  *.woff2 |             +--------------------+|
// | buildtools/legacy_tv |   | |  tv.html |             |  engine_bundle.js  ||
// +----------------------+   | +----------+             +--------------------+|
//                            +------------------------------------------------+

const argparse = require('argparse');
const child_process = require('child_process');
var crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const path = require('path');
const fswatch = require('node-watch');  // Like fs.watch(), but works on Linux.
const pjoin = path.join;

const ROOT_DIR = path.dirname(__dirname);  // The repo root.
const VERSION_SCRIPT = pjoin(ROOT_DIR, 'tools/write_version_header.py');

const cfg = {
  watch: false,
  verbose: false,
  debug: false,
  startHttpServer: false,
  wasmModules: ['trace_processor', 'trace_to_text'],
  testConfigs: ['jest.unit.config.js'],

  // The fields below will be changed by main() after cmdline parsing.
  // Directory structure:
  // out/xxx/    -> outDir         : Root build dir, for both ninja/wasm and UI.
  //   ui/       -> outUiDir       : UI dir. All outputs from this script.
  //    tsc/     -> outTscDir      : Transpiled .ts -> .js.
  //      gen/   -> outGenDir      : Auto-generated .ts/.js (e.g. protos).
  //    dist/    -> outDistRootDir : Only index.html and service_worker.js
  //      v1.2/  -> outDistDir     : JS bundles and assets
  //    chrome_extension/          : Chrome extension.
  outDir: pjoin(ROOT_DIR, 'out/ui'),
  version: '',  // v1.2.3, derived from the CHANGELOG + git.
  outUiDir: '',
  outDistRootDir: '',
  outTscDir: '',
  outGenDir: '',
  outDistRootDir: '',
  outDistDir: '',
  outExtDir: '',
};

const RULES = [
  {r: /ui\/src\/assets\/index.html/, f: copyIndexHtml},
  {r: /ui\/src\/assets\/((.*)[.]png)/, f: copyAssets},
  {r: /buildtools\/typefaces\/(.+[.]woff2)/, f: copyAssets},
  {r: /buildtools\/catapult_trace_viewer\/(.+(js|html))/, f: copyAssets},
  {r: /ui\/src\/assets\/.+[.]scss/, f: compileScss},
  {r: /ui\/src\/assets\/.+[.]scss/, f: compileScss},
  {r: /ui\/src\/chrome_extension\/.*/, f: copyExtensionAssets},
  {r: /.*\/dist\/.+\/(?!manifest\.json).*/, f: genServiceWorkerManifestJson},
  {r: /.*\/dist\/.*/, f: notifyLiveServer},
];

let tasks = [];
let tasksTot = 0, tasksRan = 0;
let serverStarted = false;
let httpWatches = [];
let tStart = Date.now();
let subprocesses = [];

function main() {
  const parser = new argparse.ArgumentParser();
  parser.addArgument('--out', {help: 'Output directory'});
  parser.addArgument(['--watch', '-w'], {action: 'storeTrue'});
  parser.addArgument(['--serve', '-s'], {action: 'storeTrue'});
  parser.addArgument(['--verbose', '-v'], {action: 'storeTrue'});
  parser.addArgument(['--no-build', '-n'], {action: 'storeTrue'});
  parser.addArgument(['--no-wasm', '-W'], {action: 'storeTrue'});
  parser.addArgument(['--run-tests', '-t'], {action: 'storeTrue'});
  parser.addArgument(['--debug', '-d'], {action: 'storeTrue'});

  const args = parser.parseArgs();
  const clean = !args.no_build;
  cfg.outDir = path.resolve(ensureDir(args.out || cfg.outDir));
  cfg.outUiDir = ensureDir(pjoin(cfg.outDir, 'ui'), clean);
  cfg.outExtDir = ensureDir(pjoin(cfg.outUiDir, 'chrome_extension'));
  cfg.outDistRootDir = ensureDir(pjoin(cfg.outUiDir, 'dist'));
  const proc = exec('python3', [VERSION_SCRIPT, '--stdout'], {stdout: 'pipe'});
  cfg.version = proc.stdout.toString().trim();
  cfg.outDistDir = ensureDir(pjoin(cfg.outDistRootDir, cfg.version));
  cfg.outTscDir = ensureDir(pjoin(cfg.outUiDir, 'tsc'));
  cfg.outGenDir = ensureDir(pjoin(cfg.outUiDir, 'tsc/gen'));
  cfg.watch = !!args.watch;
  cfg.verbose = !!args.verbose;
  cfg.debug = !!args.debug;
  cfg.startHttpServer = args.serve;

  process.on('SIGINT', () => {
    console.log('\nSIGINT received. Killing all child processes and exiting');
    for (const proc of subprocesses) {
      if (proc) proc.kill('SIGINT');
    }
    process.exit(130);  // 130 -> Same behavior of bash when killed by SIGINT.
  });

  // Check that deps are current before starting.
  const installBuildDeps = pjoin(ROOT_DIR, 'tools/install-build-deps');
  const depsArgs = ['--check-only', pjoin(cfg.outDir, '.check_deps'), '--ui'];
  exec(installBuildDeps, depsArgs);

  console.log('Entering', cfg.outDir);
  process.chdir(cfg.outDir);

  updateSymlinks();  // Links //ui/out -> //out/xxx/ui/

  // Enqueue empty task. This is needed only for --no-build --serve. The HTTP
  // server is started when the task queue reaches quiescence, but it takes at
  // least one task for that.
  addTask(() => {});

  if (!args.no_build) {
    buildWasm(args.no_wasm);
    scanDir('ui/src/assets');
    scanDir('ui/src/chrome_extension');
    scanDir('buildtools/typefaces');
    scanDir('buildtools/catapult_trace_viewer');
    compileProtos();
    genVersion();
    transpileTsProject('ui');
    transpileTsProject('ui/src/service_worker');
    bundleJs('rollup.config.js');
    genServiceWorkerManifestJson();

    // Watches the /dist. When changed:
    // - Notifies the HTTP live reload clients.
    // - Regenerates the ServiceWorker file map.
    scanDir(cfg.outDistRootDir);
  }

  if (args.run_tests) {
    runTests();
  }
}

// -----------
// Build rules
// -----------

function runTests() {
  const args =
      ['--rootDir', cfg.outTscDir, '--verbose', '--runInBand', '--forceExit'];
  for (const cfgFile of cfg.testConfigs) {
    args.push('--projects', pjoin(ROOT_DIR, 'ui/config', cfgFile));
  }
  if (cfg.watch) {
    args.push('--watchAll');
    addTask(execNode, ['jest', args, {async: true}]);
  } else {
    addTask(execNode, ['jest', args]);
  }
}

function copyIndexHtml(src) {
  const index_html = () => {
    let html = fs.readFileSync(src).toString();
    // First copy the index.html as-is into the dist/v1.2.3/ directory. This is
    // only used for archival purporses, so one can open
    // ui.perfetto.dev/v1.2.3/ to skip the auto-update and channel logic.
    fs.writeFileSync(pjoin(cfg.outDistDir, 'index.html'), html);

    // Then copy it into the dist/ root by patching the version code.
    // TODO(primiano): in next CLs, this script should take a
    // --release_map=xxx.json argument, to populate this with multiple channels.
    const versionMap = JSON.stringify({'stable': cfg.version});
    const bodyRegex = /data-perfetto_version='[^']*'/;
    html = html.replace(bodyRegex, `data-perfetto_version='${versionMap}'`);
    fs.writeFileSync(pjoin(cfg.outDistRootDir, 'index.html'), html);
  };
  addTask(index_html);
}

function copyAssets(src, dst) {
  addTask(cp, [src, pjoin(cfg.outDistDir, 'assets', dst)]);
}

function compileScss() {
  const src = pjoin(ROOT_DIR, 'ui/src/assets/perfetto.scss');
  const dst = pjoin(cfg.outDistDir, 'perfetto.css');
  // In watch mode, don't exit(1) if scss fails. It can easily happen by
  // having a typo in the css. It will still print an errror.
  const noErrCheck = !!cfg.watch;
  addTask(execNode, ['node-sass', ['--quiet', src, dst], {noErrCheck}]);
}

function compileProtos() {
  const dstJs = pjoin(cfg.outGenDir, 'protos.js');
  const dstTs = pjoin(cfg.outGenDir, 'protos.d.ts');
  const inputs = [
    'protos/perfetto/trace_processor/trace_processor.proto',
    'protos/perfetto/common/trace_stats.proto',
    'protos/perfetto/common/tracing_service_capabilities.proto',
    'protos/perfetto/config/perfetto_config.proto',
    'protos/perfetto/ipc/consumer_port.proto',
    'protos/perfetto/ipc/wire_protocol.proto',
    'protos/perfetto/metrics/metrics.proto',
  ];
  const pbjsArgs = [
    '--force-number',
    '-t',
    'static-module',
    '-w',
    'commonjs',
    '-p',
    ROOT_DIR,
    '-o',
    dstJs
  ].concat(inputs);
  addTask(execNode, ['pbjs', pbjsArgs]);
  const pbtsArgs = ['-p', ROOT_DIR, '-o', dstTs, dstJs];
  addTask(execNode, ['pbts', pbtsArgs]);
}

// Generates a .ts source that defines the VERSION and SCM_REVISION constants.
function genVersion() {
  const cmd = 'python3';
  const args =
      [VERSION_SCRIPT, '--ts_out', pjoin(cfg.outGenDir, 'perfetto_version.ts')]
  addTask(exec, [cmd, args]);
}

function updateSymlinks() {
  mklink(cfg.outUiDir, pjoin(ROOT_DIR, 'ui/out'));

  // Creates a out/dist_version -> out/dist/v1.2.3 symlink, so rollup config
  // can point to that without having to know the current version number.
  mklink(
      path.relative(cfg.outUiDir, cfg.outDistDir),
      pjoin(cfg.outUiDir, 'dist_version'));

  mklink(
      pjoin(ROOT_DIR, 'ui/node_modules'), pjoin(cfg.outTscDir, 'node_modules'))
}

// Invokes ninja for building the {trace_processor, trace_to_text} Wasm modules.
// It copies the .wasm directly into the out/dist/ dir, and the .js/.ts into
// out/tsc/, so the typescript compiler and the bundler can pick them up.
function buildWasm(skipWasmBuild) {
  if (!skipWasmBuild) {
    const gnArgs = ['gen', `--args=is_debug=${cfg.debug}`, cfg.outDir];
    addTask(exec, [pjoin(ROOT_DIR, 'tools/gn'), gnArgs]);

    const ninjaArgs = ['-C', cfg.outDir]
    ninjaArgs.push(...cfg.wasmModules.map(x => `${x}_wasm`));
    addTask(exec, [pjoin(ROOT_DIR, 'tools/ninja'), ninjaArgs]);
  }

  const wasmOutDir = pjoin(cfg.outDir, 'wasm');
  for (const wasmMod of cfg.wasmModules) {
    // The .wasm file goes directly into the dist dir (also .map in debug)
    for (const ext of ['.wasm'].concat(cfg.debug ? ['.wasm.map'] : [])) {
      const src = `${wasmOutDir}/${wasmMod}${ext}`;
      addTask(cp, [src, pjoin(cfg.outDistDir, wasmMod + ext)]);
    }
    // The .js / .ts go into intermediates, they will be bundled by rollup.
    for (const ext of ['.js', '.d.ts']) {
      const fname = `${wasmMod}${ext}`;
      addTask(cp, [pjoin(wasmOutDir, fname), pjoin(cfg.outGenDir, fname)]);
    }
  }
}

// This transpiles all the sources (frontend, controller, engine, extension) in
// one go. The only project that has a dedicated invocation is service_worker.
function transpileTsProject(project) {
  const args = ['--project', pjoin(ROOT_DIR, project)];
  if (cfg.watch) {
    args.push('--watch', '--preserveWatchOutput');
    addTask(execNode, ['tsc', args, {async: true}]);
  } else {
    addTask(execNode, ['tsc', args]);
  }
}

// Creates the three {frontend, controller, engine}_bundle.js in one invocation.
function bundleJs(cfgName) {
  const rcfg = pjoin(ROOT_DIR, 'ui/config', cfgName)
  const args = ['-c', rcfg, '--no-indent'];
  args.push(...(cfg.verbose ? [] : ['--silent']));
  if (cfg.watch) {
    // --waitForBundleInput is so that we can run tsc --watch and rollup --watch
    // together, without having to wait that tsc completes the first build.
    args.push('--watch', '--waitForBundleInput', '--no-watch.clearScreen');
    addTask(execNode, ['rollup', args, {async: true}]);
  } else {
    addTask(execNode, ['rollup', args]);
  }
}

function genServiceWorkerManifestJson() {
  function make_manifest() {
    const manifest = {resources: {}};
    // When building the subresource manifest skip source maps, the manifest
    // itself and the copy of the index.html which is copied under /v1.2.3/.
    // The root /index.html will be fetched by service_worker.js separately.
    const skipRegex = /(\.map|manifest\.json|index.html)$/;
    walk(cfg.outDistDir, absPath => {
      const contents = fs.readFileSync(absPath);
      const relPath = path.relative(cfg.outDistDir, absPath);
      const b64 = crypto.createHash('sha256').update(contents).digest('base64');
      manifest.resources[relPath] = 'sha256-' + b64;
    }, skipRegex);
    const manifestJson = JSON.stringify(manifest, null, 2);
    fs.writeFileSync(pjoin(cfg.outDistDir, 'manifest.json'), manifestJson);
  }
  addTask(make_manifest, []);
}

function startServer() {
  const port = 10000;
  console.log(`Starting HTTP server on http://localhost:${port}`)
  http.createServer(function(req, res) {
        console.debug(req.method, req.url);
        let uri = req.url.split('?', 1)[0];
        if (uri.endsWith('/')) {
          uri += 'index.html';
        }

        if (uri === '/live_reload') {
          // Implements the Server-Side-Events protocol.
          const head = {
            'Content-Type': 'text/event-stream',
            'Connection': 'keep-alive',
            'Cache-Control': 'no-cache'
          };
          res.writeHead(200, head);
          const arrayIdx = httpWatches.length;
          // We never remove from the array, the delete leaves an undefined item
          // around. It makes keeping track of the index easier at the cost of a
          // small leak.
          httpWatches.push(res);
          req.on('close', () => delete httpWatches[arrayIdx]);
          return;
        }

        const absPath = path.normalize(path.join(cfg.outDistRootDir, uri));
        fs.readFile(absPath, function(err, data) {
          if (err) {
            res.writeHead(404);
            res.end(JSON.stringify(err));
            return;
          }

          const mimeMap = {
            'html': 'text/html',
            'css': 'text/css',
            'js': 'application/javascript',
            'wasm': 'application/wasm',
          };
          const ext = uri.split('.').pop();
          const cType = mimeMap[ext] || 'octect/stream';
          const head = {
            'Content-Type': cType,
            'Content-Length': data.length,
            'Last-Modified': fs.statSync(absPath).mtime.toUTCString(),
            'Cache-Control': 'no-cache',
          };
          res.writeHead(200, head);
          res.write(data);
          res.end();
        });
      })
      .listen(port, '127.0.0.1');
}

// Called whenever a change in the out/dist directory is detected. It sends a
// Server-Side-Event to the live_reload.ts script.
function notifyLiveServer(changedFile) {
  for (const cli of httpWatches) {
    if (cli === undefined) continue;
    cli.write(
        'data: ' + path.relative(cfg.outDistRootDir, changedFile) + '\n\n');
  }
}

function copyExtensionAssets() {
  addTask(cp, [
    pjoin(ROOT_DIR, 'ui/src/assets/logo-128.png'),
    pjoin(cfg.outExtDir, 'logo-128.png')
  ]);
  addTask(cp, [
    pjoin(ROOT_DIR, 'ui/src/chrome_extension/manifest.json'),
    pjoin(cfg.outExtDir, 'manifest.json')
  ]);
}

// -----------------------
// Task chaining functions
// -----------------------

function addTask(func, args) {
  const task = new Task(func, args);
  for (const t of tasks) {
    if (t.identity === task.identity) {
      return;
    }
  }
  tasks.push(task);
  setTimeout(runTasks, 0);
}

function runTasks() {
  const snapTasks = tasks.splice(0);  // snap = std::move(tasks).
  tasksTot += snapTasks.length;
  for (const task of snapTasks) {
    const DIM = '\u001b[2m';
    const BRT = '\u001b[37m';
    const RST = '\u001b[0m';
    const ms = (new Date(Date.now() - tStart)).toISOString().slice(17, -1);
    const ts = `[${DIM}${ms}${RST}]`;
    const descr = task.description.substr(0, 80);
    console.log(`${ts} ${BRT}${++tasksRan}/${tasksTot}${RST}\t${descr}`);
    task.func.apply(/*this=*/ undefined, task.args);
  }
  // Start the web server once reaching quiescence.
  if (tasks.length === 0 && !serverStarted && cfg.startHttpServer) {
    serverStarted = true;
    startServer();
  }
}

// Executes all the RULES that match the given |absPath|.
function scanFile(absPath) {
  console.assert(fs.existsSync(absPath));
  console.assert(path.isAbsolute(absPath));
  const normPath = path.relative(ROOT_DIR, absPath);
  for (const rule of RULES) {
    const match = rule.r.exec(normPath);
    if (!match || match[0] !== normPath) continue;
    const captureGroup = match.length > 1 ? match[1] : undefined;
    rule.f(absPath, captureGroup);
  }
}

// Walks the passed |dir| recursively and, for each file, invokes the matching
// RULES. If --watch is used, it also installs a fswatch() and re-triggers the
// matching RULES on each file change.
function scanDir(dir, regex) {
  const filterFn = regex ? absPath => regex.test(absPath) : () => true;
  const absDir = path.isAbsolute(dir) ? dir : pjoin(ROOT_DIR, dir);
  // Add a fs watch if in watch mode.
  if (cfg.watch) {
    fswatch(absDir, {recursive: true}, (_eventType, filePath) => {
      if (!filterFn(filePath)) return;
      if (cfg.verbose) {
        console.log('File change detected', _eventType, filePath);
      }
      if (fs.existsSync(filePath)) {
        scanFile(filePath, filterFn);
      }
    });
  }
  walk(absDir, f => {
    if (filterFn(f)) scanFile(f);
  });
}

function exec(cmd, args, opts) {
  opts = opts || {};
  opts.stdout = opts.stdout || 'inherit';
  if (cfg.verbose) console.log(`${cmd} ${args.join(' ')}\n`);
  const spwOpts = {cwd: cfg.outDir, stdio: ['ignore', opts.stdout, 'inherit']};
  const checkExitCode = (code, signal) => {
    if (signal === 'SIGINT' || signal === 'SIGTERM') return;
    if (code !== 0 && !opts.noErrCheck) {
      console.error(`${cmd} ${args.join(' ')} failed with code ${code}`);
      process.exit(1);
    }
  };
  if (opts.async) {
    const proc = child_process.spawn(cmd, args, spwOpts);
    const procIndex = subprocesses.length;
    subprocesses.push(proc);
    return new Promise((resolve, _reject) => {
      proc.on('exit', (code, signal) => {
        delete subprocesses[procIndex];
        checkExitCode(code, signal);
        resolve();
      });
    });
  } else {
    const spawnRes = child_process.spawnSync(cmd, args, spwOpts);
    checkExitCode(spawnRes.status, spawnRes.signal);
    return spawnRes;
  }
}

function execNode(module, args, opts) {
  const modPath = pjoin(ROOT_DIR, 'ui/node_modules/.bin', module);
  const nodeBin = pjoin(ROOT_DIR, 'tools/node');
  args = [modPath].concat(args || []);
  const argsJson = JSON.stringify(args);
  return exec(nodeBin, args, opts);
}

// ------------------------------------------
// File system & subprocess utility functions
// ------------------------------------------

class Task {
  constructor(func, args) {
    this.func = func;
    this.args = args || [];
    // |identity| is used to dedupe identical tasks in the queue.
    this.identity = JSON.stringify([this.func.name, this.args]);
  }

  get description() {
    const ret = this.func.name.startsWith('exec') ? [] : [this.func.name];
    const flattenedArgs = [].concat.apply([], this.args);
    for (const arg of flattenedArgs) {
      const argStr = `${arg}`;
      if (argStr.startsWith('/')) {
        ret.push(path.relative(cfg.outDir, arg));
      } else {
        ret.push(argStr);
      }
    }
    return ret.join(' ');
  }
}

function walk(dir, callback, skipRegex) {
  for (const child of fs.readdirSync(dir)) {
    const childPath = pjoin(dir, child);
    const stat = fs.lstatSync(childPath);
    if (skipRegex !== undefined && skipRegex.test(child)) continue;
    if (stat.isDirectory()) {
      walk(childPath, callback, skipRegex);
    } else if (!stat.isSymbolicLink()) {
      callback(childPath);
    }
  }
}

function ensureDir(dirPath, clean) {
  const exists = fs.existsSync(dirPath);
  if (exists && clean) {
    console.log('rm', dirPath);
    fs.rmSync(dirPath, {recursive: true});
  }
  if (!exists || clean) fs.mkdirSync(dirPath, {recursive: true});
  return dirPath;
}

function cp(src, dst) {
  ensureDir(path.dirname(dst));
  if (cfg.verbose) {
    console.log(
        'cp', path.relative(ROOT_DIR, src), '->', path.relative(ROOT_DIR, dst));
  }
  fs.copyFileSync(src, dst);
}

function mklink(src, dst) {
  // If the symlink already points to the right place don't touch it. This is
  // to avoid changing the mtime of the ui/ dir when unnecessary.
  if (fs.existsSync(dst)) {
    if (fs.lstatSync(dst).isSymbolicLink() && fs.readlinkSync(dst) === src) {
      return;
    } else {
      fs.unlinkSync(dst);
    }
  }
  fs.symlinkSync(src, dst);
}

main();
