test(() => {
  if (typeof PerformanceObserver.supportedEntryTypes === "undefined")
    assert_unreached("supportedEntryTypes is not supported.");
  assert_greater_than(PerformanceObserver.supportedEntryTypes.indexOf("element"),
    -1, "There should be an entry 'element' in PerformanceObserver.supportedEntryTypes");
}, "supportedEntryTypes contains 'element'.");
