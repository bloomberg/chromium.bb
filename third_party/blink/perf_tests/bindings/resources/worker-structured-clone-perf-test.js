const WorkerStructuredClonePerfTestRunner = (function() {
  function pingPong(data) {
    return new Promise((resolve, reject) => {
      let beginSerialize, endSerialize, beginDeserialize, endDeserialize;
      worker.addEventListener('message', function listener(e) {
        try {
          e.data;  // Force deserialization.
          endDeserialize = PerfTestRunner.now();
          worker.removeEventListener('message', listener);
          // TODO(panicker): Deserialize is currently including worker hop and
          // not accurate. Report this from the worker.
          resolve([endSerialize - beginSerialize, endDeserialize - beginDeserialize,
                  endDeserialize - beginSerialize]);
        } catch (err) { reject(err); }
      });
      beginSerialize = PerfTestRunner.now();
      worker.postMessage(data);
      beginDeserialize = endSerialize = PerfTestRunner.now();
      // While Chrome does the deserialize lazily when e.data is read, this
      // isn't portable, so it's more fair to measure from when the message is
      // posted.
    });
  }

  return {
    measureTimeAsync(test) {
      let isDone = false;
      worker = new Worker('resources/worker-structured-clone.js');
      PerfTestRunner.startMeasureValuesAsync({
        description: test.description,
        unit: 'ms',
        warmUpCount: test.warmUpCount || 10,
        iterationCount: test.iterationCount || 250,
        done() { isDone = true; },
        run: pingPongUntilDone,
      });

      function pingPongUntilDone() {
        pingPong(test.data).then(([serializeTime, deserializeTime, totalTime]) => {
          console.log([serializeTime, deserializeTime, totalTime]);
          if (test.measure === 'serialize')
            PerfTestRunner.measureValueAsync(serializeTime);
          else if (test.measure === 'deserialize')
            PerfTestRunner.measureValueAsync(deserializeTime);
          else if (test.measure === 'roundtrip')
            PerfTestRunner.measureValueAsync(totalTime);
          if (!isDone) pingPongUntilDone();
        });
      }
    },
  };
})();
