// SPDX-License-Identifier: BSD-3-Clause

/*
 * Copyright (c) 2024, Andrei Rusanescu <andreirusanescu154gmail.com>
 */

#include <syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include "osmem.h"
#include "block_meta.h"

/* Helper macros */
#define ALIGNMENT 8
#define ALIGN(size) (((size_t)(size) + (ALIGNMENT - 1)) & ~0x7)
#define PAGE_SIZE ((4) * (1024))
#define MMAP_THRESHOLD ((128) * (1024))

static struct block_meta *heap_start, *heap_end;

/* NULL or the block preallocated on heap */
static void *heap_pre;

/**
 * Allocates a block with mmap()
 * @param size to be allocated
 * @return returns the newly allocates block
 */
static struct block_meta *mmap_alloc(size_t req)
{
	struct block_meta *new_block = (struct block_meta *)mmap(NULL, req, PROT_READ | PROT_WRITE,
							  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	DIE(new_block == MAP_FAILED, "mmap() failed");

	new_block->size = req - ALIGN(sizeof(struct block_meta));
	new_block->status = STATUS_MAPPED;
	new_block->prev = heap_end;
	new_block->next = NULL;
	return new_block;
}

/**
 * Splits a block in req size and remaining_size
 * @param node split node
 * @param remaining_size size of the new block
 * @param req size of the node
 */
static void fragment(struct block_meta *node, size_t remaining_size, size_t req)
{
	struct block_meta *fragmented = (struct block_meta *)((char *)node + req);

	fragmented->next = node->next;
	fragmented->prev = node;
	node->next = fragmented;
	if (fragmented->next)
		fragmented->next->prev = fragmented;


	fragmented->size = remaining_size - ALIGN(sizeof(struct block_meta));
	fragmented->status = STATUS_FREE;
	node->size = req - ALIGN(sizeof(struct block_meta));

	/* Update heap_end if the split node is at the end of the heap */
	if (node == heap_end)
		heap_end = fragmented;
}

/**
 * Find best approach of finding a free block of
 * at least @param requested size
 * @return the best block or NULL
 */
static struct block_meta *find_fit(size_t requested)
{
	struct block_meta *node;
	size_t remaining_size;
	struct block_meta *best_node = NULL;
	size_t minimal_remaining = (size_t)-1;

	for (node = heap_start; node != NULL; node = node->next) {
		if (node->status == STATUS_FREE && sizeof(struct block_meta) + node->size >= requested) {
			remaining_size = sizeof(struct block_meta) + node->size - requested;
			if (remaining_size < minimal_remaining) {
				minimal_remaining = remaining_size;
				best_node = node;
				/* Best node possible */
				if (remaining_size == 0)
					break;
			}
		}
	}

	/* Found the best node */
	if (best_node) {
		/* Can be split */
		if (minimal_remaining >= ALIGN(sizeof(struct block_meta)) + 1)
			fragment(best_node, minimal_remaining, requested);
		return best_node;
	}
	return NULL;
}

/**
 * Initialises the heap with sbrk() (ideally) or mmap()
 * @param requested the requested size
 */
static struct block_meta *initialise_heap(size_t requested)
{
	struct block_meta *new_block = NULL;

	if (requested <= MMAP_THRESHOLD && !heap_pre) {
		new_block = (struct block_meta *)sbrk(MMAP_THRESHOLD);
		DIE(new_block == ((void *)-1), "sbrk() failed");
		size_t remaining_size = MMAP_THRESHOLD - requested;

		heap_pre = (void *)(new_block + 1);
		new_block->status = STATUS_ALLOC;
		new_block->size = requested - ALIGN(sizeof(struct block_meta));

		if (!heap_start) {
			heap_start = heap_end = new_block;
			heap_start->next = heap_start->prev = NULL;
		} else {
			new_block->prev = heap_end;
			heap_end->next = new_block;
			heap_end = new_block;
		}

		if (remaining_size >= ALIGN(sizeof(struct block_meta)) + 1)
			fragment(new_block, remaining_size, requested);
	} else {
		new_block = mmap_alloc(requested);
		if (!heap_start) {
			heap_start = heap_end = new_block;
			heap_start->next = heap_start->prev = NULL;
		} else {
			heap_end->next = new_block;
			heap_end = new_block;
			new_block->prev = heap_end;
		}
	}

	return new_block;
}

/**
 * Simulates malloc() function from libc
 * Allocates @param size bytes
 * @return Address of the allocated chunk
 */
void *os_malloc(size_t size)
{
	if (!size)
		return NULL;
	/* Total padded size */
	size_t requested = ALIGN(sizeof(struct block_meta)) + ALIGN(size);
	struct block_meta *new_block = NULL;

	/* Initialise heap with a preallocated size */
	if (!heap_pre)
		return initialise_heap(requested) + 1;

	/* Try finding an empty block */
	new_block = find_fit(requested);
	if (new_block) {
		new_block->status = STATUS_ALLOC;
		return new_block + 1;
	}

	/* Requested size is over MMAP_THRESHOLD => MMAP allocation */
	if (requested > MMAP_THRESHOLD) {
		new_block = mmap_alloc(requested);
		heap_end->next = new_block;
		heap_end = heap_end->next;
		return new_block + 1;
	}

	/* Get the last block on the heap */
	new_block = heap_end;
	while (new_block && new_block->status == STATUS_MAPPED)
		new_block = new_block->prev;

	/* Try expanding the last block on the heap with the required size */
	if (new_block && new_block->status == STATUS_FREE) {
		void *addr = (void *)sbrk(ALIGN(size) - new_block->size);

		DIE(addr == ((void *)-1), "sbrk() failed");
		new_block->size = ALIGN(size);
		new_block->status = STATUS_ALLOC;
		return new_block + 1;
	}

	new_block = (struct block_meta *)sbrk(requested);
	DIE(new_block == ((void *)-1), "sbrk() failed");
	new_block->size = ALIGN(size);
	new_block->prev = heap_end;
	new_block->next = NULL;
	heap_end->next = new_block;
	heap_end = heap_end->next;
	new_block->status = STATUS_ALLOC;
	return new_block + 1;
}

/**
 * Simulates free() from libc
 * @param ptr address to be freed
 */
void os_free(void *ptr)
{
	if (!ptr)
		return;

	struct block_meta *addr = (struct block_meta *)((char *)ptr - ALIGN(sizeof(struct block_meta)));
	struct block_meta *next = addr->next, *prev = addr->prev;
	long res;

	if (addr->status == STATUS_MAPPED) {
		if (prev)
			prev->next = next;
		else
			heap_start = next;
		if (next)
			next->prev = prev;
		else
			heap_end = prev;
		res = munmap(addr, ALIGN(sizeof(struct block_meta)) + ALIGN(addr->size));
		DIE(res < 0, "munmap() failed");
		if (!heap_end || !heap_start)
			heap_pre = NULL;

		/* Try to coalesce 2 free blocks */
		if (next && prev) {
			if (prev->status == STATUS_FREE && next->status == STATUS_FREE) {
				prev->size += ALIGN(sizeof(struct block_meta)) + next->size;
				prev->next = next->next;
				if (next->next)
					next->next->prev = prev;
				if (next == heap_end)
					heap_end = prev;
			}
		}
		return;
	}

	addr->status = STATUS_FREE;
	if (next && next->status == STATUS_FREE) {
		addr->size += ALIGN(sizeof(struct block_meta)) + next->size;
		addr->next = next->next;
		if (next->next)
			next->next->prev = addr;
		if (next == heap_end)
			heap_end = addr;
	}

	if (prev && prev->status == STATUS_FREE) {
		prev->size += ALIGN(sizeof(struct block_meta)) + addr->size;
		prev->next = addr->next;
		if (addr->next)
			addr->next->prev = prev;
		if (addr == heap_end)
			heap_end = prev;
	}
}

static void *memset(void *source, int value, size_t num)
{
	char *src = (char *)source;

	for (size_t i = 0; i < num; ++i)
		*(src + i) = value;
	return source;
}

/**
 * Simulates calloc() function from libc
 * Allocates @param nmemb * @param size bytes
 * and sets them to 0
 * @return the address of the allocated pointer
 */
void *os_calloc(size_t nmemb, size_t size)
{
	if (!nmemb || !size)
		return NULL;
	size_t total = ALIGN(nmemb * size);

	/* Total allocation cost >= PAGE_SIZE => mmap */
	if (total + sizeof(struct block_meta) >= PAGE_SIZE) {
		struct block_meta *new_block = mmap_alloc(total + sizeof(struct block_meta));

		if (heap_end) {
			heap_end->next = new_block;
			heap_end = heap_end->next;
		} else {
			heap_start = heap_end = new_block;
		}
		return new_block + 1;
	}

	void *addr = os_malloc(total);

	if (addr)
		memset(addr, 0, total);
	return addr;
}

static void *memcpy(void *destination, const void *source, size_t num)
{
	char *dst = (char *)destination;
	char *src = (char *)source;

	for (size_t i = 0; i < num; ++i)
		*(dst + i) = *(src + i);
	return dst;
}

/**
 * Simulates realloc() function from libc
 * Reallocates @param ptr to @param size bytes
 * @return the address of the reallocated pointer
 */
void *os_realloc(void *ptr, size_t size)
{
	size_t requested = ALIGN(size);

	if (!ptr)
		return os_malloc(size);

	if (!requested) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *block = (struct block_meta *)((char *)ptr - ALIGN(sizeof(struct block_meta)));
	size_t prev_size = block->size, remaining_size;
	void *addr;

	if (block->status == STATUS_FREE)
		return NULL;

	/* MAPPED blocks are not truncated or expanded */
	if (block->status == STATUS_MAPPED && block->size != requested) {
		addr = os_malloc(size);
		memcpy(addr, ptr, prev_size < requested ? prev_size : requested);
		os_free(ptr);
		return addr;
	} else if (block->size == requested) {
		return block + 1;
	}

	/* truncate */
	if (requested < block->size) {
		remaining_size = block->size - requested;

		/* if possible split block */
		if (remaining_size >= ALIGN(sizeof(struct block_meta)) + 1)
			fragment(block, remaining_size, requested + ALIGN(sizeof(struct block_meta)));
		block->status = STATUS_ALLOC;
		return block + 1;
	}

	/* Try expanding the last block */
	if (requested - block->size <= MMAP_THRESHOLD) {
		struct block_meta *last = heap_end;

		/* get the last heap block */
		while (last && last->status == STATUS_MAPPED)
			last = last->prev;

		/* last block is our block => can be expanded*/
		if (last == block) {
			addr = (void *)sbrk(requested - block->size);
			DIE(addr == ((void *)-1), "sbrk() failed");
			block->size = requested;
			return block + 1;
		}
	}

	/* try coalescing blocks */
	struct block_meta *next = block->next;

	if (next && next->status == STATUS_FREE) {
		block->size += ALIGN(sizeof(struct block_meta)) + next->size;
		block->next = next->next;
		if (next->next)
			next->next->prev = block;
		if (next == heap_end)
			heap_end = block;

		if (block->size >= requested) {
			remaining_size = block->size - requested;
			if (remaining_size >= ALIGN(sizeof(struct block_meta)) + 1)
				fragment(block, remaining_size, requested + ALIGN(sizeof(struct block_meta)));
			return block + 1;
		}
	}

	addr = os_malloc(size);
	memcpy(addr, ptr, prev_size < size ? prev_size : size);
	if (requested > MMAP_THRESHOLD && block == heap_pre)
		heap_pre = NULL;
	os_free(ptr);

	return addr;
}
