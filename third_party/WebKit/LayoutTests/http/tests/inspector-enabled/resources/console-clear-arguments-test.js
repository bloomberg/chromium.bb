function print(text)
{
    var output = document.getElementById("output");
    var div = document.createElement("div");
    div.textContent = text;
    output.appendChild(div);
}

function dumpConsoleMessageCounts()
{
    var consoleMessageCount = window.internals.numberOfConsoleMessages(document);
    if (consoleMessageCount === 3)
        print("PASSED: found argument counts for 3 messages");
    else
        print("FAILED: unexpected number of messages: " + consoleMessageCount);

    var consoleMessageWithArgumentsCount = window.internals.numberOfConsoleMessagesWithArguments(document);
    if (consoleMessageWithArgumentsCount === 0)
        print("PASSED: no messages with arguments");
    else
        print("FAILED: unexpected number of messages with arguments: " + consoleMessageWithArgumentsCount);
}
