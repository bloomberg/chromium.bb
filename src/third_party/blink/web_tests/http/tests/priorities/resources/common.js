/**
 * These values map to some of the the current
 * priority enum members in blink::ResourceLoadPriority.
 * The values are exposed through window.internals
 * and in these tests, we use the below variables to represent
 * the exposed values in a readable way.
 */
const kLow = 1,
      kMedium = 2,
      kHigh = 3,
      kVeryHigh = 4;

function reportPriority(url, optionalDoc) {
  const documentToUse = optionalDoc ? optionalDoc : document;
  const loadPriority = internals.getResourcePriority(url, documentToUse);
  window.opener.postMessage(loadPriority, '*');
}

function reportFailure() {
  window.opener.postMessage('FAILED', '*');
}
