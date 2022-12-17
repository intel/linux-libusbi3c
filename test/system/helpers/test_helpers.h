/***************************************************************************
  USBI3C  -  Library to talk to I3C devices via USB.
  -------------------
  copyright            : (C) 2022 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation.             *
 *                                                                         *
 ***************************************************************************/

#ifndef __system_test_helpers_h__
#define __system_test_helpers_h__

#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

#include "usbi3c.h"

#define RETURN_SUCCESS 0
#define RETURN_FAILURE -1

#define TRUE 1
#define FALSE 0

/* test_helpers.c */
struct usbi3c_device *helper_usbi3c_controller_init(void);

#endif // __system_test_helpers_h__
