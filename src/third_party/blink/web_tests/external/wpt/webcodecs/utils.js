function makeImageBitmap(width, height) {
  let canvas = new OffscreenCanvas(width, height);

  let ctx = canvas.getContext('2d');
  ctx.fillStyle = 'rgba(50, 100, 150, 255)';
  ctx.fillRect(0, 0, width, height);

  return canvas.transferToImageBitmap();
}

// Gives a chance to pending output and error callbacks to complete before
// resolving.
function endAfterEventLoopTurn() {
  return new Promise(resolve => step_timeout(resolve, 0));
}

// Returns a codec initialization with callbacks that expected to not be called.
function getDefaultCodecInit(test) {
  return {
    output: test.unreached_func("unexpected output"),
    error: test.unreached_func("unexpected error"),
  }
}

// Checks that codec can be configured, reset, reconfigured, and that incomplete
// or invalid configs throw errors immediately.
function testConfigurations(codec, validCondig, invalidCodecs) {
  assert_equals(codec.state, "unconfigured");

  const requiredConfigPairs = validCondig;
  let incrementalConfig = {};

  for (let key in requiredConfigPairs) {
    // Configure should fail while required keys are missing.
    assert_throws_js(TypeError, () => { codec.configure(incrementalConfig); });
    incrementalConfig[key] = requiredConfigPairs[key];
    assert_equals(codec.state, "unconfigured");
  }

  // Configure should pass once incrementalConfig meets all requirements.
  codec.configure(incrementalConfig);
  assert_equals(codec.state, "configured");

  // We should be able to reconfigure the codec.
  codec.configure(incrementalConfig);
  assert_equals(codec.state, "configured");

  let config = incrementalConfig;

  invalidCodecs.forEach(badCodec => {
    // Invalid codecs should fail.
    config.codec = badCodec;
    assert_throws_js(TypeError, () => { codec.configure(config); }, badCodec);
  })

  // The failed configures should not affect the current config.
  assert_equals(codec.state, "configured");

  // Test we can configure after a reset.
  codec.reset()
  assert_equals(codec.state, "unconfigured");

  codec.configure(validCondig);
  assert_equals(codec.state, "configured");
}

// Performs an encode or decode with the provided input, depending on whether
// the passed codec is an encoder or a decoder.
function encodeOrDecodeShouldThrow(codec, input) {
  // We are testing encode/decode on codecs in invalid states.
  assert_not_equals(codec.state, "configured");

  if (codec.decode) {
    assert_throws_dom("InvalidStateError",
                      () => codec.decode(input),
                      "decode");
  } else if (codec.encode) {
    // Encoders consume frames, so clone it to be safe.
    assert_throws_dom("InvalidStateError",
                  () => codec.encode(input.clone()),
                  "encode");

  } else {
    assert_unreached("Codec should have encode or decode function");
  }
}

// Makes sure that we cannot close, configure, reset, flush, decode or encode a
// closed codec.
function testClosedCodec(test, codec, validconfig, codecInput) {
  assert_equals(codec.state, "unconfigured");

  codec.close();
  assert_equals(codec.state, "closed");

  assert_throws_dom("InvalidStateError",
                    () => codec.configure(validconfig),
                    "configure");
  assert_throws_dom("InvalidStateError",
                    () => codec.reset(),
                    "reset");
  assert_throws_dom("InvalidStateError",
                    () => codec.close(),
                    "close");

  encodeOrDecodeShouldThrow(codec, codecInput);

  return promise_rejects_dom(test, 'InvalidStateError', codec.flush(), 'flush');
}

// Makes sure we cannot flush, encode or decode with an unconfigured coded, and
// that reset is a valid no-op.
function testUnconfiguredCodec(test, codec, codecInput) {
  assert_equals(codec.state, "unconfigured");

  // Configure() and Close() are valid operations that would transition us into
  // a different state.

  // Resetting an unconfigured encoder is a no-op.
  codec.reset();
  assert_equals(codec.state, "unconfigured");

  encodeOrDecodeShouldThrow(codec, codecInput);

  return promise_rejects_dom(test, 'InvalidStateError', codec.flush(), 'flush');
}