#ifndef EBE16798_7A4D_4AB5_A8EC_0F6457DA42F6
#define EBE16798_7A4D_4AB5_A8EC_0F6457DA42F6

#include <cstdint>

namespace ob
{
    using Price = int32_t;
    using Qty = uint32_t;
    using OrderId = uint64_t;

    enum class Side : uint8_t
    {
        Buy,
        Sell
    };
}

#endif /* EBE16798_7A4D_4AB5_A8EC_0F6457DA42F6 */
