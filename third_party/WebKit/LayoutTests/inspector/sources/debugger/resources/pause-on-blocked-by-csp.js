function test()
{
    InspectorTest.startDebuggerTest(step1);

    function step1()
    {
        DebuggerAgent.setPauseOnExceptions(WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
        InspectorTest.evaluateInPage("setTimeout(testAction, 100);");
        InspectorTest.waitUntilPausedAndDumpStackAndResume(step2);
    }

    function step2()
    {
        DebuggerAgent.setPauseOnExceptions(WebInspector.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
        InspectorTest.completeDebuggerTest();
    }
}

window.addEventListener("load", runTest);
