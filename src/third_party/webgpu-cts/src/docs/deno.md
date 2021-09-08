# Running the CTS on Deno

Since version 1.8, Deno experimentally implements the WebGPU API out of the box.
You can use the `./tools/deno` script to run the CTS in Deno. To do this you
will first need to install Deno: [stable](https://deno.land#installation), or
build the main branch from source
(`cargo install --git https://github.com/denoland/deno --bin deno`).

On macOS and recent Linux, you can just run `./tools/deno` as is. On Windows and
older Linux releases you will need to run
`deno run --unstable --allow-read --allow-write --allow-env ./tools/deno`.

## Usage

```
Usage:
  tools/deno [OPTIONS...] QUERIES...
  tools/deno 'unittests:*' 'webgpu:buffers,*'
Options:
  --verbose             Print result/log of every test as it runs.
  --debug               Include debug messages in logging.
  --print-json          Print the complete result JSON in the output.
  --trace               Record a trace of the WebGPU operations into a ./trace directory.
  --ignore-file=<file>  A line delimited text file containing names of tests to skip.
```
