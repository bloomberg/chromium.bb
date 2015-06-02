Routing
=======

A composable suite of objects and elements to make routing with web components
a breeze.

* [Hash-](#MoreRouting.HashDriver) and [path-](#MoreRouting.PathDriver)based
  routing.
* [Named routes](#more-routing).
* [Nested (mounted) routes](#more-route--nesting).
* [Declarative route switching](#more-route-switch).
* [Polymer helpers](#polymer-helpers--filters) for easy integration into existing elements.

[Rob Dodson](https://github.com/robdodson) has whipped up a great video
that can get you started with more-routing:

<p align="center">
  <a href="https://www.youtube.com/watch?v=-67kb7poIT8">
    <img src="http://img.youtube.com/vi/-67kb7poIT8/0.jpg" alt="Moar routing with... more-routing">
  </a>
</p>

Or, TL;DR:

```html
<link rel="import" href="../more-routing/more-routing.html">

<more-routing-config driver="path"></more-routing-config>
<more-route name="user" path="/users/:userId">
  <more-route name="user-bio" path="/bio"></more-route>
</more-route>

<more-route-selector selectedParams="{{params}}">
  <core-pages>
    <section route="/">
      This is the index.
    </section>

    <section route="/about">
      It's a routing demo!
      <a href="{{ urlFor('user-bio', {userId: 1}) }}">Read about user 1</a>.
    </section>

    <section route="user">
      <header>Heyo user {{params.userId}}!</header>
      <template if="{{ route('user-bio').active }}">
        All the details about {{params.userId}} that you didn't want to know...
      </template>
    </section>
  </core-pages>
</more-route-selector>
```

And finally, check out [the demo](demo/) for a project that makes comprehensive
use of the various features in more-routing.


Element API
===========

<a name="more-routing-config"></a>
`<more-routing-config>`
----------------

_Defined in [`more-routing-config.html`](more-routing-config.html)._

The declarative interface for configuring [`MoreRouting`](#MoreRouting).
Currently, this lets you declare which [driver](#MoreRouting.Driver) you wish
to use (`hash` or `path`):

```html
<more-routing-config driver="hash"></more-routing-config>
```

You should place this as early in the load process for your app as you can. Any
routes defined prior to the driver being set will trigger an error.


<a name="more-route"></a>
`<more-route>`
--------------

_Defined in [`more-route.html`](more-route.html)._

Reference routes by path, and extract their params:

```html
<more-route path="/users/:userId" params="{{user}}"></more-route>
```

Declare a named route:

```html
<more-route path="/users/:userId" name="user"></more-route>
```

Reference a named route:

```html
<more-route name="user" params="{{user}}"></more-route>
```


<a name="more-route--nesting"></a>
### Route Nesting

Routes can also be nested:

```html
<more-route path="/users/:userId" name="user">
  <more-route path="/bio" name="user-bio"></more-route>
</more-route>
```

In this example, the route named `user-bio` will match `/users/:userId/bio`.

Finally, `<more-route>` elements can declare a routing context for the element
that contains them by setting the `context` attribute. See the
[routed elements](#more-route-selector--routed-elements) section for more info.


<a name="more-route-selector"></a>
`<more-route-selector>`
-----------------------

_Defined in [`more-route-selector.html`](more-route-selector.html)._

Manages a [`<core-selector>`](https://www.polymer-project.org/docs/elements/core-elements.html#core-selector)
(or anything that extends it/looks like one), where each item in the selector
have an associated route. The most specific route that is active will be
selected.

```html
<more-route-selector>
  <core-pages>
    <section route="/">The index!</section>
    <section route="user">A user (named route)</section>
    <section route="/about">Another route</section>
  </core-pages>
</more-route-selector>
```

By default, `more-route-selector` will look for the `route` attribute on any
children of the `core-selector` (change this via `routeAttribute`).

It exposes information about the selected route via a few properties:

`selectedParams`: The params of the selected route.

`selectedRoute`: The [`MoreRouting.Route`](#MoreRouting.Route) representing the
selected route.

`selectedPath`: The path expression of the selected route.

`selectedIndex`: The index of the selected route (relative to `routes`).


<a name="more-route-selector--routed-elements"></a>
### Routed Elements

Elements can declare a route to be associated with, which allows
`<more-route-selector>` to be smart and use that as the route it checks against
for your element. For example:

```html
<polymer-element name="routed-element">
  <template>
    <more-route path="/my/route" context></more-route>
    I'm a routed element!
  </template>
</polymer-element>
```

```html
<more-route-selector>
  <core-pages>
    <section route="/">The index!</section>
    <routed-element></routed-element>
  </core-pages>
</more-route-selector>
```

In this example, The `<more-route-selector>` will choose `<routed-element>`
whenever the path begins with `/my/route`. Keep it DRY!


<a name="more-route-selector--nesting-contexts"></a>
### Nesting Contexts

Similar to [`more-route`'s nesting behavior](#more-route--nesting), any items in
the core-selector also behave as nesting contexts. Any route declared within a
routing context is effectively _mounted_ on the context route.

Taking the example element, `<routed-element>` above; if we were to add the
following to its template:

```html
<more-route path="/:tab" params="{{params}}"></more-route>
```

That route's full path would be `/my/route/:tab`, because `/my/route` is the
context in which it is nested. This allows you to create custom elements that
make use of routes, _while not requiring knowledge of the app's route
hierarchy_. Very handy for composable components!

**Note:** All items in a `<more-route-selector>` are treated as routing
contexts!


<a name="polymer-helpers"></a>
Polymer Helpers
---------------

<a name="polymer-helpers--filters"></a>
### Filters

_Defined in [`polymer-expressions.html`](polymer-expressions.html)._

Several filters (functions) are exposed to templates for your convenience:

#### `route`

You can fetch a `MoreRouting.Route` object via the `route` filter. Handy for
reading params, etc on the fly.

```html
<x-user model="{{ route('user').params }}"></x-user>
```

**Note:** The `route()` helper is unfortunately _not_ aware of the current
routing context. Consider using only named routes to avoid confusion!

#### `urlFor`

Generates a URL for the specified route and params:

```html
<a href="{{ urlFor('user', {userId: 1}) }}">User 1</a>
```


JavaScript API
==============

<a name="MoreRouting"></a>
`MoreRouting`
-------------

_Defined in [`routing.html`](routing.html)._

The main entry point into `more-routing`, exposed as a global JavaScript
namespace of `MoreRouting`. For the most part, all elements and helpers are
built on top of it.

`MoreRouting` manages the current [driver](#MoreRouting.Driver), and maintains
an identity map of all routes.


<a name="MoreRouting.driver"></a>
### `MoreRouting.driver`

Before you can make use of navigation and URL parsing, a driver must be
registered. Simply assign an instance of `MoreRouting.Driver` to this property.

This is exposed as a declarative element via
[`<more-routing-config driver="...">`](#more-routing).


<a name="MoreRouting.getRoute"></a>
### `MoreRouting.getRoute`

Returns a [`MoreRouting.Route`](#MoreRouting.Route), by path...

    MoreRouting.getRoute('/users/:userId')

...or by name:

    MoreRouting.getRoute('user')

Because routes are identity mapped, `getRoute` guarantees that it will return
the same `Route` object for the same path.


<a name="MoreRouting.Route"></a>
`MoreRouting.Route`
-------------------

_Defined in [`route.html`](route.html)._

The embodiment for an individual route. It has various handy properties.
Highlights:

``active``: Whether the route is active (the current URL matches).

``params``: A map of param keys to their values (matching the `:named` tokens)
within the path.

``path``: The path expression that this route matches.

### Paths

Path expressions begin with a `/` (to disambiguate them from route names), and
are a series of tokens separated by `/`.

Tokens can be of the form `:named`, where they named a parameter. Or they can
be regular strings, at which point they are static path parts.

Should be familiar from many other routing systems.


<a href="MoreRouting.Driver"></a>
`MoreRouting.Driver`
--------------------

_Defined in [`driver.html`](driver.html)._

Drivers manage how the URL is read, and how to navigate to URLs. There are two
built in drivers:


<a href="MoreRouting.HashDriver"></a>
### `MoreRouting.HashDriver`

_Defined in [`hash-driver.html`](hash-driver.html)._

Provides hash-based routing, generating URLs like `#!/users/1/bio`. It has a
configurable prefix (after the `#`). By default, it uses a prefix of `!/`
(to fit the [AJAX-crawling spec](https://developers.google.com/webmasters/ajax-crawling/docs/specification)).


<a href="MoreRouting.PathDriver"></a>
### `MoreRouting.PathDriver`

_Defined in [`path-driver.html`](path-driver.html)._

Provides true path-based routing (via `pushState`), generating URLs like
`/users/1/bio`. If your web app is mounted on a path other than `/`, you can
specify that mount point via the `prefix`.
