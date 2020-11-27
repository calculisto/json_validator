isto::json_validator
====================

A C++20 headers-only library


Requirements
------------

- `taocpp/json <https://github.com/taocpp/json>`_
- `isto/uri <https://github.com/le-migou/uri>`_

To run the tests:

- `GNU Make <https://www.gnu.org/software/make/>`_.
- The `onqtam/doctest library <https://github.com/onqtam/doctest>`_.


Installation
------------

This is a headers-only library. Just put it where your compiler can find it.

It needs `taocpp/json <https://github.com/taocpp/json>`_ and 
`isto/uri <https://github.com/le-migou/uri>`_ which are both also headers-only.


Tests
-----

The tests require the `onqtam/doctest library`_.
Edit the ``config.mk`` file to make the ``DOCTEST_HEADERS`` variable point to 
the directory containing ``doctest/doctest.h``. 

To execute the tests run

    $ make check

in the root directory of the project.


License
-------

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception


Affiliation
-----------

This material is developed by the Scientific Computations and Modelling
platform at the Institut des Sciences de la Terre d'Orléans
(https://www.isto-orleans.fr/), a joint laboratory of the University of Orléans
(https://www.univ-orleans.fr/), the french National Center For Scientific
Research (https://www.cnrs.fr/) and the french Geological Survey
(https://www.brgm.eu/).
