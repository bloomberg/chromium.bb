function perfTestAddToLog(text) {
  PerfTestRunner.log(text);
}

function perfTestAddToSummary(text) {
}

function perfTestMeasureValue(value) {
  PerfTestRunner.measureValueAsync(value);
  PerfTestRunner.gc();
}

function perfTestNotifyAbort() {
  PerfTestRunner.logFatalError("benchmark aborted");
}

const numIterations = 10;
const numWarmUpIterations = 5;

function getConfigForPerformanceTest(connectionType, dataType, async,
                                     verifyData) {
  return {
    prefixUrl:
      connectionType === 'WebSocket' ? 'ws://localhost:8001/benchmark_helper' :
      'http://localhost:8001/073be001e10950692ccbf3a2ad21c245', // XHR or fetch
    printSize: true,
    numXHRs: 1,
    numFetches: 1,
    numSockets: 1,
    // + 1 is for a warmup iteration by the Telemetry framework.
    numIterations: numIterations + numWarmUpIterations + 1,
    numWarmUpIterations: numWarmUpIterations,
    minTotal: 10240000,
    startSize: 10240000,
    stopThreshold: 10240000,
    multipliers: [2],
    verifyData: verifyData,
    dataType: dataType,
    async: async,
    addToLog: perfTestAddToLog,
    addToSummary: perfTestAddToSummary,
    measureValue: perfTestMeasureValue,
    notifyAbort: perfTestNotifyAbort
  };
}

function startPerformanceTest(connectionType, benchmarkName,
    dataType, isWorker, async, verifyData){
  initWorker(connectionType, 'http://localhost:8001');

  PerfTestRunner.prepareToMeasureValuesAsync({
      done: function() {
        var config = getConfigForPerformanceTest(connectionType, dataType,
                                                 async, verifyData);
        doAction(config, isWorker, 'stop');
      },
      unit: 'ms',
      dromaeoIterationCount: numIterations
  });

  var config = getConfigForPerformanceTest(connectionType, dataType, async,
                                           verifyData);
  doAction(config, isWorker, benchmarkName);
}
