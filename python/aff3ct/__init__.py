#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package

'''
This is the GNU Radio AFF3CT module. Place your Python package
description here (python/__init__.py).
'''
import os

from gnuradio import fec  # registers the generic_encoder / generic_decoder bindings

# import pybind11 generated symbols into the aff3ct namespace
try:
    # this might fail if the module is python-only
    from .aff3ct_python import *
except ModuleNotFoundError:
    pass

# import any pure python here
#
