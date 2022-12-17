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

#ifndef __LIST_I_H__
#define __LIST_I_H__

/*
 * @brief Structure representing a singly linked list node.
 */
struct list {
	void *data;
	struct list *next;
};

/** linked list declarations **/
typedef void (*list_free_data_fn_t)(void *data); ///< Callback to free data used in a linked list
struct list *list_tail(struct list *list_head);
struct list *list_prepend(struct list *list_head, void *data);
struct list *list_append(struct list *list_head, void *data);
void list_free_list_and_data(struct list **list_head, list_free_data_fn_t list_free_data_fn);
void list_free_list(struct list **list_head);
typedef int (*comparison_fn_t)(const void *a, const void *b);
struct list *list_free_node(struct list *list_head, struct list *list_node, list_free_data_fn_t list_free_data_fn);
struct list *list_free_matching_nodes(struct list *list_head, const void *item, comparison_fn_t comparison_fn, list_free_data_fn_t list_free_data_fn);
struct list *list_concat(struct list *list1, struct list *list2);
struct list *list_search_node(struct list *list_head, const void *item, comparison_fn_t comparison_fn);
void *list_search(struct list *list_head, const void *item, comparison_fn_t comparison_fn);
int list_len(struct list *list_head);

#endif /* end of include guard: __LIST_I_H__ */
