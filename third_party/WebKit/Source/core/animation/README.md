# `Source/core/animation`

This directory contains the main thread animation engine. This implements the
Web Animations timing model that drives CSS Animations, Transitions and exposes
the Web Animations API (`element.animate()`) to Javascript.

## Contacts

As of 2018 Blink animations is maintained by the
[cc/ team](https://cs.chromium.org/chromium/src/cc/OWNERS).

## Specifications Implemented

*   [CSS Animations Level 1](https://www.w3.org/TR/css-animations-1/)
*   [CSS Transitions](https://www.w3.org/TR/css-transitions-1/)
*   [Web Animations](https://www.w3.org/TR/web-animations-1/)
*   [CSS Properties and Values API Level 1 - Animation Behavior of Custom Properties](https://www.w3.org/TR/css-properties-values-api-1/#animation-behavior-of-custom-properties)
*   Individual CSS property animation behaviour [e.g. transform interolation](https://www.w3.org/TR/css-transforms-1/#interpolation-of-transforms).

## Integration with Chromium

The Blink animation engine interacts with Blink/Chrome in the following ways:

*   ### [Blink's Style engine](../css)

    The most user visible functionality of the animation engine is animating
    CSS values. This means animations have a place in the [CSS cascade][] and
    influence the [ComputedStyle][]s returned by [styleForElement()][].

    The implementation for this lives under [ApplyAnimatedStandardProperties()][]
    for standard properties and [ApplyAnimatedCustomProperties()][] for custom
    properties. Custom properties have more complex application logic due to
    dynamic dependencies introduced by [variable references].

    Animations can be controlled by CSS via the [`animation`](https://www.w3.org/TR/css-animations-1/#animation)
    and [`transition`](https://www.w3.org/TR/css-transitions-1/#transition-shorthand-property) properties.
    In code this happens when [styleForElement()][] uses [CalculateAnimationUpdate()][]
    and [CalculateTransitionUpdate()][] to build a [set of mutations][] to make
    to the animation engine which gets [applied later][].

[CSS cascade]: https://www.w3.org/TR/css-cascade-3/#cascade-origin
[ComputedStyle]: https://cs.chromium.org/search/?q=class:blink::ComputedStyle$
[styleForElement()]: https://cs.chromium.org/search/?q=function:StyleResolver::styleForElement
[ApplyAnimatedStandardProperties()]: https://cs.chromium.org/?type=cs&q=function:StyleResolver::ApplyAnimatedStandardProperties
[ApplyAnimatedCustomProperties()]: https://cs.chromium.org/?type=cs&q=function:ApplyAnimatedCustomProperties
[variable references]: https://www.w3.org/TR/css-variables-1/#using-variables
[CalculateAnimationUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateAnimationUpdate
[CalculateTransitionUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::CalculateTransitionUpdate
[MaybeApplyPendingUpdate()]: https://cs.chromium.org/search/?q=function:CSSAnimations::MaybeApplyPendingUpdate
[set of mutations]: https://cs.chromium.org/search/?q=class:CSSAnimationUpdate
[applied later]: https://cs.chromium.org/search/?q=function:Element::StyleForLayoutObject+MaybeApplyPendingUpdate

*   ### [Chromium's Compositor](../../../../../cc/README.md)

    Chromium's compositor has a separate, more lightweight [animation
    engine](../../../../../cc/animation/README.md) that runs separate to the
    main thread. Blink's animation engine delegates animations to the compositor
    where possible for better performance and power utilisation.

    #### Compositable animations

    A subset of style properties (currently transform, opacity, filter, and
    backdrop-filter) can be mutated on the compositor thread. Animations that
    mutates only these properties are a candidate for being accelerated and run
    on compositor thread which ensures they are isolated from Blink's main
    thread work.

    Whether or not an animation can be accelerated is determined by
    [CheckCanStartAnimationOnCompositor()][] which looks at several aspects
    such as the composite mode, other animations affecting same property, and
    whether the target element can be promoted and mutated in compositor.  
    Reasons for not compositing animations are captured in [FailureCodes][].

    #### Lifetime of a compositor animation

    Animations that can be accelerated get added to the [PendingAnimations][]
    list. The pending list is updates as part of document lifecycle and ensures
    each pending animation gets a corresponding [cc::AnimationPlayer][]
    representing the animation on the compositor. The player is initialized with
    appropriate timing values and corresponding effects.

    Note that changing that animation playback rate, start time, or effect,
    simply adds the animation back on to the pending list and causes the
    compositor animation to be cancelled and a new one to be started. See
    [Animation::PreCommit()][] for more details.

    An accelerated animation is still running on main thread ensuring that its
    effective output is reflected in the Element style. So while the compositor
    animation updates the visuals the main thread animation updates the computed
    style. There is a special case logic to ensure updates from such accelerated
    animations do not cause spurious commits from main to compositor (See
    [CompositedLayerMapping::UpdateGraphicsLayerGeometry()][])

    A compositor animation provide updates on its playback state changes (e.g.,
    on start, finish, abort) to its blink counterpart via
    [CompositorAnimationDelegate][] interface. Blink animation uses the start
    event callback to obtain an accurate start time for the animation which is
    important to ensure its output accurately reflects the compositor animation
    output.

[CheckCanStartAnimationOnCompositor()]: https://cs.chromium.org/search/?q=file:Animation.h+function:CheckCanStartAnimationOnCompositor
[FailureCodes]: https://cs.chromium.org/search/?q=return%5Cs%2B(CompositorAnimations::)?FailureCode
[cc::AnimationPlayer]: https://cs.chromium.org/search/?q=file:src/cc/animation/animation_player.h+class:AnimationPlayer
[PendingAnimations]: https://cs.chromium.org/search/?q=file:PendingAnimations.h+class:PendingAnimations
[Animation::PreCommit()]: https://cs.chromium.org/search/?q=file:Animation.h+function:PreCommit
[CompositorAnimationDelegate]: https://cs.chromium.org/search/?q=file:CompositorAnimationDelegate.h
[CompositedLayerMapping::UpdateGraphicsLayerGeometry()]: https://cs.chromium.org/search/?q=file:CompositedLayerMapping.h+function:UpdateGraphicsLayerGeometry

*   ### Javascript

    [EffectInput](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/animation/EffectInput.cpp)
    contains the helper functions that are used to
    [process a keyframe argument](https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument)
    which can take an argument of either object or array form.

    [PlayStateUpdateScope](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/animation/Animation.h?l=323):
    whenever there is a mutation to the animation engine from JS level, this
    gets created and the destructor has the logic that handles everything. It
    keeps the old and new state of the animation, checks the difference and
    mutate the properties of the animation, at the end it calls
    SetOutdatedAnimation() to inform the animation timeline that the time state
    of this animation is dirtied.

    There are a couple of other integration points that are less critical to everyday browsing:

*   ### DevTools

    The animations timeline uses [InspectorAnimationAgent][] to track all active
    animations. This class has interfaces for pausing, adjusting
    DocumentTimeline playback rate, and seeking animations.

    InspectorAnimationAgent clones the inspected animation in order to avoid
    firing animation events, and suppresses the effects of the original
    animation. From this point on, modifications can be made to the cloned
    animation without having any effect on the underlying animation or its
    listeners.

[InspectorAnimationAgent]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/inspector/InspectorAnimationAgent.h

*   ### SVG

    The `element.animate()` API supports targeting SVG attributes in its
    keyframes. This is an experimental implementation guarded by the
    WebAnimationsSVG flag and not exposed on the web.

    This feature should provide a high fidelity alternative to our SMIL
    implementation.

## Architecture

### Animation Timing Model

The animation engine is built around the
[timing model](https://www.w3.org/TR/web-animations-1/#timing-model) described
in the Web Animations spec.

This describes a hierarchy of entities:
*   DocumentTimeline: Represents the wall clock time
    *   Animation: Represents an individual animation and when it started
        playing.
        *   AnimationEffect: Represents the effect an animation has during the
            animation e.g. updating an elements color property.

Time trickles down from the DocumentTimeline and is transformed at each stage to
produce some progress fraction that can be used to apply the effects of the
animations.

For example:

*   DocumentTimeline notifies that the time is 20 seconds.
    *   Animation was started at 18 seconds and computes that it has a current
        time of 2 seconds.
        *   AnimationEffect has a duration of 8 seconds and computes that it has
            a progress of 25%.

            The effect is animating an element8s transform property from `none`
            to `rotate(360deg)` so it computes the current effect to be
            `rotate(90deg)`.

### Life cycle of an animation

1.  An Animation is created via CSS<sup>1</sup> or `element.animate()`.
2.  At the start of the next frame the Animation and its AnimationEffect are
    updated with the currentTime of the DocumentTimeline.
3.  The AnimationEffect gets sampled with its computed localTime, pushes a
    SampledEffect into its target element s EffectStack and marks the elements
    style as dirty to ensure it gets updated later in the document lifecycle.
4.  During the next style resolve on the target element all the SampledEffects
    in its EffectStack are incorporated into building the element's
    ComputedStyle.

One key takeaway here is to note that timing updates are done in a separate
phase to effect application. Effect application must occur during style
resolution which is a highly complex process with a well defined place in the
document lifecycle. Updates to animation timing will request style updates
rather than invoke them directly.

<sup>1</sup> CSS animations and transitions are actually created/destroyed
during style resolve (step 4). There is special logic for forcing these
animations to have their timing updated and their effects included in
the same style resolve. An unfortunate side effect of this is that style
resolution can cause style to get dirtied, this is currently a
[code health bug](http://crbug.com/492887).

### KeyframeEffects

TODO: Describe how KeyframeEffects represent an animation's keyframes.

### Keyframe Interpolations

TODO: Describe how interpolation works and where to look for further details.

#### InterpolationTypes

TODO: Describe various interpolation types and where they are defined.

#### Applying a stack of Interpolations

TODO: Describe how interpolations interact with the effect stack.

## Testing pointers

Test new animation features using end to end web-platform-tests to ensure
cross-browser interoperability. Use unit testing when access to chrome internals
is required. Test chrome specific features such as compositing of animation
using LayoutTests or unit tests.

### End to end testing

Features in the Web Animations spec are tested in
[web-animations][web-animations-tests]. [Writing web platform tests][] has
pointers for how to get started. If Chrome does not correctly implement the
spec, add a corresponding -expected.txt file with your test listing the expected
failure in Chrome.

[Layout tests](../../../../../docs/testing/writing_layout_tests.md) are located
in [third_party/WebKit/LayoutTests][]. These should be written when needing end
to end testing but either when testing chrome specific features (i.e.
non-standardized) such as compositing or when the test requires access to chrome
internal features not easily tested by web-platform-tests.

[web-animations-tests]: https://cs.chromium.org/chromium/src/third_party/WebKit/LayoutTests/external/wpt/web-animations/
[Writing web platform tests]: ../../../../../docs/testing/web_platform_tests.md#Writing-tests
[third_party/WebKit/LayoutTests]: https://cs.chromium.org/chromium/src/third_party/WebKit/LayoutTests/animations/

### Unit testing

Unit testing of animations can range from [extending Test][] when you will
manually construct an instance of your object to [extending RenderingTest][]
where you can load HTML, [enable compositing][] if necessary, and run assertions
about the state.

[extending Test]: https://cs.chromium.org/search/?q=public%5C+::testing::Test+file:core%5C/Animation&sq=package:chromium&type=cs
[extending RenderingTest]: https://cs.chromium.org/search/?q=public%5C+RenderingTest+file:core%5C/animation&type=cs
[enable compositing]: https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/animation/CompositorAnimationsTest.cpp?type=cs&sq=package:chromium&q=file:core%5C/animation%5C/.*Test%5C.cpp+EnableCompositing

## Ongoing work

### Properties And Values API

TODO: Summarize properties and values API.

### Web Animations API

TODO: Summarize Web Animations API.

### [Animation Worklet](../../modules/animationworklet/README.md)

AnimationWorklet is a new primitive for creating high performance procedural
animations on the web. It is being incubated as part of the
[CSS Houdini task force](https://github.com/w3c/css-houdini-drafts/wiki), and if
successful will be transferred to that task force for full standardization.

A [WorkletAnimation][] behaves and exposes the same animation interface as other
web animation but it allows the animation itself to be highly customized in
Javascript by providing an `animate` callback. These animations run inside an
isolated worklet global scope.

[WorkletAnimation]: https://cs.chromium.org/search/?q=file:animationworklet/WorkletAnimation.h+class:WorkletAnimation
