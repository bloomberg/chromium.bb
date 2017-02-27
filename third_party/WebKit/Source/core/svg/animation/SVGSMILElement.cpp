/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/animation/SVGSMILElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/XLinkNames.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTreeScopeResources.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "platform/heap/Handle.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

class RepeatEvent final : public Event {
 public:
  static RepeatEvent* create(const AtomicString& type, int repeat) {
    return new RepeatEvent(type, false, false, repeat);
  }

  ~RepeatEvent() override {}

  int repeat() const { return m_repeat; }

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::trace(visitor); }

 protected:
  RepeatEvent(const AtomicString& type,
              bool canBubble,
              bool cancelable,
              int repeat = -1)
      : Event(type, canBubble, cancelable), m_repeat(repeat) {}

 private:
  int m_repeat;
};

inline RepeatEvent* toRepeatEvent(Event* event) {
  SECURITY_DCHECK(!event || event->type() == "repeatn");
  return static_cast<RepeatEvent*>(event);
}

// This is used for duration type time values that can't be negative.
static const double invalidCachedTime = -1.;

class ConditionEventListener final : public EventListener {
 public:
  static ConditionEventListener* create(SVGSMILElement* animation,
                                        SVGSMILElement::Condition* condition) {
    return new ConditionEventListener(animation, condition);
  }

  static const ConditionEventListener* cast(const EventListener* listener) {
    return listener->type() == ConditionEventListenerType
               ? static_cast<const ConditionEventListener*>(listener)
               : nullptr;
  }

  bool operator==(const EventListener& other) const override;

  void disconnectAnimation() { m_animation = nullptr; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_animation);
    visitor->trace(m_condition);
    EventListener::trace(visitor);
  }

 private:
  ConditionEventListener(SVGSMILElement* animation,
                         SVGSMILElement::Condition* condition)
      : EventListener(ConditionEventListenerType),
        m_animation(animation),
        m_condition(condition) {}

  void handleEvent(ExecutionContext*, Event*) override;

  Member<SVGSMILElement> m_animation;
  Member<SVGSMILElement::Condition> m_condition;
};

bool ConditionEventListener::operator==(const EventListener& listener) const {
  if (const ConditionEventListener* conditionEventListener =
          ConditionEventListener::cast(&listener))
    return m_animation == conditionEventListener->m_animation &&
           m_condition == conditionEventListener->m_condition;
  return false;
}

void ConditionEventListener::handleEvent(ExecutionContext*, Event* event) {
  if (!m_animation)
    return;
  if (event->type() == "repeatn" &&
      toRepeatEvent(event)->repeat() != m_condition->repeat())
    return;
  m_animation->addInstanceTime(m_condition->getBeginOrEnd(),
                               m_animation->elapsed() + m_condition->offset());
}

SVGSMILElement::Condition::Condition(Type type,
                                     BeginOrEnd beginOrEnd,
                                     const AtomicString& baseID,
                                     const AtomicString& name,
                                     SMILTime offset,
                                     int repeat)
    : m_type(type),
      m_beginOrEnd(beginOrEnd),
      m_baseID(baseID),
      m_name(name),
      m_offset(offset),
      m_repeat(repeat) {}

SVGSMILElement::Condition::~Condition() = default;

DEFINE_TRACE(SVGSMILElement::Condition) {
  visitor->trace(m_syncBase);
  visitor->trace(m_eventListener);
}

void SVGSMILElement::Condition::connectSyncBase(SVGSMILElement& timedElement) {
  DCHECK(!m_baseID.isEmpty());
  Element* element = timedElement.treeScope().getElementById(m_baseID);
  if (!element || !isSVGSMILElement(*element)) {
    m_syncBase = nullptr;
    return;
  }
  m_syncBase = toSVGSMILElement(element);
  m_syncBase->addSyncBaseDependent(timedElement);
}

void SVGSMILElement::Condition::disconnectSyncBase(
    SVGSMILElement& timedElement) {
  if (!m_syncBase)
    return;
  m_syncBase->removeSyncBaseDependent(timedElement);
  m_syncBase = nullptr;
}

SVGElement* SVGSMILElement::Condition::lookupEventBase(
    SVGSMILElement& timedElement) const {
  Element* eventBase = m_baseID.isEmpty()
                           ? timedElement.targetElement()
                           : timedElement.treeScope().getElementById(m_baseID);
  if (!eventBase || !eventBase->isSVGElement())
    return nullptr;
  return toSVGElement(eventBase);
}

void SVGSMILElement::Condition::connectEventBase(SVGSMILElement& timedElement) {
  DCHECK(!m_syncBase);
  SVGElement* eventBase = lookupEventBase(timedElement);
  if (!eventBase) {
    if (m_baseID.isEmpty())
      return;
    SVGTreeScopeResources& treeScopeResources =
        timedElement.treeScope().ensureSVGTreeScopedResources();
    if (!treeScopeResources.isElementPendingResource(timedElement, m_baseID))
      treeScopeResources.addPendingResource(m_baseID, timedElement);
    return;
  }
  DCHECK(!m_eventListener);
  m_eventListener = ConditionEventListener::create(&timedElement, this);
  eventBase->addEventListener(m_name, m_eventListener, false);
  timedElement.addReferenceTo(eventBase);
}

void SVGSMILElement::Condition::disconnectEventBase(
    SVGSMILElement& timedElement) {
  DCHECK(!m_syncBase);
  if (!m_eventListener)
    return;
  // Note: It's a memory optimization to try to remove our condition event
  // listener, but it's not guaranteed to work, since we have no guarantee that
  // we will be able to find our condition's original eventBase. So, we also
  // have to disconnect ourselves from our condition event listener, in case it
  // later fires.
  if (SVGElement* eventBase = lookupEventBase(timedElement))
    eventBase->removeEventListener(m_name, m_eventListener, false);
  m_eventListener->disconnectAnimation();
  m_eventListener = nullptr;
}

SVGSMILElement::SVGSMILElement(const QualifiedName& tagName, Document& doc)
    : SVGElement(tagName, doc),
      SVGTests(this),
      m_attributeName(anyQName()),
      m_targetElement(nullptr),
      m_syncBaseConditionsConnected(false),
      m_hasEndEventConditions(false),
      m_isWaitingForFirstInterval(true),
      m_isScheduled(false),
      m_interval(SMILInterval(SMILTime::unresolved(), SMILTime::unresolved())),
      m_previousIntervalBegin(SMILTime::unresolved()),
      m_activeState(Inactive),
      m_lastPercent(0),
      m_lastRepeat(0),
      m_nextProgressTime(0),
      m_documentOrderIndex(0),
      m_cachedDur(invalidCachedTime),
      m_cachedRepeatDur(invalidCachedTime),
      m_cachedRepeatCount(invalidCachedTime),
      m_cachedMin(invalidCachedTime),
      m_cachedMax(invalidCachedTime) {
  resolveFirstInterval();
}

SVGSMILElement::~SVGSMILElement() {}

void SVGSMILElement::clearResourceAndEventBaseReferences() {
  removeAllOutgoingReferences();
}

void SVGSMILElement::clearConditions() {
  disconnectSyncBaseConditions();
  disconnectEventBaseConditions();
  m_conditions.clear();
}

void SVGSMILElement::buildPendingResource() {
  clearResourceAndEventBaseReferences();

  if (!isConnected()) {
    // Reset the target element if we are no longer in the document.
    setTargetElement(nullptr);
    return;
  }

  AtomicString id;
  const AtomicString& href = SVGURIReference::legacyHrefString(*this);
  Element* target;
  if (href.isEmpty())
    target = parentNode() && parentNode()->isElementNode()
                 ? toElement(parentNode())
                 : nullptr;
  else
    target =
        SVGURIReference::targetElementFromIRIString(href, treeScope(), &id);
  SVGElement* svgTarget =
      target && target->isSVGElement() ? toSVGElement(target) : nullptr;

  if (svgTarget && !svgTarget->isConnected())
    svgTarget = nullptr;

  if (svgTarget != targetElement())
    setTargetElement(svgTarget);

  if (!svgTarget) {
    // Do not register as pending if we are already pending this resource.
    if (treeScope().ensureSVGTreeScopedResources().isElementPendingResource(
            *this, id))
      return;
    if (!id.isEmpty()) {
      treeScope().ensureSVGTreeScopedResources().addPendingResource(id, *this);
      DCHECK(hasPendingResources());
    }
  } else {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    addReferenceTo(svgTarget);
  }
  connectEventBaseConditions();
}

static inline void clearTimesWithDynamicOrigins(
    Vector<SMILTimeWithOrigin>& timeList) {
  for (int i = timeList.size() - 1; i >= 0; --i) {
    if (timeList[i].originIsScript())
      timeList.remove(i);
  }
}

void SVGSMILElement::reset() {
  clearAnimatedType();

  m_activeState = Inactive;
  m_isWaitingForFirstInterval = true;
  m_interval.begin = SMILTime::unresolved();
  m_interval.end = SMILTime::unresolved();
  m_previousIntervalBegin = SMILTime::unresolved();
  m_lastPercent = 0;
  m_lastRepeat = 0;
  m_nextProgressTime = 0;
  resolveFirstInterval();
}

Node::InsertionNotificationRequest SVGSMILElement::insertedInto(
    ContainerNode* rootParent) {
  SVGElement::insertedInto(rootParent);

  if (!rootParent->isConnected())
    return InsertionDone;

  UseCounter::count(document(), UseCounter::SVGSMILElementInDocument);
  if (document().isLoadCompleted())
    UseCounter::count(&document(), UseCounter::SVGSMILElementInsertedAfterLoad);

  SVGSVGElement* owner = ownerSVGElement();
  if (!owner)
    return InsertionDone;

  m_timeContainer = owner->timeContainer();
  ASSERT(m_timeContainer);
  m_timeContainer->setDocumentOrderIndexesDirty();

  // "If no attribute is present, the default begin value (an offset-value of 0)
  // must be evaluated."
  if (!fastHasAttribute(SVGNames::beginAttr))
    m_beginTimes.push_back(SMILTimeWithOrigin());

  if (m_isWaitingForFirstInterval)
    resolveFirstInterval();

  if (m_timeContainer)
    m_timeContainer->notifyIntervalsChanged();

  buildPendingResource();

  return InsertionDone;
}

void SVGSMILElement::removedFrom(ContainerNode* rootParent) {
  if (rootParent->isConnected()) {
    clearResourceAndEventBaseReferences();
    clearConditions();
    setTargetElement(nullptr);
    animationAttributeChanged();
    m_timeContainer = nullptr;
  }

  SVGElement::removedFrom(rootParent);
}

SMILTime SVGSMILElement::parseOffsetValue(const String& data) {
  bool ok;
  double result = 0;
  String parse = data.stripWhiteSpace();
  if (parse.endsWith('h'))
    result = parse.left(parse.length() - 1).toDouble(&ok) * 60 * 60;
  else if (parse.endsWith("min"))
    result = parse.left(parse.length() - 3).toDouble(&ok) * 60;
  else if (parse.endsWith("ms"))
    result = parse.left(parse.length() - 2).toDouble(&ok) / 1000;
  else if (parse.endsWith('s'))
    result = parse.left(parse.length() - 1).toDouble(&ok);
  else
    result = parse.toDouble(&ok);
  if (!ok || !SMILTime(result).isFinite())
    return SMILTime::unresolved();
  return result;
}

SMILTime SVGSMILElement::parseClockValue(const String& data) {
  if (data.isNull())
    return SMILTime::unresolved();

  String parse = data.stripWhiteSpace();

  DEFINE_STATIC_LOCAL(const AtomicString, indefiniteValue, ("indefinite"));
  if (parse == indefiniteValue)
    return SMILTime::indefinite();

  double result = 0;
  bool ok;
  size_t doublePointOne = parse.find(':');
  size_t doublePointTwo = parse.find(':', doublePointOne + 1);
  if (doublePointOne == 2 && doublePointTwo == 5 && parse.length() >= 8) {
    result += parse.substring(0, 2).toUIntStrict(&ok) * 60 * 60;
    if (!ok)
      return SMILTime::unresolved();
    result += parse.substring(3, 2).toUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::unresolved();
    result += parse.substring(6).toDouble(&ok);
  } else if (doublePointOne == 2 && doublePointTwo == kNotFound &&
             parse.length() >= 5) {
    result += parse.substring(0, 2).toUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::unresolved();
    result += parse.substring(3).toDouble(&ok);
  } else
    return parseOffsetValue(parse);

  if (!ok || !SMILTime(result).isFinite())
    return SMILTime::unresolved();
  return result;
}

static void sortTimeList(Vector<SMILTimeWithOrigin>& timeList) {
  std::sort(timeList.begin(), timeList.end());
}

bool SVGSMILElement::parseCondition(const String& value,
                                    BeginOrEnd beginOrEnd) {
  String parseString = value.stripWhiteSpace();

  double sign = 1.;
  bool ok;
  size_t pos = parseString.find('+');
  if (pos == kNotFound) {
    pos = parseString.find('-');
    if (pos != kNotFound)
      sign = -1.;
  }
  String conditionString;
  SMILTime offset = 0;
  if (pos == kNotFound)
    conditionString = parseString;
  else {
    conditionString = parseString.left(pos).stripWhiteSpace();
    String offsetString = parseString.substring(pos + 1).stripWhiteSpace();
    offset = parseOffsetValue(offsetString);
    if (offset.isUnresolved())
      return false;
    offset = offset * sign;
  }
  if (conditionString.isEmpty())
    return false;
  pos = conditionString.find('.');

  String baseID;
  String nameString;
  if (pos == kNotFound)
    nameString = conditionString;
  else {
    baseID = conditionString.left(pos);
    nameString = conditionString.substring(pos + 1);
  }
  if (nameString.isEmpty())
    return false;

  Condition::Type type;
  int repeat = -1;
  if (nameString.startsWith("repeat(") && nameString.endsWith(')')) {
    repeat = nameString.substring(7, nameString.length() - 8).toUIntStrict(&ok);
    if (!ok)
      return false;
    nameString = "repeatn";
    type = Condition::EventBase;
  } else if (nameString == "begin" || nameString == "end") {
    if (baseID.isEmpty())
      return false;
    UseCounter::count(&document(), UseCounter::SVGSMILBeginOrEndSyncbaseValue);
    type = Condition::Syncbase;
  } else if (nameString.startsWith("accesskey(")) {
    // FIXME: accesskey() support.
    type = Condition::AccessKey;
  } else {
    UseCounter::count(&document(), UseCounter::SVGSMILBeginOrEndEventValue);
    type = Condition::EventBase;
  }

  m_conditions.push_back(
      Condition::create(type, beginOrEnd, AtomicString(baseID),
                        AtomicString(nameString), offset, repeat));

  if (type == Condition::EventBase && beginOrEnd == End)
    m_hasEndEventConditions = true;

  return true;
}

void SVGSMILElement::parseBeginOrEnd(const String& parseString,
                                     BeginOrEnd beginOrEnd) {
  Vector<SMILTimeWithOrigin>& timeList =
      beginOrEnd == Begin ? m_beginTimes : m_endTimes;
  if (beginOrEnd == End)
    m_hasEndEventConditions = false;
  HashSet<SMILTime> existing;
  for (unsigned n = 0; n < timeList.size(); ++n) {
    if (!timeList[n].time().isUnresolved())
      existing.insert(timeList[n].time().value());
  }
  Vector<String> splitString;
  parseString.split(';', splitString);
  for (unsigned n = 0; n < splitString.size(); ++n) {
    SMILTime value = parseClockValue(splitString[n]);
    if (value.isUnresolved())
      parseCondition(splitString[n], beginOrEnd);
    else if (!existing.contains(value.value()))
      timeList.push_back(
          SMILTimeWithOrigin(value, SMILTimeWithOrigin::ParserOrigin));
  }
  sortTimeList(timeList);
}

void SVGSMILElement::parseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.newValue;
  if (name == SVGNames::beginAttr) {
    if (!m_conditions.isEmpty()) {
      clearConditions();
      parseBeginOrEnd(fastGetAttribute(SVGNames::endAttr), End);
    }
    parseBeginOrEnd(value.getString(), Begin);
    if (isConnected()) {
      connectSyncBaseConditions();
      connectEventBaseConditions();
      beginListChanged(elapsed());
    }
    animationAttributeChanged();
  } else if (name == SVGNames::endAttr) {
    if (!m_conditions.isEmpty()) {
      clearConditions();
      parseBeginOrEnd(fastGetAttribute(SVGNames::beginAttr), Begin);
    }
    parseBeginOrEnd(value.getString(), End);
    if (isConnected()) {
      connectSyncBaseConditions();
      connectEventBaseConditions();
      endListChanged(elapsed());
    }
    animationAttributeChanged();
  } else if (name == SVGNames::onbeginAttr) {
    setAttributeEventListener(
        EventTypeNames::beginEvent,
        createAttributeEventListener(this, name, value, eventParameterName()));
  } else if (name == SVGNames::onendAttr) {
    setAttributeEventListener(
        EventTypeNames::endEvent,
        createAttributeEventListener(this, name, value, eventParameterName()));
  } else if (name == SVGNames::onrepeatAttr) {
    setAttributeEventListener(
        EventTypeNames::repeatEvent,
        createAttributeEventListener(this, name, value, eventParameterName()));
  } else {
    SVGElement::parseAttribute(params);
  }
}

void SVGSMILElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (attrName == SVGNames::durAttr) {
    m_cachedDur = invalidCachedTime;
  } else if (attrName == SVGNames::repeatDurAttr) {
    m_cachedRepeatDur = invalidCachedTime;
  } else if (attrName == SVGNames::repeatCountAttr) {
    m_cachedRepeatCount = invalidCachedTime;
  } else if (attrName == SVGNames::minAttr) {
    m_cachedMin = invalidCachedTime;
  } else if (attrName == SVGNames::maxAttr) {
    m_cachedMax = invalidCachedTime;
  } else if (attrName.matches(SVGNames::hrefAttr) ||
             attrName.matches(XLinkNames::hrefAttr)) {
    // TODO(fs): Could be smarter here when 'href' is specified and 'xlink:href'
    // is changed.
    SVGElement::InvalidationGuard invalidationGuard(this);
    buildPendingResource();
    if (m_targetElement)
      clearAnimatedType();
  } else {
    SVGElement::svgAttributeChanged(attrName);
    return;
  }

  animationAttributeChanged();
}

void SVGSMILElement::connectSyncBaseConditions() {
  if (m_syncBaseConditionsConnected)
    disconnectSyncBaseConditions();
  m_syncBaseConditionsConnected = true;
  for (Condition* condition : m_conditions) {
    if (condition->getType() == Condition::Syncbase)
      condition->connectSyncBase(*this);
  }
}

void SVGSMILElement::disconnectSyncBaseConditions() {
  if (!m_syncBaseConditionsConnected)
    return;
  m_syncBaseConditionsConnected = false;
  for (Condition* condition : m_conditions) {
    if (condition->getType() == Condition::Syncbase)
      condition->disconnectSyncBase(*this);
  }
}

void SVGSMILElement::connectEventBaseConditions() {
  disconnectEventBaseConditions();
  for (Condition* condition : m_conditions) {
    if (condition->getType() == Condition::EventBase)
      condition->connectEventBase(*this);
  }
}

void SVGSMILElement::disconnectEventBaseConditions() {
  for (Condition* condition : m_conditions) {
    if (condition->getType() == Condition::EventBase)
      condition->disconnectEventBase(*this);
  }
}

void SVGSMILElement::setTargetElement(SVGElement* target) {
  unscheduleIfScheduled();

  if (m_targetElement) {
    // Clear values that may depend on the previous target.
    clearAnimatedType();
    disconnectSyncBaseConditions();
  }

  // If the animation state is not Inactive, always reset to a clear state
  // before leaving the old target element.
  if (m_activeState != Inactive)
    endedActiveInterval();

  m_targetElement = target;
  schedule();
}

SMILTime SVGSMILElement::elapsed() const {
  return m_timeContainer ? m_timeContainer->elapsed() : 0;
}

bool SVGSMILElement::isFrozen() const {
  return m_activeState == Frozen;
}

SVGSMILElement::Restart SVGSMILElement::getRestart() const {
  DEFINE_STATIC_LOCAL(const AtomicString, never, ("never"));
  DEFINE_STATIC_LOCAL(const AtomicString, whenNotActive, ("whenNotActive"));
  const AtomicString& value = fastGetAttribute(SVGNames::restartAttr);
  if (value == never)
    return RestartNever;
  if (value == whenNotActive)
    return RestartWhenNotActive;
  return RestartAlways;
}

SVGSMILElement::FillMode SVGSMILElement::fill() const {
  DEFINE_STATIC_LOCAL(const AtomicString, freeze, ("freeze"));
  const AtomicString& value = fastGetAttribute(SVGNames::fillAttr);
  return value == freeze ? FillFreeze : FillRemove;
}

SMILTime SVGSMILElement::dur() const {
  if (m_cachedDur != invalidCachedTime)
    return m_cachedDur;
  const AtomicString& value = fastGetAttribute(SVGNames::durAttr);
  SMILTime clockValue = parseClockValue(value);
  return m_cachedDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
}

SMILTime SVGSMILElement::repeatDur() const {
  if (m_cachedRepeatDur != invalidCachedTime)
    return m_cachedRepeatDur;
  const AtomicString& value = fastGetAttribute(SVGNames::repeatDurAttr);
  SMILTime clockValue = parseClockValue(value);
  m_cachedRepeatDur = clockValue <= 0 ? SMILTime::unresolved() : clockValue;
  return m_cachedRepeatDur;
}

// So a count is not really a time but let just all pretend we did not notice.
SMILTime SVGSMILElement::repeatCount() const {
  if (m_cachedRepeatCount != invalidCachedTime)
    return m_cachedRepeatCount;
  SMILTime computedRepeatCount = SMILTime::unresolved();
  const AtomicString& value = fastGetAttribute(SVGNames::repeatCountAttr);
  if (!value.isNull()) {
    DEFINE_STATIC_LOCAL(const AtomicString, indefiniteValue, ("indefinite"));
    if (value == indefiniteValue) {
      computedRepeatCount = SMILTime::indefinite();
    } else {
      bool ok;
      double result = value.toDouble(&ok);
      if (ok && result > 0)
        computedRepeatCount = result;
    }
  }
  m_cachedRepeatCount = computedRepeatCount;
  return m_cachedRepeatCount;
}

SMILTime SVGSMILElement::maxValue() const {
  if (m_cachedMax != invalidCachedTime)
    return m_cachedMax;
  const AtomicString& value = fastGetAttribute(SVGNames::maxAttr);
  SMILTime result = parseClockValue(value);
  return m_cachedMax = (result.isUnresolved() || result <= 0)
                           ? SMILTime::indefinite()
                           : result;
}

SMILTime SVGSMILElement::minValue() const {
  if (m_cachedMin != invalidCachedTime)
    return m_cachedMin;
  const AtomicString& value = fastGetAttribute(SVGNames::minAttr);
  SMILTime result = parseClockValue(value);
  return m_cachedMin = (result.isUnresolved() || result < 0) ? 0 : result;
}

SMILTime SVGSMILElement::simpleDuration() const {
  return std::min(dur(), SMILTime::indefinite());
}

void SVGSMILElement::addInstanceTime(BeginOrEnd beginOrEnd,
                                     SMILTime time,
                                     SMILTimeWithOrigin::Origin origin) {
  SMILTime elapsed = this->elapsed();
  if (elapsed.isUnresolved())
    return;
  Vector<SMILTimeWithOrigin>& list =
      beginOrEnd == Begin ? m_beginTimes : m_endTimes;
  list.push_back(SMILTimeWithOrigin(time, origin));
  sortTimeList(list);
  if (beginOrEnd == Begin)
    beginListChanged(elapsed);
  else
    endListChanged(elapsed);
}

inline bool compareTimes(const SMILTimeWithOrigin& left,
                         const SMILTimeWithOrigin& right) {
  return left.time() < right.time();
}

SMILTime SVGSMILElement::findInstanceTime(BeginOrEnd beginOrEnd,
                                          SMILTime minimumTime,
                                          bool equalsMinimumOK) const {
  const Vector<SMILTimeWithOrigin>& list =
      beginOrEnd == Begin ? m_beginTimes : m_endTimes;
  int sizeOfList = list.size();

  if (!sizeOfList)
    return beginOrEnd == Begin ? SMILTime::unresolved()
                               : SMILTime::indefinite();

  const SMILTimeWithOrigin dummyTimeWithOrigin(
      minimumTime, SMILTimeWithOrigin::ParserOrigin);
  const SMILTimeWithOrigin* result = std::lower_bound(
      list.begin(), list.end(), dummyTimeWithOrigin, compareTimes);
  int indexOfResult = result - list.begin();
  if (indexOfResult == sizeOfList)
    return SMILTime::unresolved();
  const SMILTime& currentTime = list[indexOfResult].time();

  // The special value "indefinite" does not yield an instance time in the begin
  // list.
  if (currentTime.isIndefinite() && beginOrEnd == Begin)
    return SMILTime::unresolved();

  if (currentTime > minimumTime)
    return currentTime;

  ASSERT(currentTime == minimumTime);
  if (equalsMinimumOK)
    return currentTime;

  // If the equals is not accepted, return the next bigger item in the list.
  SMILTime nextTime = currentTime;
  while (indexOfResult < sizeOfList - 1) {
    nextTime = list[indexOfResult + 1].time();
    if (nextTime > minimumTime)
      return nextTime;
    ++indexOfResult;
  }

  return beginOrEnd == Begin ? SMILTime::unresolved() : SMILTime::indefinite();
}

SMILTime SVGSMILElement::repeatingDuration() const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime repeatCount = this->repeatCount();
  SMILTime repeatDur = this->repeatDur();
  SMILTime simpleDuration = this->simpleDuration();
  if (!simpleDuration ||
      (repeatDur.isUnresolved() && repeatCount.isUnresolved()))
    return simpleDuration;
  repeatDur = std::min(repeatDur, SMILTime::indefinite());
  SMILTime repeatCountDuration = simpleDuration * repeatCount;
  if (!repeatCountDuration.isUnresolved())
    return std::min(repeatDur, repeatCountDuration);
  return repeatDur;
}

SMILTime SVGSMILElement::resolveActiveEnd(SMILTime resolvedBegin,
                                          SMILTime resolvedEnd) const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime preliminaryActiveDuration;
  if (!resolvedEnd.isUnresolved() && dur().isUnresolved() &&
      repeatDur().isUnresolved() && repeatCount().isUnresolved())
    preliminaryActiveDuration = resolvedEnd - resolvedBegin;
  else if (!resolvedEnd.isFinite())
    preliminaryActiveDuration = repeatingDuration();
  else
    preliminaryActiveDuration =
        std::min(repeatingDuration(), resolvedEnd - resolvedBegin);

  SMILTime minValue = this->minValue();
  SMILTime maxValue = this->maxValue();
  if (minValue > maxValue) {
    // Ignore both.
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#MinMax
    minValue = 0;
    maxValue = SMILTime::indefinite();
  }
  return resolvedBegin +
         std::min(maxValue, std::max(minValue, preliminaryActiveDuration));
}

SMILInterval SVGSMILElement::resolveInterval(
    IntervalSelector intervalSelector) const {
  bool first = intervalSelector == FirstInterval;
  // See the pseudocode in http://www.w3.org/TR/SMIL3/smil-timing.html#q90.
  SMILTime beginAfter =
      first ? -std::numeric_limits<double>::infinity() : m_interval.end;
  SMILTime lastIntervalTempEnd = std::numeric_limits<double>::infinity();
  while (true) {
    bool equalsMinimumOK = !first || m_interval.end > m_interval.begin;
    SMILTime tempBegin = findInstanceTime(Begin, beginAfter, equalsMinimumOK);
    if (tempBegin.isUnresolved())
      break;
    SMILTime tempEnd;
    if (m_endTimes.isEmpty())
      tempEnd = resolveActiveEnd(tempBegin, SMILTime::indefinite());
    else {
      tempEnd = findInstanceTime(End, tempBegin, true);
      if ((first && tempBegin == tempEnd && tempEnd == lastIntervalTempEnd) ||
          (!first && tempEnd == m_interval.end))
        tempEnd = findInstanceTime(End, tempBegin, false);
      if (tempEnd.isUnresolved()) {
        if (!m_endTimes.isEmpty() && !m_hasEndEventConditions)
          break;
      }
      tempEnd = resolveActiveEnd(tempBegin, tempEnd);
    }
    if (!first || (tempEnd > 0 || (!tempBegin.value() && !tempEnd.value())))
      return SMILInterval(tempBegin, tempEnd);

    beginAfter = tempEnd;
    lastIntervalTempEnd = tempEnd;
  }
  return SMILInterval(SMILTime::unresolved(), SMILTime::unresolved());
}

void SVGSMILElement::resolveFirstInterval() {
  SMILInterval firstInterval = resolveInterval(FirstInterval);
  ASSERT(!firstInterval.begin.isIndefinite());

  if (!firstInterval.begin.isUnresolved() && firstInterval != m_interval) {
    m_interval = firstInterval;
    notifyDependentsIntervalChanged();
    m_nextProgressTime = m_nextProgressTime.isUnresolved()
                             ? m_interval.begin
                             : std::min(m_nextProgressTime, m_interval.begin);

    if (m_timeContainer)
      m_timeContainer->notifyIntervalsChanged();
  }
}

bool SVGSMILElement::resolveNextInterval() {
  SMILInterval nextInterval = resolveInterval(NextInterval);
  ASSERT(!nextInterval.begin.isIndefinite());

  if (!nextInterval.begin.isUnresolved() &&
      nextInterval.begin != m_interval.begin) {
    m_interval = nextInterval;
    notifyDependentsIntervalChanged();
    m_nextProgressTime = m_nextProgressTime.isUnresolved()
                             ? m_interval.begin
                             : std::min(m_nextProgressTime, m_interval.begin);
    return true;
  }

  return false;
}

SMILTime SVGSMILElement::nextProgressTime() const {
  return m_nextProgressTime;
}

void SVGSMILElement::beginListChanged(SMILTime eventTime) {
  if (m_isWaitingForFirstInterval) {
    resolveFirstInterval();
  } else if (this->getRestart() != RestartNever) {
    SMILTime newBegin = findInstanceTime(Begin, eventTime, true);
    if (newBegin.isFinite() &&
        (m_interval.end <= eventTime || newBegin < m_interval.begin)) {
      // Begin time changed, re-resolve the interval.
      SMILTime oldBegin = m_interval.begin;
      m_interval.end = eventTime;
      m_interval = resolveInterval(NextInterval);
      ASSERT(!m_interval.begin.isUnresolved());
      if (m_interval.begin != oldBegin) {
        if (m_activeState == Active && m_interval.begin > eventTime) {
          m_activeState = determineActiveState(eventTime);
          if (m_activeState != Active)
            endedActiveInterval();
        }
        notifyDependentsIntervalChanged();
      }
    }
  }
  m_nextProgressTime = elapsed();

  if (m_timeContainer)
    m_timeContainer->notifyIntervalsChanged();
}

void SVGSMILElement::endListChanged(SMILTime) {
  SMILTime elapsed = this->elapsed();
  if (m_isWaitingForFirstInterval) {
    resolveFirstInterval();
  } else if (elapsed < m_interval.end && m_interval.begin.isFinite()) {
    SMILTime newEnd = findInstanceTime(End, m_interval.begin, false);
    if (newEnd < m_interval.end) {
      newEnd = resolveActiveEnd(m_interval.begin, newEnd);
      if (newEnd != m_interval.end) {
        m_interval.end = newEnd;
        notifyDependentsIntervalChanged();
      }
    }
  }
  m_nextProgressTime = elapsed;

  if (m_timeContainer)
    m_timeContainer->notifyIntervalsChanged();
}

SVGSMILElement::RestartedInterval SVGSMILElement::maybeRestartInterval(
    double elapsed) {
  DCHECK(!m_isWaitingForFirstInterval);
  DCHECK(elapsed >= m_interval.begin);

  Restart restart = this->getRestart();
  if (restart == RestartNever)
    return DidNotRestartInterval;

  if (elapsed < m_interval.end) {
    if (restart != RestartAlways)
      return DidNotRestartInterval;
    SMILTime nextBegin = findInstanceTime(Begin, m_interval.begin, false);
    if (nextBegin < m_interval.end) {
      m_interval.end = nextBegin;
      notifyDependentsIntervalChanged();
    }
  }

  if (elapsed >= m_interval.end) {
    if (resolveNextInterval() && elapsed >= m_interval.begin)
      return DidRestartInterval;
  }
  return DidNotRestartInterval;
}

void SVGSMILElement::seekToIntervalCorrespondingToTime(double elapsed) {
  DCHECK(!m_isWaitingForFirstInterval);
  DCHECK(elapsed >= m_interval.begin);

  // Manually seek from interval to interval, just as if the animation would run
  // regulary.
  while (true) {
    // Figure out the next value in the begin time list after the current
    // interval begin.
    SMILTime nextBegin = findInstanceTime(Begin, m_interval.begin, false);

    // If the 'nextBegin' time is unresolved (eg. just one defined interval),
    // we're done seeking.
    if (nextBegin.isUnresolved())
      return;

    // If the 'nextBegin' time is larger than or equal to the current interval
    // end time, we're done seeking.  If the 'elapsed' time is smaller than the
    // next begin interval time, we're done seeking.
    if (nextBegin < m_interval.end && elapsed >= nextBegin) {
      // End current interval, and start a new interval from the 'nextBegin'
      // time.
      m_interval.end = nextBegin;
      if (!resolveNextInterval())
        break;
      continue;
    }

    // If the desired 'elapsed' time is past the current interval, advance to
    // the next.
    if (elapsed >= m_interval.end) {
      if (!resolveNextInterval())
        break;
      continue;
    }

    return;
  }
}

float SVGSMILElement::calculateAnimationPercentAndRepeat(
    double elapsed,
    unsigned& repeat) const {
  SMILTime simpleDuration = this->simpleDuration();
  repeat = 0;
  if (simpleDuration.isIndefinite()) {
    repeat = 0;
    return 0.f;
  }
  if (!simpleDuration) {
    repeat = 0;
    return 1.f;
  }
  DCHECK(m_interval.begin.isFinite());
  DCHECK(simpleDuration.isFinite());
  double activeTime = elapsed - m_interval.begin.value();
  SMILTime repeatingDuration = this->repeatingDuration();
  if (elapsed >= m_interval.end || activeTime > repeatingDuration) {
    repeat = static_cast<unsigned>(repeatingDuration.value() /
                                   simpleDuration.value());
    if (!fmod(repeatingDuration.value(), simpleDuration.value()))
      repeat--;

    double percent = (m_interval.end.value() - m_interval.begin.value()) /
                     simpleDuration.value();
    percent = percent - floor(percent);
    if (percent < std::numeric_limits<float>::epsilon() ||
        1 - percent < std::numeric_limits<float>::epsilon())
      return 1.0f;
    return clampTo<float>(percent);
  }
  repeat = static_cast<unsigned>(activeTime / simpleDuration.value());
  double simpleTime = fmod(activeTime, simpleDuration.value());
  return clampTo<float>(simpleTime / simpleDuration.value());
}

SMILTime SVGSMILElement::calculateNextProgressTime(double elapsed) const {
  if (m_activeState == Active) {
    // If duration is indefinite the value does not actually change over time.
    // Same is true for <set>.
    SMILTime simpleDuration = this->simpleDuration();
    if (simpleDuration.isIndefinite() || isSVGSetElement(*this)) {
      SMILTime repeatingDurationEnd = m_interval.begin + repeatingDuration();
      // We are supposed to do freeze semantics when repeating ends, even if the
      // element is still active.
      // Take care that we get a timer callback at that point.
      if (elapsed < repeatingDurationEnd &&
          repeatingDurationEnd < m_interval.end &&
          repeatingDurationEnd.isFinite())
        return repeatingDurationEnd;
      return m_interval.end;
    }
    return elapsed + 0.025;
  }
  return m_interval.begin >= elapsed ? m_interval.begin
                                     : SMILTime::unresolved();
}

SVGSMILElement::ActiveState SVGSMILElement::determineActiveState(
    SMILTime elapsed) const {
  if (elapsed >= m_interval.begin && elapsed < m_interval.end)
    return Active;

  return fill() == FillFreeze ? Frozen : Inactive;
}

bool SVGSMILElement::isContributing(double elapsed) const {
  // Animation does not contribute during the active time if it is past its
  // repeating duration and has fill=remove.
  return (m_activeState == Active &&
          (fill() == FillFreeze ||
           elapsed <= m_interval.begin + repeatingDuration())) ||
         m_activeState == Frozen;
}

bool SVGSMILElement::progress(double elapsed, bool seekToTime) {
  ASSERT(m_timeContainer);
  ASSERT(m_isWaitingForFirstInterval || m_interval.begin.isFinite());

  if (!m_syncBaseConditionsConnected)
    connectSyncBaseConditions();

  if (!m_interval.begin.isFinite()) {
    ASSERT(m_activeState == Inactive);
    m_nextProgressTime = SMILTime::unresolved();
    return false;
  }

  if (elapsed < m_interval.begin) {
    ASSERT(m_activeState != Active);
    m_nextProgressTime = m_interval.begin;
    // If the animation is frozen, it's still contributing.
    return m_activeState == Frozen;
  }

  m_previousIntervalBegin = m_interval.begin;

  if (m_isWaitingForFirstInterval) {
    m_isWaitingForFirstInterval = false;
    resolveFirstInterval();
  }

  // This call may obtain a new interval -- never call
  // calculateAnimationPercentAndRepeat() before!
  if (seekToTime) {
    seekToIntervalCorrespondingToTime(elapsed);
    if (elapsed < m_interval.begin) {
      // elapsed is not within an interval.
      m_nextProgressTime = m_interval.begin;
      return false;
    }
  }

  unsigned repeat = 0;
  float percent = calculateAnimationPercentAndRepeat(elapsed, repeat);
  RestartedInterval restartedInterval = maybeRestartInterval(elapsed);

  ActiveState oldActiveState = m_activeState;
  m_activeState = determineActiveState(elapsed);
  bool animationIsContributing = isContributing(elapsed);

  if (animationIsContributing) {
    if (oldActiveState == Inactive || restartedInterval == DidRestartInterval) {
      scheduleEvent(EventTypeNames::beginEvent);
      startedActiveInterval();
    }

    if (repeat && repeat != m_lastRepeat)
      scheduleRepeatEvents(repeat);

    m_lastPercent = percent;
    m_lastRepeat = repeat;
  }

  if ((oldActiveState == Active && m_activeState != Active) ||
      restartedInterval == DidRestartInterval) {
    scheduleEvent(EventTypeNames::endEvent);
    endedActiveInterval();
  }

  // Triggering all the pending events if the animation timeline is changed.
  if (seekToTime) {
    if (m_activeState == Inactive)
      scheduleEvent(EventTypeNames::beginEvent);

    if (repeat) {
      for (unsigned repeatEventCount = 1; repeatEventCount < repeat;
           repeatEventCount++)
        scheduleRepeatEvents(repeatEventCount);
      if (m_activeState == Inactive)
        scheduleRepeatEvents(repeat);
    }

    if (m_activeState == Inactive || m_activeState == Frozen)
      scheduleEvent(EventTypeNames::endEvent);
  }

  m_nextProgressTime = calculateNextProgressTime(elapsed);
  return animationIsContributing;
}

void SVGSMILElement::notifyDependentsIntervalChanged() {
  ASSERT(m_interval.begin.isFinite());
  // |loopBreaker| is used to avoid infinite recursions which may be caused by:
  // |notifyDependentsIntervalChanged| -> |createInstanceTimesFromSyncbase| ->
  // |add{Begin,End}Time| -> |{begin,end}TimeChanged| ->
  // |notifyDependentsIntervalChanged|
  //
  // As the set here records SVGSMILElements on the stack, it is acceptable to
  // use a HashSet of untraced heap references -- any conservative GC which
  // strikes before unwinding will find these elements on the stack.
  DEFINE_STATIC_LOCAL(HashSet<UntracedMember<SVGSMILElement>>, loopBreaker, ());
  if (!loopBreaker.insert(this).isNewEntry)
    return;

  for (SVGSMILElement* element : m_syncBaseDependents)
    element->createInstanceTimesFromSyncbase(*this);

  loopBreaker.erase(this);
}

void SVGSMILElement::createInstanceTimesFromSyncbase(SVGSMILElement& syncBase) {
  // FIXME: To be really correct, this should handle updating exising interval
  // by changing the associated times instead of creating new ones.
  for (Condition* condition : m_conditions) {
    if (condition->getType() == Condition::Syncbase &&
        condition->syncBaseEquals(syncBase)) {
      ASSERT(condition->name() == "begin" || condition->name() == "end");
      // No nested time containers in SVG, no need for crazy time space
      // conversions. Phew!
      SMILTime time = 0;
      if (condition->name() == "begin")
        time = syncBase.m_interval.begin + condition->offset();
      else
        time = syncBase.m_interval.end + condition->offset();
      if (!time.isFinite())
        continue;
      addInstanceTime(condition->getBeginOrEnd(), time);
    }
  }
}

void SVGSMILElement::addSyncBaseDependent(SVGSMILElement& animation) {
  m_syncBaseDependents.insert(&animation);
  if (m_interval.begin.isFinite())
    animation.createInstanceTimesFromSyncbase(*this);
}

void SVGSMILElement::removeSyncBaseDependent(SVGSMILElement& animation) {
  m_syncBaseDependents.erase(&animation);
}

void SVGSMILElement::beginByLinkActivation() {
  addInstanceTime(Begin, elapsed());
}

void SVGSMILElement::endedActiveInterval() {
  clearTimesWithDynamicOrigins(m_beginTimes);
  clearTimesWithDynamicOrigins(m_endTimes);
}

void SVGSMILElement::scheduleRepeatEvents(unsigned count) {
  m_repeatEventCountList.push_back(count);
  scheduleEvent(EventTypeNames::repeatEvent);
  scheduleEvent(AtomicString("repeatn"));
}

void SVGSMILElement::scheduleEvent(const AtomicString& eventType) {
  TaskRunnerHelper::get(TaskType::DOMManipulation, &document())
      ->postTask(BLINK_FROM_HERE,
                 WTF::bind(&SVGSMILElement::dispatchPendingEvent,
                           wrapPersistent(this), eventType));
}

void SVGSMILElement::dispatchPendingEvent(const AtomicString& eventType) {
  DCHECK(eventType == EventTypeNames::endEvent ||
         eventType == EventTypeNames::beginEvent ||
         eventType == EventTypeNames::repeatEvent || eventType == "repeatn");
  if (eventType == "repeatn") {
    unsigned repeatEventCount = m_repeatEventCountList.front();
    m_repeatEventCountList.remove(0);
    dispatchEvent(RepeatEvent::create(eventType, repeatEventCount));
  } else {
    dispatchEvent(Event::create(eventType));
  }
}

bool SVGSMILElement::hasValidTarget() {
  return targetElement() && targetElement()->inActiveDocument();
}

void SVGSMILElement::schedule() {
  DCHECK(!m_isScheduled);

  if (!m_timeContainer || !hasValidTarget())
    return;

  m_timeContainer->schedule(this, m_targetElement, m_attributeName);
  m_isScheduled = true;
}

void SVGSMILElement::unscheduleIfScheduled() {
  if (!m_isScheduled)
    return;

  DCHECK(m_timeContainer);
  DCHECK(m_targetElement);
  m_timeContainer->unschedule(this, m_targetElement, m_attributeName);
  m_isScheduled = false;
}

DEFINE_TRACE(SVGSMILElement) {
  visitor->trace(m_targetElement);
  visitor->trace(m_timeContainer);
  visitor->trace(m_conditions);
  visitor->trace(m_syncBaseDependents);
  SVGElement::trace(visitor);
  SVGTests::trace(visitor);
}

}  // namespace blink
