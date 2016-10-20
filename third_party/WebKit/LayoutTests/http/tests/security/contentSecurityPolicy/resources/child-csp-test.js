var CROSS_ORIGIN_URL = "http://localhost:8000/security/contentSecurityPolicy/resources/respond-with-allow-csp-from-header.php";
var SAME_ORIGIN_URL = "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/respond-with-allow-csp-from-header.php";

var EXPECT_BLOCK = true;
var EXPECT_LOAD = false;

var CROSS_ORIGIN = true;
var SAME_ORIGIN = false;

function injectIframeWithCSP(url, shouldBlock, csp, t, urlId) {
    var i = document.createElement('iframe');
    i.src = url + "&id=" + urlId;
    i.csp = csp;

    var loaded = {};
    window.addEventListener("message", function (e) {
        if (e.source != i.contentWindow)
            return;
        if (e.data["loaded"])
            loaded[e.data["id"]] = true;
    });

    if (shouldBlock) {
        window.onmessage = function (e) {
            if (e.source != i.contentWindow)
                return;
            t.unreached_func('No message should be sent from the frame.');
        }
        i.onload = t.step_func(function () {
            // Delay the check until after the postMessage has a chance to execute.
            setTimeout(t.step_func_done(function () {
                 assert_equals(loaded[urlId], undefined);
            }), 1);
        });
    } else {
        document.addEventListener("securitypolicyviolation",
            t.unreached_func("There should not be any violations."));
        i.onload = t.step_func(function () {
            // Delay the check until after the postMessage has a chance to execute.
            setTimeout(t.step_func_done(function () {
                 assert_true(loaded[urlId]);
            }), 1);
        });
    }
    document.body.appendChild(i);
}
function generateUrlWithAllowCSPFrom(useCrossOrigin, allowCspFrom) {
    var url = useCrossOrigin ? CROSS_ORIGIN_URL : SAME_ORIGIN_URL;
    return url + "?allow_csp_from=" + allowCspFrom;
}
