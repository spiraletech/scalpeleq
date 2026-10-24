#include <cstddef>
#include <cstdlib>
#include <new>

namespace {
thread_local bool measuring = false;
thread_local std::size_t allocations = 0;
}
void startAllocationProbe() noexcept { allocations = 0; measuring = true; }
std::size_t finishAllocationProbe() noexcept { measuring = false; return allocations; }

void* operator new(std::size_t bytes) {
    if(measuring)++allocations;
    if(auto* result=std::malloc(bytes==0?1:bytes))return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer,std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer,std::size_t) noexcept { std::free(pointer); }
