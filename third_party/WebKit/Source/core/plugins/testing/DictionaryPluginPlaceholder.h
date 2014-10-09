// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryPluginPlaceholder_h
#define DictionaryPluginPlaceholder_h

#include "bindings/core/v8/Dictionary.h"
#include "core/html/shadow/PluginPlaceholderElement.h"
#include "core/plugins/PluginPlaceholder.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Manipulates a plugin placeholder element based on a fixed dictionary given.
// Used for layout tests that examine the formatting of structured placeholders.
class DictionaryPluginPlaceholder : public NoBaseWillBeGarbageCollected<DictionaryPluginPlaceholder>, public PluginPlaceholder {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DictionaryPluginPlaceholder);
public:
    static PassOwnPtrWillBeRawPtr<DictionaryPluginPlaceholder> create(Document& document, const Dictionary& options)
    {
        RefPtrWillBeRawPtr<PluginPlaceholderElement> placeholder = PluginPlaceholderElement::create(document);
        String stringValue;
        if (DictionaryHelper::get(options, "message", stringValue))
            placeholder->setMessage(stringValue);

        return adoptPtrWillBeNoop(new DictionaryPluginPlaceholder(placeholder.release()));
    }

#if !ENABLE(OILPAN)
    virtual ~DictionaryPluginPlaceholder() override { }
#endif
    virtual void trace(Visitor* visitor) { visitor->trace(m_pluginPlaceholderElement); }

    virtual void loadIntoContainer(ContainerNode& container) override
    {
        container.removeChildren();
        container.appendChild(m_pluginPlaceholderElement, ASSERT_NO_EXCEPTION);
    }

private:
    DictionaryPluginPlaceholder(PassRefPtrWillBeRawPtr<PluginPlaceholderElement> element) : m_pluginPlaceholderElement(element) { }

    RefPtrWillBeMember<PluginPlaceholderElement> m_pluginPlaceholderElement;
};

} // namespace blink

#endif // DictionaryPluginPlaceholder_h
