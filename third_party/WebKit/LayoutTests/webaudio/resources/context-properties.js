// The list of the 'own' properties in various AudioContexts. These lists were
// populated by running:
//
//   Object.getOwnPropertyNames(FooAudioContext.prototype);
//
// https://webaudio.github.io/web-audio-api/#BaseAudioContext


let BaseAudioContextOwnProperties = [
  'constructor',
  'createAnalyser',
  'createBiquadFilter',
  'createBuffer',
  'createBufferSource',
  'createChannelMerger',
  'createChannelSplitter',
  'createConstantSource',
  'createConvolver',
  'createDelay',
  'createDynamicsCompressor',
  'createGain',
  'createIIRFilter',
  'createOscillator',
  'createPanner',
  'createPeriodicWave',
  'createScriptProcessor',
  'createStereoPanner',
  'createWaveShaper',
  'currentTime',
  'decodeAudioData',
  'destination',
  'listener',
  'onstatechange',
  'resume',
  'sampleRate',
  'state',

  // TODO(hongchan): these belong to AudioContext.
  'createMediaElementSource',
  'createMediaStreamDestination',
  'createMediaStreamSource',
];


let AudioContextOwnProperties = [
  'close',
  'constructor',
  'suspend',
  'getOutputTimestamp',
  'baseLatency',

  // TODO(hongchan): Not implemented yet.
  // 'outputLatency',
];


let OfflineAudioContextOwnProperties = [
  'constructor',
  'length',
  'oncomplete',
  'startRendering',
  'suspend',
];


/**
 * Verify properties in the prototype with the pre-populated list. This is a
 * 2-way comparison to detect the missing and the unexpected property at the
 * same time.
 * @param  {Object} targetPrototype           Target prototype.
 * @param  {Array} populatedList              Property dictionary.
 * @param  {Function} should                  |Should| assertion function.
 * @return {Map}                              Verification result map.
 */
function verifyPrototypeOwnProperties (targetPrototype, populatedList, should) {
  let propertyMap = new Map();
  let generatedList = Object.getOwnPropertyNames(targetPrototype);

  for (let index in populatedList) {
    propertyMap.set(populatedList[index], {
      actual: false,
      expected: true
    });
  }

  for (let index in generatedList) {
    if (propertyMap.has(generatedList[index])) {
      propertyMap.get(generatedList[index]).actual = true;
    } else {
      propertyMap.set(generatedList[index], {
        actual: true,
        expected: false
      });
    }
  }

  // TODO(hongchan): replace the Should assertion when the new test infra lands.
  for (let [property, result] of propertyMap) {
    if (result.expected && result.actual) {
      // The test meets the expectation.
      should('The property "' + property + '"')
          ._testPassed('was expected and found successfully', false);
    } else if (result.expected && !result.actual) {
      // The expected property is missing.
      should('The property "' + property + '" was expected but not found.')
          ._testFailed('', false);
    } else if (!result.expected && result.actual) {
      // Something unexpected was found.
      should('The property "' + property + '" was not expected but found.')
          ._testFailed('', false);
    }
  }
}
