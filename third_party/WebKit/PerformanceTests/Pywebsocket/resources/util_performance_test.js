const numIterations = 10;
const numWarmUpIterations = 5;

function startPerformanceTest(connectionType, benchmarkName,
    dataType, isWorker, async, verifyData){
  var iframe = document.createElement('iframe');
  if (connectionType === 'WebSocket') {
    iframe.src = "http://localhost:8001/performance_test_iframe.html";
  } else if (connectionType === 'XHR') {
    iframe.src = "http://localhost:8001/xhr_performance_test_iframe.html";
  } else {
    iframe.src = "http://localhost:8001/fetch_performance_test_iframe.html";
  }
  iframe.onload = function() {
    var child = iframe.contentWindow;
    PerfTestRunner.prepareToMeasureValuesAsync({
        done: function() {
          child.postMessage({'command': 'stop'},
                            'http://localhost:8001');
        },
        unit: 'ms',
        iterationCount: numIterations
    });

    child.postMessage({'command': 'start',
                       'connectionType': connectionType,
                       'benchmarkName': benchmarkName,
                       'dataType': dataType,
                       'isWorker': isWorker,
                       'async': async,
                       'verifyData': verifyData,
                       'numIterations': numIterations,
                       'numWarmUpIterations': numWarmUpIterations},
                      'http://localhost:8001');
  };
  document.body.appendChild(iframe);
}

onmessage = function(message) {
  if (message.data.command === 'log') {
    PerfTestRunner.log(message.data.value);
  } else if (message.data.command === 'measureValue') {
    PerfTestRunner.measureValueAsync(message.data.value);
    PerfTestRunner.gc();
  } else if (message.data.command === 'notifyAbort') {
    PerfTestRunner.logFatalError("benchmark aborted");
  }
};
