#include <assert.h>
#include <stddef.h>

#include "mfrStringValidation.h"

int main(void)
{
    char terminated[4] = {'a', 'b', 'c', '\0'};
    char unterminated[4] = {'a', 'b', 'c', 'd'};

    assert(mfrStringIsTerminated(terminated, sizeof(terminated)));
    assert(!mfrStringIsTerminated(unterminated, sizeof(unterminated)));
    assert(!mfrStringIsTerminated(NULL, sizeof(terminated)));
    assert(!mfrStringIsTerminated(terminated, 0));

    return 0;
}