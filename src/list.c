/***************************************************************************
  list.c  -  Linked list implementation
  -------------------
  copyright            : (C) 2021 Intel Corporation
  SPDX-License-Identifier: LGPL-2.1-only
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License           *
 *   version 2.1 as published by the Free Software Foundation;             *
 *                                                                         *
 ***************************************************************************/

#include <stdlib.h>

#include "usbi3c_i.h"

/**
 * @brief Find the tail of a linked list
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @return a pointer to the last node (tail) of the linked list
 */
struct list *list_tail(struct list *list_head)
{
	struct list *node = list_head;

	while (node && node->next != NULL) {
		node = node->next;
	}
	return node;
}

/**
 * @brief Prepend a node to a linked list
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @param[in] data the data to be inserted in a new node at the beginning of a linked list
 * @return a pointer to the new head node of the linked list
 *
 * @note: if the order of the list is not important, prefer to use list_prepend over
 * list_append, it is a little faster.
 */
struct list *list_prepend(struct list *list_head, void *data)
{
	struct list *new_node;

	new_node = (struct list *)malloc_or_die(sizeof(struct list));
	new_node->next = list_head;
	new_node->data = data;

	return new_node;
}

/**
 * @brief Append a node to a linked list
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @param[in] data the data to be inserted in a new node at the end of a linked list
 * @return a pointer to the head node of the linked list
 */
struct list *list_append(struct list *list_head, void *data)
{
	struct list *new_node, *last_node;

	new_node = (struct list *)malloc_or_die(sizeof(struct list));
	new_node->next = NULL;
	new_node->data = data;

	// make sure we append to the tail of the list
	if (list_head != NULL) {
		last_node = list_tail(list_head);
		last_node->next = new_node;
	} else {
		// the linked list is empty, so add the new node to the head
		list_head = new_node;
	}

	return list_head;
}

/**
 * @brief Removes a node from the linked list.
 *
 * If a list_free_data_fn is provided, the data in the node is also freed,
 * if it is not, then only the node is freed but the data is kept.
 *
 * @param[in] list_head the head of the list containing the node to be freed
 * @param[in] list_node the node to be freed
 * @param[in] list_free_data_fn the function to free the data in the node
 * @return the new head of the list, this could have changed if the node being
 * removed was the head of the list.
 */
struct list *list_free_node(struct list *list_head, struct list *list_node, list_free_data_fn_t list_free_data_fn)
{
	struct list *node = NULL;
	struct list *previous_node = NULL;

	/* removing the node from a single linked list is tricky since we need
	 * to change the "next" value in the previous node, and we cannot go back,
	 * we will need to iterate the list again */
	for (node = list_head; node; node = node->next) {

		if (node->next == list_node) {
			/* the next node is the one we want to free,
			 * keep a pointer to it so we can update its "next" member */
			previous_node = node;
			continue;
		}

		if (node == list_node) {
			/* if a function to free the data was provided, use it,
			 * if it was not, the user probably wanted to remove the
			 * node from the list without deleting the data */
			if (list_free_data_fn) {
				list_free_data_fn(list_node->data);
			}
			if (previous_node) {
				previous_node->next = node->next;
			}
			if (node == list_head) {
				/* the node being deleted was the head of the list,
				 * we need to store the new head so we don't loose it */
				list_head = node->next;
			}
			FREE(node);
			break;
		}
	}

	return list_head;
}

/**
 * @brief Removes a all matching nodes from the linked list.
 *
 * If a list_free_data_fn is provided, the data in the node is also freed,
 * if it is not, then only the node is freed but the data is kept.
 *
 * To search for the nodes to remove, the function uses a search
 * criteria (item) and a comparison function.
 *
 * The comparison function should match this type definition:
 * int (*comparison_fn_t)(const void *a, const void *b)
 *
 * The comparison_fn function should behave like this:
 * - return 0 if there is a match with the search criteria
 * - return value > 0 if there is no match and we want to continue the search
 * - return value < 0 if there is no match and we want to stop the search
 *
 * @param[in] list_head the head of the list
 * @param[in] item the search criteria
 * @param[in] comparison_fn the function to compare data from the node with the item to match
 * @param[in] list_free_data_fn the function to free the data in the node
 * @return the new head of the list, this could have changed if the node being
 * removed was the head of the list.
 */
struct list *list_free_matching_nodes(struct list *list_head, const void *item, comparison_fn_t comparison_fn, list_free_data_fn_t list_free_data_fn)
{
	struct list *node = NULL;
	struct list *next = NULL;
	struct list *previous_node = NULL;
	int comparison_result = 0;

	/* removing the node from a single linked list is tricky since we need
	 * to change the "next" value in the previous node, and we cannot go back,
	 * we will need to iterate the list again */
	node = list_head;
	while (node) {
		next = node->next;
		comparison_result = comparison_fn(node->data, item);
		if (comparison_result == 0) {
			/* we have a match, if a function to free the data was
			 * provided, use it. If it was not, then remove the
			 * node from the list without deleting the data */
			if (list_free_data_fn) {
				list_free_data_fn(node->data);
			}
			if (previous_node) {
				previous_node->next = next;
			}
			if (node == list_head) {
				/* the node being deleted was the head of the list,
				 * we need to store the new head so we don't loose it */
				list_head = next;
			}
			FREE(node);
		} else if (comparison_result > 0) {
			/* no match */
			previous_node = node;
		} else {
			/* comparison_result < 0, stop the search */
			break;
		}
		node = next;
	}

	return list_head;
}

/**
 * @brief Frees all memory allocated by the linked list.
 *
 * It calls the appropriate list_free_data_fn for each node to free the data.
 * list_free_data_fn can be NULL if no data needs to be destroyed.
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @param[in] list_free_data_fn the function to be used to destroy the data of the node
 */
void list_free_list_and_data(struct list **list_head, list_free_data_fn_t list_free_data_fn)
{
	struct list *node = *list_head;

	while (node) {
		struct list *next = NULL;

		// use whatever function was passed to free the data (if any)
		if (list_free_data_fn) {
			list_free_data_fn(node->data);
		}
		next = node->next;
		FREE(node);

		node = next;
	}
	*list_head = NULL;
}

/**
 * @brief Frees the memory allocated by the liked list without freeing the data.
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 */
void list_free_list(struct list **list_head)
{
	list_free_list_and_data(list_head, NULL);
}

/**
 * @brief Concatenates two lists.
 *
 * @param[in] list1 a list to have the other list concatenated to
 * @param[in] list2 a list to concatenate
 * @return the concatenated list (list1 + list2)
 */
struct list *list_concat(struct list *list1, struct list *list2)
{
	struct list *tail;

	if (list1 == NULL) {
		return list2;
	}

	if (list2 == NULL) {
		return list1;
	}

	tail = list_tail(list1);
	tail->next = list2;

	return list1;
}

/**
 * @brief Searches for a node within a list.
 *
 * To search for the node, the function uses a search criteria and a
 * comparison function.
 *
 * The comparison function should match this type definition:
 * int (*comparison_fn_t)(const void *a, const void *b)
 *
 * The comparison_fn function should behave like this:
 * - return 0 if a is equal to b
 * - return value < 0 if a is lower than b
 * - return value > 0 if a is bigger than b
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @param[in] item the search criteria
 * @param[in] comparison_fn a function of the type comparison_fn_t used to match the criteria
 * with the correct node in the list
 * @return the list node that matches the criteria if found, or NULL if it doesn't find one
 */
struct list *list_search_node(struct list *list_head, const void *item, comparison_fn_t comparison_fn)
{
	struct list *node = list_head;

	for (; node; node = node->next) {
		if (comparison_fn(node->data, item) == 0) {
			return node;
		}
	}

	return NULL;
}

/**
 * @brief Searches for some data within a list.
 *
 * Similar to list_search_node() with the one difference that this function returns
 * the data within the node, not the node.
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @param[in] item the search criteria
 * @param[in] comparison_fn a function of the type comparison_fn_t used to match the criteria
 * with the correct node in the list
 * @return the data from the list node that match the criteria, or NULL if it doesn't find it
 */
void *list_search(struct list *list_head, const void *item, comparison_fn_t comparison_fn)
{
	struct list *node = NULL;

	node = list_search_node(list_head, item, comparison_fn);

	if (node) {
		return node->data;
	}

	return NULL;
}

/**
 * @brief Gets the number of nodes in a linked list.
 *
 * @param[in] list_head a pointer to the first node (head) of a linked list
 * @return the number of nodes in the list.
 */
int list_len(struct list *list_head)
{
	struct list *node = NULL;
	int len;

	if (list_head == NULL) {
		return 0;
	}

	len = 1;

	node = list_head;
	while ((node = node->next) != NULL) {
		len++;
	}

	return len;
}
