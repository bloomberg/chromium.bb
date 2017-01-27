function extension_runAudits(callback)
{
    evaluateOnFrontend("InspectorTest.startExtensionAudits(reply);", callback);
}

// runs in front-end
var initialize_ExtensionsAuditsTest = function()
{
    InspectorTest.startExtensionAudits = function(callback)
    {
        const launcherView = UI.panels.audits._launcherView;
        launcherView._selectAllClicked(false);
        launcherView._auditPresentStateElement.checked = true;

        var extensionCategories = document.querySelectorAll(".audit-categories-container > label");
        for (var i = 0; i < extensionCategories.length; ++i) {
            var shouldBeEnabled = extensionCategories[i].textContent.includes("Extension");
            if (!shouldBeEnabled && extensionCategories[i].textElement)
                shouldBeEnabled = extensionCategories[i].textElement.textContent.includes("Extension");
            if (shouldBeEnabled !== extensionCategories[i].checkboxElement.checked)
                extensionCategories[i].checkboxElement.click();
        }

        function onAuditsDone()
        {
            InspectorTest.collectAuditResults(callback);
        }
        InspectorTest.addSniffer(UI.panels.audits, "auditFinishedCallback", onAuditsDone, true);

        launcherView._launchButtonClicked();
    }

    InspectorTest.dumpAuditProgress = function()
    {
        var progress = document.querySelector(".progress-indicator").shadowRoot.querySelector("progress");
        InspectorTest.addResult("Progress: " + Math.round(100 * progress.value / progress.max) + "%");
    }

    // We will render DOM node results, so preload elements.
    InspectorTest.preloadPanel("elements");
    InspectorTest.preloadPanel("audits");
}
