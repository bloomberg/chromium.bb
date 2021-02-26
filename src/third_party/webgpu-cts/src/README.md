# WebGPU Conformance Test Suite

## &gt;&gt;&gt; [**Contribution Guidelines**](https://github.com/gpuweb/gpuweb/wiki/WebGPU-CTS-guidelines) &lt;&lt;&lt; | &gt;&gt;&gt; [**View/Edit Test Plan**](https://hackmd.io/@webgpu/H1MwoqqAU) &lt;&lt;&lt;

## Run these tests live: [gpuweb.github.io/cts/standalone/](https://gpuweb.github.io/cts/standalone/)

## **NOTE**: If you are contributing tests that tentatively use GLSL instead of WGSL, develop on the [glsl-dependent](https://github.com/gpuweb/cts/tree/glsl-dependent) branch (run live at [gpuweb-cts-glsl.github.io/standalone/](http://gpuweb-cts-glsl.github.io/standalone/))

## Docs

- This file
- [Terminology used in the test framework](docs/terms.md)

## Contributing

**First, read the workflow section of the [test plan](https://hackmd.io/@webgpu/H1MwoqqAU)**
for information on how additions to the CTS should be developed.
Read [CONTRIBUTING.md](CONTRIBUTING.md) on licensing.

For realtime communication about WebGPU spec and test, join the
[#WebGPU:matrix.org room](https://app.element.io/#/room/#WebGPU:matrix.org)
on Matrix.

### Making Changes

To add new tests, imitate the pattern in neigboring tests or
neighboring files. New test files must be named ending in `.spec.ts`.

For an example test file, see `src/webgpu/examples.spec.ts`.

Since this project is written in TypeScript, it integrates best with Visual
Studio Code. There are also some default settings (in `.vscode/settings.json`)
which will be applied automatically.

### Opening a Pull Request

Before uploading, you can run pre-submit checks (`grunt pre`) to make sure
it will pass CI.

To contribute changes, simply open a pull request against the project at
<https://github.com/gpuweb/cts> to automatically notify reviewers.
Depending on whether your change is against the `main` branch or the
`glsl-dependent` branch, be sure to open it against the right one.

To make reviewers' lives easier, try to keep pull requests small and
incremental when possible (though this is often difficult since tests are very
verbose).

### Making Revisions

During the code review process, keep a few things in mind to help reviewers
review your changes. Once a reviewer has looked at your change once already:

- Avoid major additions or changes that would be best done in a follow-up PR.
- Avoid rebases (`git rebase`) and force pushes (`git push -f`). These can make
  it difficult for reviewers to review incremental changes as GitHub cannot
  view a useful diff across a rebase. If it's necessary to resolve conflicts
  with upstream changes, use a merge commit (`git merge`) and don't include any
  consequential changes in the merge, so a reviewer can skip over merge commits
  when working through the individual commits in the PR.
- When you address a review comment, mark the thread as "Resolved".

## Developing

The WebGPU CTS is written in TypeScript.

### Setup

After checking out the repository and installing node/npm, run these commands
in the checkout:

```sh
npm install

npx grunt  # show available grunt commands
```

### Build

The project builds into two directories:

- `out/`: Built framework and test files, needed to run standalone or command line.
- `out-wpt/`: Build directory for export into WPT. Contains:
    - An adapter for running WebGPU CTS tests under WPT
    - A copy of the needed files from `out/`
    - A copy of any `.html` test cases from `src/`

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

To test in a browser under the standalone harness, run `npx grunt serve`, then
open:

- `http://localhost:8080/standalone/` (defaults to `?runnow=0&worker=0&debug=0&q=webgpu:*`)
- `http://localhost:8080/standalone/?q=unittests:*`
- `http://localhost:8080/standalone/?q=unittests:basic:*`

The following url parameters change how the harness runs:

- `runnow=1` runs all matching tests on page load.
- `debug=1` enables verbose debug logging from tests.
- `worker=1` runs the tests on a Web Worker instead of the main thread.

Currently, large [test queries](docs/terms.md) may take a while to load.
You can open the browser's Dev Tools to make sure something hasn't crashed.
If a query loads too slowly, load a more targeted query.

### Export to WPT

Copy (or symlink) the `out-wpt/` directory as the `webgpu/` directory in your
WPT checkout or your browser's "internal" WPT test directory.
