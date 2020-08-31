Head
====

A head usually refers to a monitor, but it can also refer to an output window
in case of a nested compositor. A :type:`weston_output` is responsible for
driving a :type:`weston_head`. :type:`weston_head` should be initialized using
:func:`weston_head_init`, and shall be released using
:func:`weston_head_release`.

.. note::

   :func:`weston_head_init` and :func:`weston_head_release` belong to the
   private/internal backend API and should be moved accordingly once that
   section has been created.

A :type:`weston_head` must be attached/detached from a :type:`weston_output`.
To that purpose you can use :func:`weston_output_attach_head`, respectively
:func:`weston_head_detach`.

Head API
--------

.. doxygengroup:: head
   :content-only:
