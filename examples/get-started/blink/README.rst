GPIO Blink
==========

``blink`` is a minimal Linux userspace application that toggles a GPIO line
with ``libgpiod``. It can drive a 3.3 V-compatible LED module or a discrete LED
through a current-limiting resistor.

Prerequisites
-------------

* An ESP SoC development board capable of booting Linux
* A free GPIO exposed by the board
* A 3.3 V-compatible LED module, or an LED and a current-limiting resistor
* A RISC-V Linux cross-compiler compatible with the target root filesystem
* ``libgpiod`` headers, library, and pkg-config metadata for the target

Hardware Connection
-------------------

Select an exposed GPIO that is not reserved by the board or used by another
driver. For a three-pin, 3.3 V-compatible LED module, connect:

.. code-block:: text

   ESP board               LED module
   ---------               ----------
   3.3 V    -------------- VCC
   GND      -------------- GND
   free GPIO ------------- signal

For a discrete LED, connect the GPIO through a current-limiting resistor to the
LED anode, and connect the LED cathode to GND. Check the board schematic before
selecting a GPIO. A GPIO line offset is relative to its ``gpiochip`` and is not
necessarily the same as a board header pin number.

Example Folder Contents
-----------------------

The example contains one C source file under ``src/`` and uses a Makefile to
build the ``blink`` executable.

.. code-block:: text

   ├── kconfig.fragment        Linux kernel configuration requirements
   ├── Makefile                Build instructions
   ├── README.rst              This file
   └── src
       └── blink.c             GPIO blink application

Build
-----

Ensure that the cross-compiler and target ``libgpiod`` pkg-config metadata are
available. Then run:

.. code-block:: bash

   make CROSS_COMPILE=/path/to/toolchain/bin/riscv64-esp-linux-gnu-

When a build system already sets ``CC`` and ``PKG_CONFIG``, the Makefile uses
those tools directly and honours ``CPPFLAGS``, ``CFLAGS``, ``LDFLAGS``, and
``LDLIBS``.

Run ``make clean`` before rebuilding with a different toolchain. The executable
is written to ``build/blink``.

Deploy
------

Staging for Image or Package Creation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install the executable into a root filesystem or packaging directory:

.. code-block:: bash

   make DESTDIR=/path/to/rootfs install

This installs the executable as ``/usr/bin/blink`` inside the staged root
filesystem. For a read-only root filesystem such as cramfs, rebuild and reflash
the image after staging the executable.

Run
---

Identify the GPIO character device and line offset for the connected signal,
then run:

.. code-block:: console

   # blink /dev/gpiochip0 <line-offset>

The default delay is 500 milliseconds for each on and off state. To use a
one-second delay:

.. code-block:: console

   # blink -d 1000 /dev/gpiochip0 <line-offset>

For an active-low LED module:

.. code-block:: console

   # blink -l /dev/gpiochip0 <line-offset>

Press ``Ctrl+C`` to stop. The application requests the line exclusively and
returns it to the inactive state before releasing it.

Example Output
--------------

.. code-block:: console

   # blink -d 1000 /dev/gpiochip0 4
   Blinking /dev/gpiochip0 line 4 every 1000 ms. Press Ctrl+C to stop.
   LED ON
   LED OFF
   LED ON
   ^C

Kernel Configuration Fragment
-----------------------------

The included ``kconfig.fragment`` declares the generic Linux GPIO library and
GPIO character-device interface required by ``libgpiod``. The target also
needs the GPIO controller driver for its SoC.

Troubleshooting
---------------

* **Failed to open gpiochip:** Check that the GPIO character device exists
  under ``/dev`` and that the kernel enables ``CONFIG_GPIO_CDEV``.
* **Failed to request line:** Another driver or process may already own the
  line, or the line offset may be invalid.
* **The output changes but the LED does not light:** Check the module voltage,
  polarity, common ground, selected GPIO, and whether ``-l`` is required.
