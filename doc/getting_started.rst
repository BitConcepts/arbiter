.. SPDX-License-Identifier: MIT

Getting Started
###############

Prerequisites
*************

- Zephyr SDK (latest)
- west build tool
- Python 3.10+
- pip

Adding zproj to Your Project
*****************************

Add zproj to your west manifest::

    manifest:
      projects:
        - name: zproj
          remote: bitconcepts
          revision: main
          path: modules/lib/zproj

Then run::

    west update

Building a Sample
*****************

::

    west build -b native_sim modules/lib/zproj/samples/battery_policy
    west build -t run

Using zprojc
************

Install the compiler::

    pip install -e modules/lib/zproj

Validate a model::

    zprojc validate model.zrm.yaml --strict

Compile to C::

    zprojc compile model.zrm.yaml --out-c model.c --out-h model.h
