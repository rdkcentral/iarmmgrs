#ifndef MFR_STRING_VALIDATION_H
#define MFR_STRING_VALIDATION_H

#include <stddef.h>

static inline int mfrStringIsTerminated(const char *value, size_t capacity)
{
    size_t length = 0;

    if(value == NULL || capacity == 0)
    {
        return 0;
    }

    while(length < capacity && value[length] != '\0')
    {
        ++length;
    }

    return length < capacity;
}

#endif