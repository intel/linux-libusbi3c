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
#ifndef __IBI_I_H__
#define __IBI_I_H__

#include "ibi_response_i.h"
#include "usbi3c.h"

struct ibi;

struct notification;
struct ibi *ibi_init(struct ibi_response_queue *response_queue);
void ibi_destroy(struct ibi **ibi);
void ibi_handle_notification(struct notification *notification, void *user_data);
void ibi_set_callback(struct ibi *ibi, on_ibi_fn ibi_cb, void *user_data);
void ibi_call_pending(struct ibi *ibi);

#endif /* end of include guard: __IBI_I_H__ */
