Output
======

With at least a :type:`weston_head` attached, you can construct a
:type:`weston_output` object which can be used by the compositor, by enabling
the output using :func:`weston_output_enable`. The output **must not** be
already enabled.

The reverse operation, :func:`weston_output_disable`, should be used when there's
a need to reconfigure the output or it will be destroyed.

Output API
----------

.. doxygengroup:: output
   :content-only:
