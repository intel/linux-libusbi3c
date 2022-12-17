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

#ifndef __COMMON_I_H__
#define __COMMON_I_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "DEBUG: %s(): " fmt, __func__, ##__VA_ARGS__) // ignore_linebreak
#else
#define DEBUG_PRINT(...) (void)0 // ignore_linebreak
#endif

/**
 * @brief Macro that takes three arguments - a pointer, type of the container, and the
 * name of the member the pointer refers to. The macro will then expand to a new address
 * pointing to the container which accommodates the respective member.
 */
#define container_of(ptr, type, member) ((type *)((char *)(ptr)-offsetof(type, member)))

/**
 * @brief Macro to abort program execution if the value of the variable is NULL.
 * This macro is useful to be used in situations where unrecoverable errors have
 * occurred, for example running out of memory.
 */
#define ON_NULL_ABORT(var) \
	if (!var) {        \
		abort();   \
	}

/**
 * @brief wrapper for the free function that always set the pointer to NULL after freeing.
 * This macro should be used instead of free to prevent invalid memory access.
 */
#define FREE(ptr)           \
	do {                \
		free(ptr);  \
		ptr = NULL; \
	} while (0)

/**
 * @brief Allocates memory of the specified size and initializes it.
 *
 * This is a wrapper function to be used instead of calling malloc directly.
 * It uses calloc to make sure the memory gets initialized with zero,this
 * way we avoid getting segmentation faults.
 * Abort on out of memory errors, continuing the program at this point
 * would cause unexpected behaviors.
 */
static inline void *malloc_or_die(size_t size)
{
	void *ptr;
	ptr = calloc(1, (size_t)size); // ignore_function
	ON_NULL_ABORT(ptr);

	return ptr;
}

/**
 * @brief Reallocates memory of the specified size and initializes it.
 *
 * This is a wrapper function to be used instead of calling realloc directly.
 * It uses realloc to reallocate memory, this way we avoid having errors if
 * the memory is not allocated.
 * Abort on out of memory errors, continuing the program at this point
 * would cause unexpected behaviors.
 */
static inline void *realloc_or_die(void *ptr, size_t size)
{
	void *new_ptr;
	new_ptr = realloc(ptr, (size_t)size); // ignore_function
	ON_NULL_ABORT(new_ptr);

	return new_ptr;
}

#endif /* end of include guard: __COMMON_I_H__ */
