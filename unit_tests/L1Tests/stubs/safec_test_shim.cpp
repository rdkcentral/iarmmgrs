/*
 * Test-only Safe C shim for host L1 builds, which do not install safeclib.
 * Production builds resolve these symbols from the real Safe C library.
 */
#include <cstddef>
#include <cstring>

extern "C" {

int strcpy_s(char *dest, std::size_t dmax, const char *src)
{
    if (dest == nullptr || src == nullptr || dmax == 0) {
        return 400;
    }
    const std::size_t length = std::strlen(src);
    if (length >= dmax) {
        dest[0] = '\0';
        return 406;
    }
    std::memcpy(dest, src, length + 1);
    return 0;
}

int memcpy_s(void *dest, std::size_t dmax, const void *src, std::size_t smax)
{
    if (dest == nullptr || src == nullptr) {
        return 400;
    }
    if (smax > dmax) {
        std::memset(dest, 0, dmax);
        return 406;
    }
    std::memmove(dest, src, smax);
    return 0;
}

int memset_s(void *dest, std::size_t dmax, int value, std::size_t n)
{
    if (dest == nullptr) {
        return 400;
    }
    if (n > dmax) {
        return 406;
    }
    std::memset(dest, value, n);
    return 0;
}

int strcmp_s(const char *dest, std::size_t dmax, const char *src, int *result)
{
    if (dest == nullptr || src == nullptr || result == nullptr || dmax == 0) {
        return 400;
    }
    *result = std::strncmp(dest, src, dmax);
    return 0;
}

} // extern "C"
