#include <sys/mman.h>
#include "alloc.h"
#include <cstring>
#include <cstdlib>

namespace toy {

static constexpr size_t MMAP_THRESHOLD = 64 * (1ULL << 20);

template<bool clear_mem>
void * StepAllocator<clear_mem>::alloc(size_t n) {
    void * buf;
    if (n < MMAP_THRESHOLD) {
        if (clear_mem)
            buf = ::calloc(n,1);
        else
            buf = ::malloc(n);
        if (buf == nullptr)
            throw "bad alloc";
    } else {
        buf = mmap(nullptr, n,  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (MAP_FAILED == buf)
            throw "bad alloc";
    }
    return buf;
}

template<bool clear_mem>
void * StepAllocator<clear_mem>::free(void * buf, size_t n)
{
    if (n < MMAP_THRESHOLD) {
        ::free(buf);
    } else {
        if (0 != munmap(buf, n))
            throw "bad alloc";
    }
}

template<bool clear_mem>
void * StepAllocator<clear_mem>::realloc(void * buf, size_t old_size, size_t new_size) {
    if (old_size > MMAP_THRESHOLD && new_size > MMAP_THRESHOLD) {
        buf = mremap(buf, old_size, new_size, MREMAP_MAYMOVE,  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (MAP_FAILED == buf)
            throw "bad alloc";
    }
    else if (old_size < MMAP_THRESHOLD && new_size >= MMAP_THRESHOLD) {
        void * new_buf = alloc(new_size);
        memcpy(new_buf, buf, old_size);
        free(buf, old_size);
        buf = new_buf;
    } else if (old_size >= MMAP_THRESHOLD && new_size < MMAP_THRESHOLD) {
        void * new_buf = alloc(new_size);
        memcpy(new_buf, buf, new_size);
        if( 0 != munmap(buf, old_size)) {
            free(new_buf, new_size);
            throw "cannot unmap";
        }
        buf = new_buf;
    } else {
        buf = ::realloc(buf, new_size);
        if (nullptr == buf)
            throw "bad alloc";
        if (clear_mem && new_size > old_size)
            memset(reinterpret_cast<char *>(buf) + old_size, 0, new_size - old_size);
    }

    return buf;
}

template  class StepAllocator<true>;
template  class StepAllocator<false>;


//NodeAllocator:: NodeAllocator() {
//    for (int i = 0; i < 16; i++)
//    {
//
//    }
//}

}
