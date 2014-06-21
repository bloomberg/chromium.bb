if (self.importScripts)
    importScripts("/js-test-resources/js-test.js");

description("Both .URL and .url should work (for compatibility reasons).");

var url = "http://127.0.0.1:8000/eventsource/resources/event-stream.php";
var source = new EventSource(url);

shouldBeEqualToString("source.URL", url);
shouldBeEqualToString("source.url", url);
shouldBeTrue("source.URL === source.url");
finishJSTest();
