// This file is for the audiochannelmerger-* layout tests.
// Requires |audio-testing.js| to work properly.

function testMergerInput(config, done) {
  var context = new OfflineAudioContext(config.numberOfChannels, 128, 44100);
  var merger = context.createChannelMerger(config.numberOfChannels);
  var source = context.createBufferSource();

  // Create a test source buffer.
  source.buffer = createTestingAudioBuffer(
    context, config.testBufferChannelCount, 128
  );

  // Connect the output of source into the specified input of merger.
  if (config.mergerInputIndex)
    source.connect(merger, 0, config.mergerInputIndex);
  else
    source.connect(merger);
  merger.connect(context.destination);
  source.start();

  context.startRendering().then(function (buffer) {
    for (var i = 0; i < config.numberOfChannels; i++)
      Should('Channel #' + i, buffer.getChannelData(i))
        .beConstantValueOf(config.expected[i]);
    done();
  });
}