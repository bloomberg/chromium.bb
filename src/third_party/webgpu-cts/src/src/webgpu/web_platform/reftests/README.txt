Reference tests (reftests) for WebGPU canvas presentation.

These render some contents to a canvas using WebGPU, and WPT compares the rendering result with
the "reference" versions (in `ref/`) which render with 2D canvas.

This tests things like:
- The canvas has the correct orientation.
- The canvas renders with the correct transfer function.
- The canvas blends and interpolates in the correct color encoding.

TODO: Test all possible output texture formats (currently only testing bgra8unorm).
TODO: Test all possible ways to write into those formats (currently only testing B2T copy).
