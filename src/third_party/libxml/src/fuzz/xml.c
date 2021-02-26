/*
 * xml.c: a libFuzzer target to test several XML parser interfaces.
 *
 * See Copyright for the status of this software.
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xinclude.h>
#include <libxml/xmlreader.h>
#include "fuzz.h"

int
LLVMFuzzerInitialize(int *argc ATTRIBUTE_UNUSED,
                     char ***argv ATTRIBUTE_UNUSED) {
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, xmlFuzzErrorFunc);
    xmlSetExternalEntityLoader(xmlFuzzEntityLoader);

    return 0;
}

int
LLVMFuzzerTestOneInput(const char *data, size_t size) {
    static const size_t maxChunkSize = 128;
    xmlDocPtr doc;
    xmlParserCtxtPtr ctxt;
    xmlTextReaderPtr reader;
    xmlChar *out;
    const char *docBuffer, *docUrl;
    size_t docSize, consumed, chunkSize;
    int opts, outSize;

    xmlFuzzDataInit(data, size);
    opts = xmlFuzzReadInt();
    /* XML_PARSE_HUGE still causes timeouts. */
    opts &= ~XML_PARSE_HUGE;

    xmlFuzzReadEntities();
    docBuffer = xmlFuzzMainEntity(&docSize);
    docUrl = xmlFuzzMainUrl();
    if (docBuffer == NULL) {
        xmlFuzzDataCleanup();
        return(0);
    }

    /* Pull parser */

    doc = xmlReadMemory(docBuffer, docSize, docUrl, NULL, opts);
    if (opts & XML_PARSE_XINCLUDE)
        xmlXIncludeProcessFlags(doc, opts);
    /* Also test the serializer. */
    xmlDocDumpMemory(doc, &out, &outSize);
    xmlFree(out);
    xmlFreeDoc(doc);

    /* Push parser */

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, docUrl);
    xmlCtxtUseOptions(ctxt, opts);

    for (consumed = 0; consumed < docSize; consumed += chunkSize) {
        chunkSize = docSize - consumed;
        if (chunkSize > maxChunkSize)
            chunkSize = maxChunkSize;
        xmlParseChunk(ctxt, docBuffer + consumed, chunkSize, 0);
    }

    xmlParseChunk(ctxt, NULL, 0, 1);
    if (opts & XML_PARSE_XINCLUDE)
        xmlXIncludeProcessFlags(ctxt->myDoc, opts);
    xmlFreeDoc(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);

    /* Reader */

    reader = xmlReaderForMemory(docBuffer, docSize, NULL, NULL, opts);
    while (xmlTextReaderRead(reader) == 1) {
        if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE) {
            int i, n = xmlTextReaderAttributeCount(reader);
            for (i=0; i<n; i++) {
                xmlTextReaderMoveToAttributeNo(reader, i);
                while (xmlTextReaderReadAttributeValue(reader) == 1);
            }
        }
    }
    xmlFreeTextReader(reader);

    /* Cleanup */

    xmlFuzzDataCleanup();

    return(0);
}

