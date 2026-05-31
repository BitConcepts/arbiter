.. SPDX-License-Identifier: MIT

Getting Started
###############

Prerequisites
*************

- Zephyr SDK (latest)
- west build tool
- Python 3.10+
- pip

Adding arbiter to Your Project
*****************************

Add arbiter to your west manifest::

    manifest:
      projects:
        - name: arbiter
          remote: bitconcepts
          revision: main
          path: modules/lib/arbiter

Then run::

    west update

Building a Sample
*****************

::

    west build -b native_sim modules/lib/arbiter/samples/battery_policy
    west build -t run

Using arbiterc
************

Install the compiler::

    pip install -e modules/lib/arbiter

Validate a model::

    arbiterc validate model.arb.yaml --strict

Compile to C::

    arbiterc compile model.arb.yaml --out-c model.c --out-h model.h
