// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RuleFeature.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/RuleSet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class RuleFeatureSetTest : public ::testing::Test {
public:
    RuleFeatureSetTest()
    {
    }

    void SetUp()
    {
        m_document = HTMLDocument::create();
        RefPtrWillBeRawPtr<HTMLHtmlElement> html = HTMLHtmlElement::create(*m_document);
        html->appendChild(HTMLBodyElement::create(*m_document));
        m_document->appendChild(html.release());

        m_document->body()->setInnerHTML("<b><i></i></b>", ASSERT_NO_EXCEPTION);
    }

    void updateInvalidationSets(const String& selectorText)
    {
        CSSSelectorList selectorList = CSSParser::parseSelector(strictCSSParserContext(), nullptr, selectorText);

        RefPtrWillBeRawPtr<StyleRule> styleRule = StyleRule::create(std::move(selectorList), MutableStylePropertySet::create(HTMLStandardMode));
        RuleData ruleData(styleRule.get(), 0, 0, RuleHasNoSpecialState);
        m_ruleFeatureSet.updateInvalidationSets(ruleData);
    }

    void collectInvalidationSetsForClass(InvalidationLists& invalidationLists, const AtomicString& className) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet.collectInvalidationSetsForClass(invalidationLists, *element, className);
    }

    void collectInvalidationSetsForId(InvalidationLists& invalidationLists, const AtomicString& id) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet.collectInvalidationSetsForId(invalidationLists, *element, id);
    }

    void collectInvalidationSetsForAttribute(InvalidationLists& invalidationLists, const QualifiedName& attributeName) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet.collectInvalidationSetsForAttribute(invalidationLists, *element, attributeName);
    }

    void collectInvalidationSetsForPseudoClass(InvalidationLists& invalidationLists, CSSSelector::PseudoType pseudo) const
    {
        Element* element = Traversal<HTMLElement>::firstChild(*Traversal<HTMLElement>::firstChild(*m_document->body()));
        m_ruleFeatureSet.collectInvalidationSetsForPseudoClass(invalidationLists, *element, pseudo);
    }

    const HashSet<AtomicString>& classSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.classSetForTesting();
    }

    const HashSet<AtomicString>& idSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.idSetForTesting();
    }

    const HashSet<AtomicString>& tagNameSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.tagNameSetForTesting();
    }

    const HashSet<AtomicString>& attributeSet(const InvalidationSet& invalidationSet)
    {
        return invalidationSet.attributeSetForTesting();
    }

    void expectNoInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(0u, invalidationSets.size());
    }

    void expectSelfInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        EXPECT_TRUE(invalidationSets[0]->invalidatesSelf());
    }

    void expectNoSelfInvalidation(InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        EXPECT_FALSE(invalidationSets[0]->invalidatesSelf());
    }

    void expectClassInvalidation(const AtomicString& className, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> classes = classSet(*invalidationSets[0]);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(className));
    }

    void expectSiblingInvalidation(unsigned maxDirectAdjacentSelectors, const AtomicString& siblingName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        const SiblingInvalidationSet& siblingInvalidationSet = toSiblingInvalidationSet(*invalidationSets[0]);
        HashSet<AtomicString> classes = classSet(siblingInvalidationSet);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(siblingName));
        EXPECT_EQ(maxDirectAdjacentSelectors, siblingInvalidationSet.maxDirectAdjacentSelectors());
    }

    void expectSiblingDescendantInvalidation(unsigned maxDirectAdjacentSelectors, const AtomicString& siblingName, const AtomicString& descendantName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        const SiblingInvalidationSet& siblingInvalidationSet = toSiblingInvalidationSet(*invalidationSets[0]);
        HashSet<AtomicString> classes = classSet(siblingInvalidationSet);
        EXPECT_EQ(1u, classes.size());
        EXPECT_TRUE(classes.contains(siblingName));
        EXPECT_EQ(maxDirectAdjacentSelectors, siblingInvalidationSet.maxDirectAdjacentSelectors());

        HashSet<AtomicString> descendantClasses = classSet(*siblingInvalidationSet.siblingDescendants());
        EXPECT_EQ(1u, descendantClasses.size());
        EXPECT_TRUE(descendantClasses.contains(descendantName));
    }

    void expectClassesInvalidation(const AtomicString& firstClassName, const AtomicString& secondClassName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> classes = classSet(*invalidationSets[0]);
        EXPECT_EQ(2u, classes.size());
        EXPECT_TRUE(classes.contains(firstClassName));
        EXPECT_TRUE(classes.contains(secondClassName));
    }

    void expectIdInvalidation(const AtomicString& id, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> ids = idSet(*invalidationSets[0]);
        EXPECT_EQ(1u, ids.size());
        EXPECT_TRUE(ids.contains(id));
    }

    void expectTagNameInvalidation(const AtomicString& tagName, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> tagNames = tagNameSet(*invalidationSets[0]);
        EXPECT_EQ(1u, tagNames.size());
        EXPECT_TRUE(tagNames.contains(tagName));
    }

    void expectAttributeInvalidation(const AtomicString& attribute, InvalidationSetVector& invalidationSets)
    {
        EXPECT_EQ(1u, invalidationSets.size());
        HashSet<AtomicString> attributes = attributeSet(*invalidationSets[0]);
        EXPECT_EQ(1u, attributes.size());
        EXPECT_TRUE(attributes.contains(attribute));
    }

    DEFINE_INLINE_TRACE()
    {
#if ENABLE(OILPAN)
        visitor->trace(m_ruleFeatureSet);
        visitor->trace(m_document);
#endif
    }

private:
    RuleFeatureSet m_ruleFeatureSet;
    RefPtrWillBePersistent<Document> m_document;
};

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling1)
{
    updateInvalidationSets(".p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "p");
    expectSelfInvalidation(invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling2)
{
    updateInvalidationSets(".o + .p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "o");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingInvalidation(1, "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling3)
{
    updateInvalidationSets(".m + .n .o + .p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "n");
    expectNoSelfInvalidation(invalidationLists.descendants);
    expectClassInvalidation("p", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling4)
{
    updateInvalidationSets(".m + .n .o + .p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "m");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(1, "n", "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling5)
{
    updateInvalidationSets(".l ~ .m + .n .o + .p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "l");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(UINT_MAX, "n", "p", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, interleavedDescendantSibling6)
{
    updateInvalidationSets(".k > .l ~ .m + .n .o + .p");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "k");
    expectClassInvalidation("p", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}


TEST_F(RuleFeatureSetTest, anySibling)
{
    updateInvalidationSets(":-webkit-any(.q, .r) ~ .s .t");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "q");
    expectNoInvalidation(invalidationLists.descendants);
    expectSiblingDescendantInvalidation(UINT_MAX, "s", "t", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, any)
{
    updateInvalidationSets(":-webkit-any(.w, .x)");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "w");
    expectSelfInvalidation(invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, siblingAny)
{
    updateInvalidationSets(".v ~ :-webkit-any(.w, .x)");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "v");
    expectNoInvalidation(invalidationLists.descendants);
    expectClassesInvalidation("w", "x", invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, descendantSiblingAny)
{
    updateInvalidationSets(".u .v ~ :-webkit-any(.w, .x)");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "u");
    expectClassesInvalidation("w", "x", invalidationLists.descendants);
    expectNoInvalidation(invalidationLists.siblings);
}

TEST_F(RuleFeatureSetTest, id)
{
    updateInvalidationSets("#a #b");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForId(invalidationLists, "a");
    expectIdInvalidation("b", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, attribute)
{
    updateInvalidationSets("[c] [d]");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForAttribute(invalidationLists, QualifiedName("", "c", ""));
    expectAttributeInvalidation("d", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, pseudoClass)
{
    updateInvalidationSets(":valid");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForPseudoClass(invalidationLists, CSSSelector::PseudoValid);
    expectSelfInvalidation(invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, tagName)
{
    updateInvalidationSets(":valid e");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForPseudoClass(invalidationLists, CSSSelector::PseudoValid);
    expectTagNameInvalidation("e", invalidationLists.descendants);
}

TEST_F(RuleFeatureSetTest, contentPseudo)
{
    updateInvalidationSets(".a ::content .b");
    updateInvalidationSets(".a .c");

    InvalidationLists invalidationLists;
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectClassInvalidation("c", invalidationLists.descendants);

    updateInvalidationSets(".a .b");

    invalidationLists.descendants.clear();
    collectInvalidationSetsForClass(invalidationLists, "a");
    expectClassesInvalidation("b", "c", invalidationLists.descendants);
}

} // namespace blink
