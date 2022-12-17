#                                               -*- cmake -*-
#
#  UseLibUSBI3C.cmake
#
#  Copyright (C) 2021 Intel Corporation
#
#  This file is part of LibUSBI3C.
#
#  LibUSBI3C is free software; you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License
#  version 2.1 as published by the Free Software Foundation;
#


add_definitions     ( ${LIBUSBI3C_DEFINITIONS} )
include_directories ( ${LIBUSBI3C_INCLUDE_DIRS} )
link_directories    ( ${LIBUSBI3C_LIBRARY_DIRS} )

