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

#ifndef __IBI_RESPONSE_I_H__
#define __IBI_RESPONSE_I_H__

#include <stddef.h>
#include <stdint.h>

#include "usbi3c.h"

struct ibi_response_queue;

/**
 * @brief A structure to describe IBI queue entry.
 */
struct ibi_response {
	struct usbi3c_ibi descriptor; ///< IBI descriptor with IBI response info
	uint8_t *data;		      ///< If the IBI has payload this is where it is stored if not it is NULL
	uint8_t size;		      ///< size of the IBI data
	uint8_t completed;	      ///< Attribute to identify if the ibi_response has been completed or have pending data to received
};

struct ibi_response_queue *ibi_response_queue_get_queue(void);
int ibi_response_queue_enqueue(struct ibi_response_queue *queue, struct ibi_response *response);
struct ibi_response *ibi_response_queue_dequeue(struct ibi_response_queue *queue);
struct ibi_response *ibi_response_queue_front(struct ibi_response_queue *queue);
struct ibi_response *ibi_response_queue_back(struct ibi_response_queue *queue);
size_t ibi_response_queue_size(struct ibi_response_queue *queue);
void ibi_response_queue_clear(struct ibi_response_queue *queue);

int ibi_response_handle(struct ibi_response_queue *queue, uint8_t *data, size_t size);

#endif /* end of include guard: __IBI_RESPONSE_I_H__ */
