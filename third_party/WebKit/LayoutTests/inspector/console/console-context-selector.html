<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script src="../../http/tests/inspector/debugger-test.js"></script>
<script>
function setup() {
  window.worker = new Worker('resources/worker-pause.js');
  window.iframe = document.createElement('iframe');
  window.iframe.src = 'data:text/html;<script>window.foo=1;<' + '/script>';
  window.iframe.name = 'myframe';
  document.body.appendChild(window.iframe);
  return new Promise(f => window.iframe.onload = f);
}

function pauseInWorker() {
  window.worker.postMessage('pause');
}

function pauseInIframe() {
  window.iframe.contentWindow.eval('debugger;');
}

function pauseInMain() {
  debugger;
}

function destroyIframe() {
  window.iframe.parentElement.removeChild(window.iframe);
  window.iframe = null;
}

async function test()
{
  await new Promise(f => InspectorTest.startDebuggerTest(f, true));

  await InspectorTest.evaluateInPageAsync('setup()');
  var workerTarget = await InspectorTest.waitForTarget(target => target.parentTarget() === InspectorTest.mainTarget);
  var workerExecutionContext = await InspectorTest.waitForExecutionContext(workerTarget.model(SDK.RuntimeModel));
  dump();

  InspectorTest.addResult('');
  InspectorTest.addResult('Selected worker');
  UI.context.setFlavor(SDK.Target, workerTarget);
  dump();

  var mainFrame = InspectorTest.resourceTreeModel.mainFrame;
  var mainExecutionContext = InspectorTest.runtimeModel.executionContexts().find(context => context.frameId === mainFrame.id);
  var childFrame = InspectorTest.resourceTreeModel.frames().find(frame => frame !== InspectorTest.resourceTreeModel.mainFrame);
  var childExecutionContext = InspectorTest.runtimeModel.executionContexts().find(context => context.frameId === childFrame.id);
  InspectorTest.addResult('');
  InspectorTest.addResult('Selected iframe');
  UI.context.setFlavor(SDK.ExecutionContext, childExecutionContext);
  dump();

  InspectorTest.evaluateInPage('pauseInMain()');
  await InspectorTest.waitUntilPausedPromise();
  InspectorTest.addResult('');
  InspectorTest.addResult('Paused in main');
  dump();

  await new Promise(f => InspectorTest.resumeExecution(f));
  InspectorTest.addResult('');
  InspectorTest.addResult('Resumed');
  dump();

  InspectorTest.evaluateInPage('pauseInWorker()');
  await InspectorTest.waitUntilPausedPromise();
  InspectorTest.addResult('');
  InspectorTest.addResult('Paused in worker');
  dump();

  await new Promise(f => InspectorTest.resumeExecution(f));
  InspectorTest.addResult('');
  InspectorTest.addResult('Resumed');
  dump();

  InspectorTest.evaluateInPage('pauseInIframe()');
  await InspectorTest.waitUntilPausedPromise();
  InspectorTest.addResult('');
  InspectorTest.addResult('Paused in iframe');
  dump();

  await new Promise(f => InspectorTest.resumeExecution(f));
  InspectorTest.addResult('');
  InspectorTest.addResult('Resumed');
  dump();

  InspectorTest.evaluateInPage('destroyIframe()');
  await InspectorTest.waitForExecutionContextDestroyed(childExecutionContext);
  InspectorTest.addResult('');
  InspectorTest.addResult('Destroyed iframe');
  dump();

  InspectorTest.completeDebuggerTest();

  function dump() {
    var consoleView = Console.ConsoleView.instance();
    var selector = consoleView._consoleContextSelector;
    InspectorTest.addResult('Console context selector:');
    for (var executionContext of selector._items) {
      var selected = UI.context.flavor(SDK.ExecutionContext) === executionContext;
      var text = '____'.repeat(selector._depthFor(executionContext)) + selector.titleFor(executionContext);
      var disabled = !selector.isItemSelectable(executionContext);
      InspectorTest.addResult(`${selected ? '*' : ' '} ${text} ${disabled ? '[disabled]' : ''}`);
    }
  }
}
</script>
</head>
<body onload="runTest()">
<p> Tests console execution context selector.
</p>
</body>
</html>
