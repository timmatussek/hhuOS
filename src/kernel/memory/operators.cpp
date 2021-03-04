#include <Platform.h>

void *operator new(uint32_t size) {
    return Platform::getInstance().alloc(size, 0);
}

void *operator new[](uint32_t size) {
    return Platform::getInstance().alloc(size, 0);
}

void operator delete(void *ptr) {
    Platform::getInstance().free(ptr, 0);
}

void operator delete[](void *ptr) {
    Platform::getInstance().free(ptr, 0);
}

void *operator new(uint32_t, void *p) { return p; }

void *operator new[](uint32_t, void *p) { return p; }

void operator delete(void *, void *) {}

void operator delete[](void *, void *) {}

void *operator new(uint32_t size, uint32_t alignment) {
    return Platform::getInstance().alloc(size, alignment);
}

void *operator new[](uint32_t size, uint32_t alignment) {
    return Platform::getInstance().alloc(size, alignment);
}

void operator delete(void *ptr, uint32_t alignment) {
    Platform::getInstance().free(ptr, alignment);
}

void operator delete[](void *ptr, uint32_t alignment) {
    Platform::getInstance().free(ptr, alignment);
}