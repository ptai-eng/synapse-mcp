#include <version>
#include <iostream>

int main() {
#if defined(_MSVC_STL_VERSION) && defined(_MSVC_STL_UPDATE)
    std::cout << "MSVC STL _MSVC_STL_VERSION=" << _MSVC_STL_VERSION
              << "; _MSVC_STL_UPDATE=" << _MSVC_STL_UPDATE;
#elif defined(__GLIBCXX__)
    std::cout << "libstdc++ __GLIBCXX__=" << __GLIBCXX__;
#elif defined(_LIBCPP_VERSION)
    std::cout << "libc++ _LIBCPP_VERSION=" << _LIBCPP_VERSION;
#else
#error Cannot identify the standard library used for interop
#endif
    std::cout << '\n';
}
