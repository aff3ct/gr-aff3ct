# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT aff3ct_sources)
    message(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT aff3ct_sources)

########################################################################
# Check for pygccxml
########################################################################
gr_python_check_module_raw("pygccxml" "import pygccxml" PYGCCXML_FOUND)

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND aff3ct_python_files turbo_encoder_python.cc turbo_decoder_python.cc
     python_bindings.cc)

gr_pybind_make_oot(aff3ct ../../.. gr::aff3ct "${aff3ct_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(
    TARGET aff3ct_python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:aff3ct_python>
            ${PROJECT_BINARY_DIR}/test_modules/gnuradio/aff3ct/)

install(
    TARGETS aff3ct_python
    DESTINATION ${GR_PYTHON_DIR}/gnuradio/aff3ct
    COMPONENT pythonapi)
