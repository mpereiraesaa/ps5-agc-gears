#include "../include/ps5_agc.h"
#include "../include/ps5_agc_driver.h"

#include <assert.h>
#include <stddef.h>

_Static_assert(sizeof(ps5_agc_register) == 8u, "AGC register pair ABI");
_Static_assert(offsetof(ps5_agc_register, value) == 4u,
               "AGC register value ABI");
_Static_assert(PS5_AGC_GEARS_PARAMETER_OFFSET ==
                   PS5_AGC_SH_USER_DATA_GS_0 + 1,
               "gears user-data layout");
_Static_assert(PS5_AGC_GEARS_DRAW_WORDS ==
                   PS5_AGC_GEARS_PARAMETER_WORDS + 1,
               "gears user-data word count");

int main(void)
{
    int32_t (*init_fn)(void *, uint32_t) = sceAgcInit;
    int32_t (*submit_fn)(void *) = sceAgcDriverSubmitDcb;
    uint32_t *(*flip_fn)(void *, uint32_t, int32_t, uint32_t, int64_t) =
        sceAgcDcbSetFlip;
    assert(init_fn != NULL && submit_fn != NULL && flip_fn != NULL);
    return 0;
}
