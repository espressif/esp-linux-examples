System Dashboard Tests
======================

The test suite includes host-side parser unit tests (``test_parsers.c``) and an
HTTP integration test script (``test_http.py``). From
``examples/network/system-dashboard``, run both on the development host with
AddressSanitizer and UndefinedBehaviorSanitizer enabled:

.. code-block:: bash

   make CC=gcc SANITIZE=1 test

``make test`` runs:

1. ``build/test-parsers tests/fixtures`` under ``ASAN_OPTIONS=detect_leaks=1``,
   verifying ``/proc``, ``/sys``, standard OS metadata and release-file
   fallbacks, malformed metric rejection, and ``/proc/net/route``
   default-route parsing.
2. ``tests/test_http.py``, which starts ``build/esp-system-dashboard`` on an
   ephemeral localhost port and verifies ``/healthz``, ``/``,
   ``/api/v1/status``, ``HEAD`` handling, ``405`` and ``404`` responses, path
   traversal rejection, 20 concurrent requests across 4 worker threads, and
   clean ``SIGTERM`` shutdown.
