function extension_getRequestByUrl(urls, callback)
{
    function onHAR(response)
    {
        var entries = response.entries;
        for (var i = 0; i < entries.length; ++i) {
            for (var url = 0; url < urls.length; ++url) {
                if (urls[url].test(entries[i].request.url)) {
                    callback(entries[i]);
                    return;
                }
            }
        }
        output("no item found");
        callback(null);
    }
    webInspector.network.getHAR(onHAR);
}
