// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function dumpPreviewPanel() {
  TestRunner.addResult('Panel view:');

  const treeElement = UI.panels.resources._sidebar.backgroundFetchTreeElement;
  treeElement.onselect(false);

  const preview = treeElement._view._preview;

  let text = '';
  if (preview.contentElement.getElementsByClassName('background-service-metadata-entry').length)
    text = preview.contentElement.getElementsByClassName('background-service-metadata-entry')[0].textContent;
  else
    text = Array.from(preview.element.getElementsByTagName('p')).map(p => p.textContent).join();

  // There's some platform specific shortcuts + the button in the text, just keep the important bits.
  if (text.startsWith('Click the record button'))
    text = 'Click the record button';

  TestRunner.addResult(' '.repeat(4) + text);
};

async function toggleRecord(model) {
  const treeElement = UI.panels.resources._sidebar.backgroundFetchTreeElement;
  treeElement.onselect(false);

  // Simulate click.
  treeElement._view._toggleRecording();

  // Wait for the view to be aware of the change.
  await new Promise(r => {
    model.addEventListener(Resources.BackgroundServiceModel.Events.RecordingStateChanged, r);
  });

  // Yield thread in case this listener was called before the UI's listener.
  await new Promise(r => setTimeout(r, 0));
}

(async function() {
  Root.Runtime.experiments.setEnabled('backgroundServices', true);

  TestRunner.addResult(`Tests the bottom panel shows information as expected.\n`);
  await TestRunner.showPanel('resources');

  const backgroundServiceModel = TestRunner.mainTarget.model(Resources.BackgroundServiceModel);
  backgroundServiceModel.enable(Protocol.BackgroundService.ServiceName.BackgroundFetch);

  dumpPreviewPanel();
  await toggleRecord(backgroundServiceModel);
  dumpPreviewPanel();
  await toggleRecord(backgroundServiceModel);
  dumpPreviewPanel();

  backgroundServiceModel.backgroundServiceEventReceived({
    timestamp: 1556889085,  // 2019-05-03 14:11:25.000.
    origin: 'http://127.0.0.1:8000/',
    serviceWorkerRegistrationId: 42,  // invalid.
    service: Protocol.BackgroundService.ServiceName.BackgroundFetch,
    eventName: 'Event1',
    instanceId: 'Instance1',
    eventMetadata: [],
  });
  backgroundServiceModel.backgroundServiceEventReceived({
    timestamp: 1556889085,  // 2019-05-03 14:11:25.000.
    origin: 'http://127.0.0.1:8000/',
    serviceWorkerRegistrationId: 42,  // invalid.
    service: Protocol.BackgroundService.ServiceName.BackgroundFetch,
    eventName: 'Event2',
    instanceId: 'Instance1',
    eventMetadata: [{key: 'key', value: 'value'}],
  });
  dumpPreviewPanel();

  const dataGrid = UI.panels.resources._sidebar.backgroundFetchTreeElement._view._dataGrid;
  dataGrid.rootNode().children[0].select();
  dumpPreviewPanel();
  dataGrid.rootNode().children[1].select();
  dumpPreviewPanel();

  // Simulate clicking the clear button.
  UI.panels.resources._sidebar.backgroundFetchTreeElement._view._clearEvents();
  dumpPreviewPanel();

  TestRunner.completeTest();
})();
