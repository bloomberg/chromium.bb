function initialize_DocumentationTests()
{

InspectorTest.killLoadXHRWithPrefix = function(urlPrefix)
{
    var originalLoadXHR = window.loadXHR;
    window.loadXHR = function(url)
    {
        if (url.startsWith(urlPrefix))
            return new Promise(load);

        return originalLoadXHR(url);

        function load(successCallback, failureCallback)
        {
            failureCallback();
        }
    }
}

}