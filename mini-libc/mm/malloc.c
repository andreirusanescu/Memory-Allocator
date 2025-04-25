// SPDX-License-Identifier: BSD-3-Clause

#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <internal/essentials.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

void *malloc(size_t size)
{
	void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;
	mem_list_add(ret, size);
	return ret;
}

void *calloc(size_t nmemb, size_t size)
{
	void *ret = mmap(NULL, size * nmemb, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;
	mem_list_add(ret, size);
	return ret;
}

void free(void *ptr)
{
	if (!ptr) return;
	struct mem_list *p = mem_list_find(ptr);
	if (!p)
		return;
	munmap(ptr, p->len);
	mem_list_del(ptr);
}

void *realloc(void *ptr, size_t size)
{
	struct mem_list *p = mem_list_find(ptr);
	if (!p) {
		void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (ret == MAP_FAILED)
			return NULL;
		mem_list_add(ret, size);
		return ret;
	}
	void *ret = mremap(ptr, p->len, size, MREMAP_MAYMOVE);
	if (ret == MAP_FAILED) {
		return NULL;
	}
	mem_list_del(ptr);
	mem_list_add(ret, size);
	return ret;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
	struct mem_list *p = mem_list_find(ptr);
	if (!p) {
		void *ret = mmap(NULL, size * nmemb, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (ret == MAP_FAILED)
			return NULL;
		mem_list_add(ret, size);
		return ret;
	}
	void *ret = mremap(ptr, p->len, size * nmemb, MREMAP_MAYMOVE);
	if (ret == MAP_FAILED) {
		return NULL;
	}
	mem_list_del(ptr);
	mem_list_add(ret, size);
	return ret;
}
