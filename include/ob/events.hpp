#ifndef AF2032C2_D594_4C28_8414_5BF0230993B8
#define AF2032C2_D594_4C28_8414_5BF0230993B8

#include <ob/types.hpp>

namespace ob
{
    enum class MsgType : uint8_t
    {
        Unknown,
        NewLimit,
        NewMarket,
        Cancel
    };

    struct Message
    {
        MsgType m_msgType{MsgType::Unknown};
        OrderId m_orderId{0};
        Side m_side{Side::Unknown};
        Price m_price{0};
        Qty m_qty{0};
    };

    enum class EventType : uint8_t
    {
        Unknown,
        Ack,
        Trade,
        CancelAck,
        Reject
    };

    struct Event
    {
        EventType m_eventType{EventType::Unknown};
        OrderId m_orderId{0};
        OrderId m_counterId{0};
        Price m_price{0};
        Qty m_qty{0};

        /**
         * @brief Compares all fields of two events for equality.
         * @param a First event to compare.
         * @param b Second event to compare.
         * @return True when every event field is equal.
         */
        friend bool operator==(const Event &a, const Event &b)
        {
            return a.m_eventType == b.m_eventType &&
                   a.m_orderId == b.m_orderId &&
                   a.m_counterId == b.m_counterId &&
                   a.m_price == b.m_price &&
                   a.m_qty == b.m_qty;
        }

        /**
         * @brief Compares two events for inequality.
         * @param a First event to compare.
         * @param b Second event to compare.
         * @return True when at least one event field differs.
         */
        friend bool operator!=(const Event &a, const Event &b)
        {
            return !(a == b);
        }
    };

}

#endif /* AF2032C2_D594_4C28_8414_5BF0230993B8 */
