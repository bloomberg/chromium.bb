// Test case adapted from Mozilla tests for font-variant-subproperties available under
// http://creativecommons.org/publicdomain/zero/1.0/
// See discussion on https://bugzilla.mozilla.org/show_bug.cgi?id=1261445

// data associated with gsubtest test font for testing font features

// prefix
gPrefix = "";

// equivalent properties
// setting prop: value should match the specific feature settings listed
//
// each of these tests evaluate whether a given feature is enabled as required
// and also whether features that shouldn't be enabled are or not.
var gPropertyData = [
  // font-variant-caps
  // valid values
  { prop: "font-variant-caps", value: "normal", features: {"smcp": 0} },
  { prop: "font-variant-caps", value: "small-caps", features: {"smcp": 1, "c2sc": 0} },
  { prop: "font-variant-caps", value: "all-small-caps", features: {"smcp": 1, "c2sc": 1, "pcap": 0} },
  { prop: "font-variant-caps", value: "petite-caps", features: {"pcap": 1, "smcp": 0} },
  { prop: "font-variant-caps", value: "all-petite-caps", features: {"c2pc": 1, "pcap": 1, "smcp": 0} },
  { prop: "font-variant-caps", value: "titling-caps", features: {"titl": 1, "smcp": 0} },
  { prop: "font-variant-caps", value: "unicase", features: {"unic": 1, "titl": 0} },

  // invalid values
  { prop: "font-variant-caps", value: "normal small-caps", features: {"smcp": 0}, invalid: true },
  { prop: "font-variant-caps", value: "small-caps potato", features: {"smcp": 0}, invalid: true },
  { prop: "font-variant-caps", value: "small-caps petite-caps", features: {"smcp": 0, "pcap": 0}, invalid: true },
  { prop: "font-variant-caps", value: "small-caps all-small-caps", features: {"smcp": 0, "c2sc": 0}, invalid: true },
  { prop: "font-variant-caps", value: "small-cap", features: {"smcp": 0}, invalid: true },

];

// note: the code below requires an array "gFeatures" from :
//   layout/reftests/fonts/gsubtest/gsubtest-features.js

// The font defines feature lookups for all OpenType features for a
// specific set of PUA codepoints, as listed in the gFeatures array.
// Using these codepoints and feature combinations, tests can be
// constructed to detect when certain features are enabled or not.

// return a created table containing tests for a given property
//
// Ex: { prop: "font-variant-ligatures", value: "common-ligatures", features: {"liga": 1, "clig": 1, "dlig": 0, "hlig": 0} }
//
// This means that for the property 'font-variant-ligatures' with the value 'common-ligatures', the features listed should
// either be explicitly enabled or disabled.

// propData is the prop/value list with corresponding feature assertions
// whichProp is either "all" or a specific subproperty (i.e. "font-variant-position")
// isRef is true when this is the reference
// debug outputs the prop/value pair along with the tests

// default PASS codepoint used for reference rendering
// need to use a PUA codepoint to avoid problems related to Freetype auto-hinting
const kRefCodepoint = 0xe00c;

function createFeatureTestTable(propData, whichProp, isRef, debug)
{
  var table = document.createElement("table");

  if (typeof(isRef) == "undefined") {
    isRef = false;
  }

  if (typeof(debug) == "undefined") {
    debug = false;
  }

  var doAll = (whichProp == "all");
  for (var i in propData) {
    var data = propData[i];

    if (!doAll && data.prop != whichProp) continue;

    var row = document.createElement("tr");
    var invalid = false;
    if ("invalid" in data) {
      invalid = true;
      row.className = "invalid";
    }

    var cell = document.createElement("td");
    cell.className = "prop";
    var styledecl = gPrefix + data.prop + ": " + data.value + ";";
    cell.innerHTML = styledecl;
    row.appendChild(cell);
    if (debug) {
      table.appendChild(row);
    }

    row = document.createElement("tr");
    if (invalid) {
      row.className = "invalid";
    }

    cell = document.createElement("td");
    cell.className = "features";
    if (!isRef) {
      cell.style.cssText = styledecl;
    }

    for (var f in data.features) {
      var feature = data.features[f];

      var cp, unsupported = "F".charCodeAt(0);
      var basecp = gFeatures[f];

      if (typeof(basecp) == "undefined") {
        cp = unsupported;
      } else {
        switch(feature) {
        case 0:
          cp = basecp;
          break;
        case 1:
          cp = basecp + 1;
          break;
        case 2:
          cp = basecp + 2;
          break;
        case 3:
          cp = basecp + 3;
          break;
        default:
          cp = basecp + 1;
          break;
        }
      }

      var span = document.createElement("span");
      var cpOut = (isRef ? kRefCodepoint : cp);
      span.innerHTML = "&#x" + cpOut.toString(16) + ";";
      span.title = f + "=" + feature;
      cell.appendChild(span);
    }
    row.appendChild(cell);
    table.appendChild(row);
  }

  return table;
}
