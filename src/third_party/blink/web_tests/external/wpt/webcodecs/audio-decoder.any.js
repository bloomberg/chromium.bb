// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

const defaultConfig = {
  codec: "opus",
  sampleRate: 48000,
  numberOfChannels: 2
};

function getFakeChunk() {
  return new EncodedAudioChunk({
    type:'key',
    timestamp:0,
    data:Uint8Array.of(0)
  });
}

promise_test(t => {
  // AudioDecoderInit lacks required fields.
  assert_throws_js(TypeError, () => { new AudioDecoder({}); });

  // AudioDecoderInit has required fields.
  let decoder = new AudioDecoder(getDefaultCodecInit(t));

  assert_equals(decoder.state, "unconfigured");
  decoder.close();

  return endAfterEventLoopTurn();
}, 'Test AudioDecoder construction');

promise_test(t => {
  let decoder = new AudioDecoder(getDefaultCodecInit(t));

  let badCodecsList = [
    '',                         // Empty codec
    'bogus',                    // Non exsitent codec
    'vp8',                      // Video codec
    'audio/webm; codecs="opus"' // Codec with mime type
  ]

  testConfigurations(decoder, defaultConfig, badCodecsList);

  return endAfterEventLoopTurn();
}, 'Test AudioDecoder.configure()');

promise_test(t => {
  let decoder = new AudioDecoder(getDefaultCodecInit(t));

  return testClosedCodec(t, decoder, defaultConfig, getFakeChunk());
}, 'Verify closed AudioDecoder operations');

promise_test(t => {
  let decoder = new AudioDecoder(getDefaultCodecInit(t));

  return testUnconfiguredCodec(t, decoder, getFakeChunk());
}, 'Verify unconfigured AudioDecoder operations');
