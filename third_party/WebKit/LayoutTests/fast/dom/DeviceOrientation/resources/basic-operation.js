// This test can be used to test device orientation event types: 'deviceorientation' and 'deviceorientationabsolute'.
function testBasicOperation(eventType) {
    window.jsTestIsAsync = true;
    if (!window.testRunner)
        debug('This test can not be run without the TestRunner');

    mockAlpha = 1.1;
    mockBeta = 2.2;
    mockGamma = 3.3;
    mockAbsolute = true;
    testRunner.setMockDeviceOrientation(true, mockAlpha, true, mockBeta, true, mockGamma, mockAbsolute);

    window.addEventListener(eventType, function(e) {
        event = e;
        shouldBe('event.alpha', 'mockAlpha');
        shouldBe('event.beta', 'mockBeta');
        shouldBe('event.gamma', 'mockGamma');
        shouldBe('event.absolute', 'mockAbsolute');
        finishJSTest();
    });
}
