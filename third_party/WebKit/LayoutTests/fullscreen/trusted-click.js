// Invokes callback from a trusted event.
// When testing manually, a button is added to the container.
function trusted_click(callback, container)
{
    var document = container.ownerDocument;

    if (window.testRunner) {
        // Running under LayoutTests. Use timeout to be async.
        setTimeout(function()
        {
            document.addEventListener("click", callback);
            eventSender.mouseDown();
            eventSender.mouseUp();
            document.removeEventListener("click", callback);
        }, 0);
    } else {
        // Running as manual test. Show a button to click.
        var button = document.createElement("button");
        button.textContent = "click to run test";
        button.style.display = "block";
        button.style.fontSize = "20px";
        button.style.padding = "10px";
        button.onclick = function()
        {
            callback();
            button.onclick = null;
            container.removeChild(button);
        };
        container.appendChild(button);
    }
}

// Invokes element.requestFullscreen() from a trusted event.
// When testing manually, a button is added to the container,
// or to element's parent if no container is provided.
function trusted_request(element, container)
{
    var request = element.requestFullscreen.bind(element);
    trusted_click(request, container || element.parentNode);
}
