Instructions for Maintainers
============================

Release
-------

To release a build:

1. Push all changes, verify CI passes (see link in README.rst).
2. Increment __version__ in __init__.py
3. ``git tag -a uri-template-py-[version]``
4. ``git push --tags origin master``
5. ``python setup.py sdist upload``
6. Brew coffee or tea.
