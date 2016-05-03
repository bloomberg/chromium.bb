// Starting configuration number.  This should be zero normally.
var configNumber = 0;

var mediaFile = findMediaFile("video", "content/test");
var onscreenParent = document.createElement("div");
// The onscreen parent's height is also used to make sure that the off-screen
// parent is, in fact, off-screen.
onscreenParent.style.height = "1000px";
document.body.insertBefore(onscreenParent, document.body.firstChild);
// Is the page optimized for mobile?  We can't un-optimize it.
var isOptimizedForMobile = false;
// Also create another root that's off the bottom of the window.
var offscreenParent = document.createElement("div");
document.body.appendChild(offscreenParent);

function didPlaybackStart(element)
{
    return !element.paused || element.ended;
}

function becomeOptimizedForMobile(enable)
{
    // If we're in the right state already, then return;
    if (enable == isOptimizedForMobile)
        return;

    if (!enable) {
        // We can't transition out of optimized for mobile.
        endTest();
    } else {
        // This only works once.
        mobileMetaTag = document.createElement('meta');
        mobileMetaTag.name = "viewport";
        mobileMetaTag.content = "width=device-width";
        document.head.appendChild(mobileMetaTag);
        isOptimizedForMobile = true;
    }
}

function addResultsRow(spec)
{
    // Add a row to the results table.
    var row = document.getElementById("results").insertRow();
    var td = row.insertCell();

    // Add experiment number
    row.insertCell().innerText = (""+spec.experimentNumber);

    // Process experiment type specially.
    var type = spec.experimentType;
    var smallType = "";
    smallType += type.includes("-forvideo")?"v":"";
    smallType += type.includes("-foraudio")?"a":"";
    smallType += type.includes("-ifviewport")?"V":"";
    smallType += type.includes("-ifpagevisible")?"P":"";
    smallType += type.includes("-ifmuted")?"M":"";
    smallType += type.includes("-playmuted")?"p":"";
    smallType += type.includes("-ifmobile")?"m":"";
    row.insertCell().innerText = smallType;

    // Add remaining fields.
    var fields = [ "elementType", "autoplayType", "mutedType", "mobileType",
        "visType", "settingValue", "playedEarly", "played", "muted"];
    for(idx in fields)
        row.insertCell().innerText = spec[fields[idx]].substring(0,7);
}

function configureElementViaScript(element, spec)
{
    if(spec.autoplayType == "play()")
        element.play();
}

function queueNextExperiment()
{
    // Start the next config, but let the event queue drain.
    setTimeout(runNextConfig, 0);
}

function checkElementStatus(element)
{
    // Update the spec with the results.
    var didStart = didPlaybackStart(element);
    element.spec.played = didStart ? "played" : "no";
    element.spec.muted = didStart ? (element.muted ? "muted" : "unmuted") : "-";

    addResultsRow(element.spec);
    element.remove();

    // Scroll back to the top, in case this was a scrolling test.
    onscreenParent.scrollIntoView();

    // Also make sure that the page is visible again.  Hidden pages cause the
    // test to proceed very slowly.
    testRunner.setPageVisibility("visible");

    queueNextExperiment();
}

function runOneConfig(spec)
{
    internals.settings.setAutoplayExperimentMode(spec.experimentType);
    internals.settings.setViewportMetaEnabled(true);
    testRunner.setAutoplayAllowed(spec.settingValue == "enabled");

    // Create, configure, and attach a media element.
    var element = document.createElement(spec.elementType);
    element.controls = true;

    // Hide or show the page.
    if (spec.visType == "obscured")
        testRunner.setPageVisibility("hidden");

    // Pick whether the element will be visible when canPlayThrough.
    if (spec.visType == "offscreen" || spec.visType == "scroll")
        offscreenParent.appendChild(element);
    else
        onscreenParent.appendChild(element);

    // Set any attributes before canPlayThrough.
    if (spec.mutedType == "yes")
        element.muted = true;
    if (spec.autoplayType == "attr")
        element.autoplay = true;

    becomeOptimizedForMobile(spec.mobileType == "yes");

    spec.playedEarly = "-";

    // Record the spec in the element, so that we can display the
    // results later.
    element.spec = spec;
    window.internals.triggerAutoplayViewportCheck(element);

    // Wait for canplaythrough before continuing, so that the media
    // might actually be playing.
    element.addEventListener("canplaythrough", function()
    {
        // Now that we can play, if we're supposed to play / mute via js do so.
        configureElementViaScript(element, spec);

        // If we're supposed to scroll the item onscreen after it is ready to
        // play, then do so now.
        if(spec.visType == "scroll") {
            // Record the play state here, too, before we scroll.
            spec.playedEarly = didPlaybackStart(element) ? "yes" : "no";

            // We are supposed to scroll the player into view.
            element.scrollIntoView(true);
            // TODO(liberato): remove once autoplay gesture override experiment concludes.
            window.internals.triggerAutoplayViewportCheck(element);
            // Once these two methods return, changes to the element state due
            // to the autoplay experiment should be observable synchronously.
            checkElementStatus(element, spec);
        } else {
            // Record the results immediately.
            checkElementStatus(element, spec);
        }
    });

    // Set the source, which will eventually lead to canPlayThrough.
    element.src = mediaFile;
}

var experimentTypes = []; // set in start().
var elementTypes = []; // set in start().
var autoplayTypes = ["none", "attr", "play()"];
var mutedTypes = ["no", "yes"];
var visTypes = ["onscreen", "scroll", "offscreen", "obscured"];
// mobileTypes must always start with no, since we cannot un-optimize the page.
var mobileTypes = ["no", "yes"];
var autoplaySettings = ["enabled", "disabled"];

function runNextConfig()
{
    // Convert configNumber into a spec, and run it.
    var exp = configNumber;

    // Convert this experiment number into settings.
    var spec = {};
    spec.elementType = elementTypes[exp % elementTypes.length];
    exp = Math.floor(exp / elementTypes.length);
    spec.experimentType = experimentTypes[exp % experimentTypes.length];
    exp = Math.floor(exp / experimentTypes.length);
    spec.autoplayType = autoplayTypes[exp % autoplayTypes.length];
    exp = Math.floor(exp / autoplayTypes.length);
    spec.mutedType = mutedTypes[exp % mutedTypes.length];
    exp = Math.floor(exp / mutedTypes.length);
    spec.visType = visTypes[exp % visTypes.length];
    exp = Math.floor(exp / visTypes.length);
    spec.settingValue = autoplaySettings[exp % autoplaySettings.length];
    exp = Math.floor(exp / autoplaySettings.length);
    // Mobile must always change last, so that all the "no" cases precede
    // all the "yes" cases, since we can't switch the doc back to "not
    // optimized for mobile".
    spec.mobileType = mobileTypes[exp % mobileTypes.length];
    exp = Math.floor(exp / mobileTypes.length);

    spec.experimentNumber = configNumber;

    // Return null if configNumber was larger than the highest experiment.
    if (exp > 0)
        endTest();

    configNumber++;

    // To keep the test fast, skip a few combinations.
    var skip = false;
    if (!spec.experimentType.includes("-ifmuted") && spec.mutedType != "no")
        skip = true;

    // Only allow basic combinations for the mobile case.  We just want to
    // test video with autoplay, no mute options when testing -ifmobile.
    // Similarly, if we're setting the page to be optimied for mobile, then
    // require that we're one of those tests.
    if ((spec.mobileType == "yes" || spec.experimentType.includes("-ifmobile"))
        && (spec.elementType != "video" || spec.autoplayType != "attr"
            || spec.mutedType != "no"
            || spec.visType != "onscreen"
            || (spec.experimentType != "enabled-forvideo"
                && spec.experimentType != "enabled-forvideo-ifmobile")))
        skip = true;

    var mismatched =(spec.elementType == "video"
        && spec.experimentType.includes("-foraudio"))
        || (spec.elementType == "audio"
        && spec.experimentType.includes("-forvideo"));

    if (spec.autoplayType == "none" && spec.visType != 'onscreen')
        skip = true;
    else if (spec.experimentType.includes("-ifmuted")
        && spec.visType != "onscreen")
        skip = true;
    else if (spec.visType == "offscreen"
        && spec.autoplayType != "attr")
        skip = true;
    else if (!spec.experimentType.includes("-ifmuted")
        && spec.mutedType == "yes")
        skip = true;
    else if (spec.elementType == "audio" && spec.mutedType == "yes")
        skip = true;
    else if (spec.elementType == "audio" && spec.visType != "scroll")
        skip = true;
    else if (mismatched && spec.visType !="onscreen")
        skip = true;
    else if (mismatched && spec.autoplayType != "attr")
        skip = true;
    else if (spec.visType == "obscured"
        && !spec.experimentType.includes("-ifpagevisible"))
        skip = true;
    else if ((spec.visType == "offscreen" || spec.visType == "scroll"
        || spec.autoplayType != "attr" || spec.elementType != "video")
        && spec.experimentType.includes("-ifpagevisible"))
        skip = true;
    else if (spec.settingValue == "disabled"
        && ((spec.visType != "onscreen") || (spec.autoplayType != "play()")))
        skip = true;

    if (skip)
        queueNextExperiment();
    else
        runOneConfig(spec);
}

function start(mediaType, experiments) {
  elementTypes = [ mediaType ];
  experimentTypes = experiments;

  window.internals.settings.setMediaPlaybackRequiresUserGesture(true);
  runNextConfig();
}
