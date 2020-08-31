# WebGPU Conformance Test Suite

## >>> [**Contribution Guidelines**](https://github.com/gpuweb/gpuweb/wiki/WebGPU-CTS-guidelines) <<<

## Run these tests live: [gpuweb.github.io/cts/standalone/](https://gpuweb.github.io/cts/standalone/)

## **NOTE**: If you are contributing tests that tentatively use GLSL instead of WGSL, develop on the [glsl-dependent](https://github.com/gpuweb/cts/tree/glsl-dependent) branch (run live at [gpuweb-cts-glsl.github.io/standalone/](http://gpuweb-cts-glsl.github.io/standalone/))

## Docs

- [Terminology used in the test framework](docs/terms.md)

## Developing

The WebGPU CTS is written in TypeScript, and builds into two directories:

- `out/`: Built framework and test files, needed to run standalone or command line.
- `out-wpt/`: Build directory for export into WPT. Contains WPT runner and a copy of just the needed files from `out/`.

### Setup

After checking out the repository and installing Yarn, run these commands to
set up dependencies:

```sh
cd webgpu/
yarn install

npx grunt  # show available grunt commands
```

### Build

To build and run all pre-submit checks (including type and lint checks and
unittests), use:

```sh
npx grunt pre
```

For a quicker iterative build:

```sh
npx grunt test
```

### Run

To test in a browser under the standalone harness, run `grunt serve`, then
open:

- http://localhost:8080/standalone/ (defaults to ?runnow=0&worker=0&debug=0&q=webgpu:)
- http://localhost:8080/standalone/?runnow=1&q=unittests:
- http://localhost:8080/standalone/?runnow=1&q=unittests:basic:&q=unittests:params:

### Debug

To see debug logs in a browser, use the `debug=1` query string:

- http://localhost:8080/standalone/?q=webgpu:validation&debug=1

### Making Changes

To add new tests, simply imitate the pattern in neigboring tests or
neighboring files. New test files must be named ending in `.spec.ts`.

For an example test file, see `src/webgpu/examples.spec.ts`.

Since this project is written in TypeScript, it integrates best with Visual
Studio Code. There are also some default settings (in `.vscode/settings.json`)
which will be applied automatically.

Before uploading, you should run pre-submit checks (`grunt pre`).

Be sure to read [CONTRIBUTING.md](CONTRIBUTING.md).

### Export to WPT

Copy (or symlink) the `out-wpt/` directory as the `webgpu/` directory in your
WPT checkout.
