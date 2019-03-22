# Overview
This component deals with multiple tabs or pages navigation that starts with an
explicit user action, and has similar context. The component contains tools to
process these navigations and present them in a way that provides users a
smoother UI flow within their tabs.

# Understanding Terms
This component introduces several terms, include:
* Pageload: A load of a page by a given user at a moment in time, and they are
derived from the user's Chrome History data.
* User Journey: A set of connected pageloads that are determined based on an
explicit user action, such as omnibox search, and the semantic relationship
with others.

# Provisions
This component provides a `JourneyInfoFetcher`, an authenticated fetcher that
fetches a list of pageload information in a json format from the user journey
endpoint server. This fetcher can be used by any client that needs access to 
these information from the server.
