#ifndef AF2032C2_D594_4C28_8414_5BF0230993B8
#define AF2032C2_D594_4C28_8414_5BF0230993B8

#include <ob/types.hpp>

namespace ob
{
    enum class MsgType : uint8_t
    {
        NewLimit,
        NewMarket,
        Cancel
    };

    struct Message
    {
        MsgType m_msgType;
        OrderId m_orderId;
        Side m_side;
        Price m_price;
        Qty m_qty;
    };

    enum class EventType : uint8_t
    {
        Ack,
        Trade,
        CancelAck,
        Reject
    };

    struct Event
    {
        EventType m_eventType;
        OrderId m_orderId;
        OrderId m_counterId;
        Price m_price;
        Qty m_qty;

        friend bool operator==(const Event &a, const Event &b)
        {
            return a.m_eventType == b.m_eventType &&
                   a.m_orderId == b.m_orderId &&
                   a.m_counterId == b.m_counterId &&
                   a.m_price == b.m_price &&
                   a.m_qty == b.m_qty;
        }

        friend bool operator!=(const Event &a, const Event &b)
        {
            return !(a == b);
        }
    };

}

#endif /* AF2032C2_D594_4C28_8414_5BF0230993B8 */
