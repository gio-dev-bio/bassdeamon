#pragma once
#include <cstdlib>
#include <new>
namespace allocation { thread_local bool watch=false; thread_local size_t count=0; }
void* operator new(std::size_t n){if(allocation::watch)++allocation::count;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){if(allocation::watch)++allocation::count;void* p=nullptr;if(posix_memalign(&p,static_cast<size_t>(a),n?n:1)==0)return p;throw std::bad_alloc();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::align_val_t) noexcept{std::free(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t,std::align_val_t) noexcept{std::free(p);}
