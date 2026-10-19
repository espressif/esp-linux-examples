System Dashboard
================

``esp-system-dashboard`` is a minimal HTTP system monitor and versioned
read-only JSON telemetry service for ESP SoCs running Linux. The daemon is
written in C with ``libmicrohttpd`` and serves a single-page HTML dashboard
whose vanilla JavaScript polls ``/api/v1/status`` every two seconds. No Python,
Node.js, JavaScript framework, or target internet access is required at
runtime.

The service is a standalone userspace application that reads standard kernel
interfaces under ``/proc`` and ``/sys`` and requires no additional kernel
configuration beyond a stock networking-enabled image.

Prerequisites
-------------

* An ESP SoC development board running Linux with an active network interface
* A RISC-V Linux cross-compiler and sysroot matching the target C library
  (``musl`` or ``glibc``)
* ``libmicrohttpd >= 1.0.2`` and ``pkg-config`` available in the target
  sysroot (build time) and ``libmicrohttpd.so.12`` installed on the target root
  filesystem (runtime)

Example Folder Contents
-----------------------

.. code-block:: text

   ├── Makefile                Build, install, and test rules
   ├── README.rst              This file
   ├── src
   │   ├── main.c              CLI argument parsing and signal handling
   │   ├── http_server.c       libmicrohttpd daemon and JSON serialization
   │   ├── http_server.h
   │   ├── system_info.c       OS identity, board model, kernel, and rootfs mount
   │   ├── system_info.h
   │   ├── system_metrics.c    CPU jiffies, memory, uptime, and load average
   │   ├── system_metrics.h
   │   ├── network_info.c      Network interfaces, addresses, counters, and route
   │   └── network_info.h
   ├── tests
   │   ├── README.rst          Test suite documentation
   │   ├── fixtures            Synthetic /proc, /sys, and /etc trees for tests
   │   ├── test_http.py        HTTP integration test suite
   │   └── test_parsers.c      Parser unit test suite
   └── www
       └── index.html          Static browser dashboard (HTML, CSS, vanilla JS)

Build
-----

From the repository root, select the example and cross-compile against a target
sysroot that provides ``libmicrohttpd`` (for example, using the prebuilt RV64
GNU toolchain ``riscv64-esp-linux-gnu-`` or ``musl`` multilib toolchain
``riscv64-esp-linux-musl-``):

.. code-block:: bash

   cd examples/network/system-dashboard
   make CROSS_COMPILE=<toolchain-prefix> \
       PKG_CONFIG_SYSROOT_DIR=/path/to/sysroot \
       PKG_CONFIG_LIBDIR=/path/to/sysroot/usr/lib/pkgconfig

The compiled executable is written to ``build/esp-system-dashboard``. Run
``make clean`` before rebuilding with a different toolchain or sysroot.

The Makefile checks for ``libmicrohttpd >= 1.0.2`` through ``pkg-config``.
It accepts ``CC``, ``CPPFLAGS``, ``CFLAGS``, ``LDFLAGS``, and ``PKG_CONFIG``
from the environment or the ``make`` command line. When ``CC`` is not set by
the caller, it falls back to ``$(CROSS_COMPILE)gcc`` (default
``CROSS_COMPILE ?= riscv64-esp-linux-gnu-``).

Deploy
------

Staging for Image or Package Creation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To stage the executable and static web assets into a root filesystem or
packaging directory, use the ``install`` target with ``DESTDIR``. The Makefile
honours ``prefix`` (default ``/usr``), ``bindir`` (default ``$(prefix)/bin``),
``datadir`` (default ``$(prefix)/share``), and ``dashboard_datadir`` (default
``$(datadir)/esp-system-dashboard``), and compiles ``DASHBOARD_WEB_ROOT`` set
to ``$(dashboard_datadir)``:

.. code-block:: bash

   make DESTDIR=/path/to/rootfs install

This installs:

* ``$(DESTDIR)/usr/bin/esp-system-dashboard``
* ``$(DESTDIR)/usr/share/esp-system-dashboard/index.html``

Copying to a Running Target
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copying directly with ``scp`` requires SSH access on the target,
``libmicrohttpd.so.12`` built for the target's architecture and C library ABI
installed in ``/usr/lib``, and a writable target filesystem. An NFS-mounted
root filesystem is the standard setup during development; the default ``cramfs``
root filesystem is read-only and cannot accept these copies.

.. code-block:: bash

   scp build/esp-system-dashboard root@<target-ip>:/usr/bin/esp-system-dashboard
   ssh root@<target-ip> mkdir -p /usr/share/esp-system-dashboard
   scp www/index.html root@<target-ip>:/usr/share/esp-system-dashboard/index.html

Run
---

Start the daemon on the target:

.. code-block:: console

   # esp-system-dashboard
   ESP system dashboard listening on http://0.0.0.0:8080/

The executable accepts the following command-line options:

* ``--bind ADDRESS``: IPv4 address to listen on (default: ``0.0.0.0``)
* ``--port PORT``: TCP port to listen on (default: ``8080``)
* ``--web-root PATH``: Directory containing ``index.html`` (default:
  ``/usr/share/esp-system-dashboard``)
* ``--help``: Print usage information and exit

Open ``http://<target-ip>:8080/`` in a browser on the same network. To stop the
daemon cleanly, send ``SIGINT`` (``Ctrl+C``) or ``SIGTERM`` (``kill -TERM
<pid>``); the process waits on ``sigwait()``, stops the ``libmicrohttpd``
daemon, and frees buffered resources before exiting with status ``0``.

.. note::

   ``esp-system-dashboard`` serves plaintext, unauthenticated HTTP and is
   intended for use on a trusted development network.

HTTP Endpoints and API Schema
-----------------------------

The server accepts ``GET`` and ``HEAD`` requests. Other HTTP methods return
``405 Method Not Allowed`` with ``Allow: GET, HEAD``. Unknown paths return
``404 Not Found`` with a JSON error body.

* ``GET /`` and ``HEAD /``: Returns ``200 OK`` with ``index.html``
  (``Content-Type: text/html; charset=utf-8``).
* ``GET /healthz`` and ``HEAD /healthz``: Readiness probe that returns
  ``204 No Content`` with ``Cache-Control: no-store``.
* ``GET /api/v1/status`` and ``HEAD /api/v1/status``: Returns ``200 OK`` with a
  JSON snapshot (``Content-Type: application/json``, ``Cache-Control:
  no-store``), or ``503 Service Unavailable`` if ``/proc`` or ``/sys`` metrics
  cannot be collected.

Example ``/api/v1/status`` response
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: json

   {
     "schema_version": "1.0",
     "system": {
       "hostname": "esp-linux",
       "model": "Espressif Systems Board",
       "kernel_release": "6.18.0",
       "architecture": "riscv64",
       "init": "init",
       "root_filesystem": {
         "source": "/dev/mmcblk0p2",
         "type": "ext4"
       },
       "os": {
         "pretty_name": "Example Linux 1.0",
         "id": "example",
         "id_like": "",
         "version_id": "1.0",
         "family": "Linux"
       }
     },
     "cpu": {
       "count": 1,
       "cumulative_jiffies": {
         "user": 412,
         "nice": 0,
         "system": 985,
         "idle": 48210,
         "iowait": 14,
         "irq": 0,
         "softirq": 31,
         "steal": 0
       }
     },
     "runtime": {
       "uptime_seconds": 496.52,
       "load_average": {
         "one_minute": 0.02,
         "five_minutes": 0.04,
         "fifteen_minutes": 0.01
       }
     },
     "memory": {
       "total_bytes": 15515648,
       "available_bytes": 11268096
     },
     "network": {
       "default_route": {
         "interface": "eth0",
         "gateway": "192.168.7.1"
       },
       "interfaces": [
         {
           "name": "eth0",
           "operational_state": "up",
           "carrier": true,
           "mtu_bytes": 1500,
           "mac_address": "02:00:00:00:00:01",
           "addresses": [
             "192.168.7.2"
           ],
           "rx_bytes": 184520,
           "tx_bytes": 96312,
           "rx_errors": 0,
           "tx_errors": 0,
           "rx_dropped": 0,
           "tx_dropped": 0
         }
       ]
     }
   }

Schema fields and kernel sources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``schema_version``: Format version string (``"1.0"``).
* ``system``:

  * ``hostname``: System hostname from ``gethostname()``.
  * ``model``: Board model string from ``/sys/firmware/devicetree/base/model``
    (or ``"Unknown device"`` if absent).
  * ``kernel_release`` and ``architecture``: Kernel version and machine
    architecture from ``uname()``.
  * ``init``: Process 1 command name from ``/proc/1/comm``.
  * ``root_filesystem``: Mount ``source`` and filesystem ``type`` for ``/``
    from ``/proc/mounts``.
  * ``os``: Operating system ``pretty_name``, ``id``, ``id_like``,
    ``version_id``, and ``family``. The collector reads ``/etc/os-release``
    first, then ``/usr/lib/os-release``, followed by distribution-specific
    release files under ``/etc`` when ``os-release`` is absent. The final
    display-name fallback is the first line of ``/etc/issue``, or
    ``"Unknown Linux"`` if no release file exists. ``family`` classifies the
    detected distribution family (default ``"Linux"``).

* ``cpu``: Logical processor ``count`` (number of ``cpuN`` lines) and
  aggregate ``cumulative_jiffies`` (``user``, ``nice``, ``system``, ``idle``,
  ``iowait``, ``irq``, ``softirq``, ``steal``) from ``/proc/stat``.
* ``runtime``: ``uptime_seconds`` from ``/proc/uptime`` and 1-, 5-, and
  15-minute ``load_average`` values from ``/proc/loadavg``.
* ``memory``: ``total_bytes`` (``MemTotal``) and ``available_bytes``
  (``MemAvailable``, or the sum of ``MemFree``, ``Buffers``, and ``Cached``
  when ``MemAvailable`` is absent) from ``/proc/meminfo``.
* ``network``:

  * ``default_route``: Outbound ``interface`` and optional IPv4 ``gateway``
    parsed from the active default destination entry (``00000000``) in
    ``/proc/net/route``.
  * ``interfaces``: Every non-loopback interface discovered from
    ``/sys/class/net`` and ``getifaddrs()``. Each entry reports ``name``,
    ``operational_state`` (``operstate``), ``carrier`` (``true``, ``false``, or
    ``null`` when unreadable), ``mtu_bytes`` (``mtu``), ``mac_address``
    (``address``), assigned IPv4 and IPv6 ``addresses``, and cumulative packet
    counters under ``/sys/class/net/<iface>/statistics/`` (``rx_bytes``,
    ``tx_bytes``, ``rx_errors``, ``tx_errors``, ``rx_dropped``,
    ``tx_dropped``).

The C server does not retain previous samples in memory. Each request returns
the current cumulative counters, and the browser computes CPU utilization
percentages and per-interface RX/TX byte rates between consecutive snapshots.
The browser uses target uptime as the interval and waits for at least 250 ms of
target time before calculating these values.

Reference Footprint
-------------------

The baseline figures below were measured on an ESP32-S31 target (``riscv32``,
``musl``) running Linux ``6.18.0`` with an active Ethernet interface:

* Stripped ``/usr/bin/esp-system-dashboard`` executable: ``21,696`` bytes
* Stripped ``/usr/lib/libmicrohttpd.so.12.62.8`` shared library: ``132,304``
  bytes
* Static ``/usr/share/esp-system-dashboard/index.html`` asset: ``9,935`` bytes
* Live process memory (from ``/proc/<pid>/status``): ``VmRSS`` measured ``772
  KiB`` before load and ``776 KiB`` after 100 ``/api/v1/status`` requests at
  concurrency 4; ``VmSize`` ranged from ``1,144 KiB`` to ``1,148 KiB``
* Process descriptors: ``2`` threads and ``5`` open file descriptors before and
  after the 100-request load test

Exact binary sizes and runtime memory usage depend on the C library, toolchain
version, compiler flags, and ``libmicrohttpd`` build configuration.

Troubleshooting
---------------

Userspace Application
~~~~~~~~~~~~~~~~~~~~~

* **Verify readiness and JSON output from the command line:** Run ``curl -i
  http://<target-ip>:8080/healthz`` (expect ``HTTP/1.1 204 No Content``) and
  ``curl -s http://<target-ip>:8080/api/v1/status``.
* **Server exits immediately with "Failed to start server":** Check whether
  another process is already bound to port ``8080`` or whether
  ``/usr/share/esp-system-dashboard/index.html`` is missing. Pass ``--port`` to
  select a different port or ``--web-root`` when running from a non-default
  directory, and check ``ps`` on the target to inspect running instances.

Shared Libraries
~~~~~~~~~~~~~~~~

* **Dynamic linker reports missing ``libmicrohttpd.so.12``:** Run ``readelf -d
  /usr/bin/esp-system-dashboard | grep NEEDED`` (or ``ldd`` when available) and
  verify that ``/usr/lib/libmicrohttpd.so.12`` is installed on the target root
  filesystem with matching architecture and C library ABI.

Kernel and Network Interfaces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Host browser cannot reach the target:** Check interface state and routing
  on the target with ``ip addr`` and ``ip route``, and confirm link state in
  ``/sys/class/net/<iface>/operstate`` and ``/sys/class/net/<iface>/carrier``.
* **Unexpected values in ``/api/v1/status``:** Inspect the underlying kernel
  files directly on the target: ``/proc/stat``, ``/proc/meminfo``,
  ``/proc/uptime``, ``/proc/loadavg``, ``/proc/mounts``, ``/proc/net/route``,
  and ``/sys/class/net/<iface>/statistics/``.
