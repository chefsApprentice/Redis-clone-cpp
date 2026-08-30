#pragma once
#include <cstddef>
#define OMP_CONTAINER_OF(ptr, type, member)                                                        \
    reinterpret_cast<type*>(reinterpret_cast<const char*>(ptr) - offsetof(type, member))

template <typename Owner, typename Member>
auto container_of(Member* ptr, Member Owner::*member) -> Owner* {
    auto* impl = reinterpret_cast<const char*>(ptr);
    auto offset = reinterpret_cast<std::ptrdiff_t>(&(((Owner*)nullptr)->*member));
    return reinterpret_cast<Owner*>(impl - offset);
}

template <typename Owner, typename Member>
auto container_of(const Member* ptr, Member Owner::*member) -> const Owner* {
    auto* impl = reinterpret_cast<const char*>(ptr);
    auto offset = reinterpret_cast<std::ptrdiff_t>(&(((Owner*)nullptr)->*member));
    return reinterpret_cast<const Owner*>(impl - offset);
}

