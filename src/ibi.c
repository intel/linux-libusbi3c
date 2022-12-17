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
#include "ibi_i.h"
#include "usbi3c_i.h"

/**
 * @brief A structure that represent an IBI notification entry
 */
struct ibi_entry {
	uint8_t report;	     ///< cause of IBI notification
	on_ibi_fn on_ibi_cb; ///< function to be called when the IBI is completed
	void *user_data;     ///< user data to share with the function callback
};

/**
 * @brief A structure used to handle IBI notifications
 */
struct ibi {
	struct list *head;			   ///< list to store IBI data until its IBI response is received
	struct ibi_response_queue *response_queue; ///< IBI response queue to handle IBI responses
	on_ibi_fn on_ibi_cb;			   ///< callback to be assigned to the ibi_entry and called once the IBI is completed
	void *user_data;			   ///< user_data to be assigned to the ibi_entry and used once the IBI is completed
};

/**
 * @brief Function to handle IBI notifications
 *
 * @param[in] notification notification information
 * @param[in] user_data user data
 */
void ibi_handle_notification(struct notification *notification, void *user_data)
{
	struct ibi *ibi = (struct ibi *)user_data;
	struct ibi_entry *entry;
	entry = malloc_or_die(sizeof(struct ibi_entry));
	entry->report = notification->code;
	entry->on_ibi_cb = ibi->on_ibi_cb;
	entry->user_data = ibi->user_data;
	ibi->head = list_append(ibi->head, entry);
	ibi_call_pending(ibi);
}

/**
 * @brief destroy an IBI structure
 *
 * @param[in] ibi the IBI structure to be destroyed
 */
void ibi_destroy(struct ibi **ibi)
{
	if (!ibi || !*ibi) {
		return;
	}
	list_free_list_and_data(&(*ibi)->head, free);
	FREE(*ibi);
}

/**
 * @brief Create a new IBI structure
 *
 * IBI structure is used to handle IBI notifications and IBI responses
 *
 * @param[in] response_queue IBI response queue to handle IBI responses
 * @return a new IBI structure
 */
struct ibi *ibi_init(struct ibi_response_queue *response_queue)
{
	if (response_queue == NULL) {
		return NULL;
	}

	struct ibi *ibi;
	ibi = malloc_or_die(sizeof(struct ibi));
	ibi->response_queue = response_queue;
	return ibi;
}
/**
 * @brief Function to set callback to call when the IBI notification is completed
 *
 * @param[in] ibi structure to handle IBI notification
 * @param[in] on_ibi_cb callback to be called when the IBI notification is completed
 * @param[in] user_data data to share with the function callback
 */
void ibi_set_callback(struct ibi *ibi, on_ibi_fn on_ibi_cb, void *user_data)
{
	if (ibi == NULL) {
		return;
	}
	ibi->on_ibi_cb = on_ibi_cb;
	ibi->user_data = user_data;
}

/**
 * @brief Function to get ibi info that has been completed and execute its callback
 *
 * @param[in] ibi structure to handle IBI notification
 */
void ibi_call_pending(struct ibi *ibi)
{
	if (ibi == NULL) {
		return;
	}

	if (!ibi_response_queue_size(ibi->response_queue) || !ibi->head) {
		return;
	}

	if (!ibi_response_queue_front(ibi->response_queue)->completed) {
		return;
	}

	struct list *head = ibi->head;
	struct ibi_entry *entry = head->data;
	struct ibi_response *response = ibi_response_queue_dequeue(ibi->response_queue);

	if (entry->on_ibi_cb) {
		entry->on_ibi_cb(entry->report,
				 &response->descriptor,
				 response->data,
				 response->size,
				 entry->user_data);
	}
	ibi->head = head->next;
	FREE(entry);
	FREE(head);
	FREE(response->data);
	FREE(response);
}
