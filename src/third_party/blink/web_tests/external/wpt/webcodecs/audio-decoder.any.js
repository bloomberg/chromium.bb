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

const invalidConfigs = [
  {
    comment: 'Emtpy codec',
    config: {codec: ''},
  },
  {
    comment: 'Unrecognized codec',
    config: {codec: 'bogus'},
  },
  {
    comment: 'Video codec',
    config: {codec: 'vp8'},
  },
  {
    comment: 'Ambiguous codec',
    config: {codec: 'vp9'},
  },
  {
    comment: 'Codec with MIME type',
    config: {codec: 'audio/webm; codecs="opus"'},
  },
];

invalidConfigs.forEach(entry => {
  promise_test(t => {
    return promise_rejects_js(t, TypeError, AudioDecoder.isConfigSupported(entry.config));
  }, 'Test that AudioDecoder.isConfigSupported() rejects invalid config:' + entry.comment);
});


invalidConfigs.forEach(entry => {
  async_test(t => {
    let codec = new AudioDecoder(getDefaultCodecInit(t));
    assert_throws_js(TypeError, () => { codec.configure(entry.config); });
    t.done();
  }, 'Test that AudioDecoder.configure() rejects invalid config:' + entry.comment);
});

promise_test(t => {
  return AudioDecoder.isConfigSupported({
    codec: 'opus',
    sampleRate: 48000,
    numberOfChannels: 2,
    // Opus header extradata.
    description: new Uint8Array([0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,
        0x01, 0x02, 0x38, 0x01, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00])
  });
}, 'Test AudioDecoder.isConfigSupported() with a valid config');

promise_test(t => {
  // Define a valid config that includes a hypothetical 'futureConfigFeature',
  // which is not yet recognized by the User Agent.
  const validConfig = {
    codec: 'opus',
    sampleRate: 48000,
    numberOfChannels: 2,
    // Opus header extradata.
    description: new Uint8Array([0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,
        0x01, 0x02, 0x38, 0x01, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00]),
    futureConfigFeature: 'foo',
  };

  // The UA will evaluate validConfig as being "valid", ignoring the
  // `futureConfigFeature` it  doesn't recognize.
  return AudioDecoder.isConfigSupported(validConfig).then((decoderSupport) => {
    // AudioDecoderSupport must contain the following properites.
    assert_true(decoderSupport.hasOwnProperty('supported'));
    assert_true(decoderSupport.hasOwnProperty('config'));

    // AudioDecoderSupport.config must not contain unrecognized properties.
    assert_false(decoderSupport.config.hasOwnProperty('futureConfigFeature'));

    // AudioDecoderSupport.config must contiain the recognized properties.
    assert_equals(decoderSupport.config.codec, validConfig.codec);
    assert_equals(decoderSupport.config.sampleRate, validConfig.sampleRate);
    assert_equals(decoderSupport.config.numberOfChannels, validConfig.numberOfChannels);

    // The description BufferSource must copy the input config description.
    assert_not_equals(decoderSupport.config.description, validConfig.description);
    let parsedDescription = new Uint8Array(decoderSupport.config.description);
    assert_equals(parsedDescription.length, validConfig.description.length);
    for (let i = 0; i < parsedDescription.length; ++i) {
      assert_equals(parsedDescription[i], validConfig.description[i]);
    }
  });
}, 'Test that AudioDecoder.isConfigSupported() returns a parsed configuration');

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
