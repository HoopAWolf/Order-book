#ifndef AD113F7B_E5B0_4EAD_A19F_570898FA5891
#define AD113F7B_E5B0_4EAD_A19F_570898FA5891

#include <deque>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <cassert>
#include <limits>
#include <ob/types.hpp>
#include <ob/events.hpp>

namespace ob
{
    class ReferenceBook
    {
        struct Resting
        {
            OrderId m_orderId{0};
            Qty m_qty{0};
        };

        using Level = std::deque<Resting>;
        using BidMap = std::map<Price, Level, std::greater<Price>>;
        using AskMap = std::map<Price, Level, std::less<Price>>;

        struct Locator
        {
            Side m_side;
            Price m_price;
        };

        BidMap m_bids;
        AskMap m_asks;
        std::unordered_map<OrderId, Locator> m_index;

    public:
        /**
         * @brief Processes a new order or cancellation message.
         * @param _msg Message to apply to the book.
         * @param _out Receives acknowledgment, rejection, and trade events.
         */
        void process(const Message &_msg, std::vector<Event> &_out)
        {
            switch (_msg.m_msgType)
            {
            case MsgType::NewLimit:
                onNew(_msg, false, _out);
                break;
            case MsgType::NewMarket:
                onNew(_msg, true, _out);
                break;
            case MsgType::Cancel:
                onCancel(_msg.m_orderId, _out);
                break;
            case MsgType::Unknown:
                assert(false);
                break;
            }
        }

        /**
         * @brief Returns the highest bid price, if the bid book is nonempty.
         * @return Best bid price, or std::nullopt when there are no bids.
         */
        std::optional<Price> bestBid() const
        {
            return m_bids.empty() ? std::nullopt : std::optional<Price>(m_bids.begin()->first);
        }

        /**
         * @brief Returns the lowest ask price, if the ask book is nonempty.
         * @return Best ask price, or std::nullopt when there are no asks.
         */
        std::optional<Price> bestAsk() const
        {
            return m_asks.empty() ? std::nullopt : std::optional<Price>(m_asks.begin()->first);
        }

        /**
         * @brief Calculates the total resting quantity at a price.
         * @param _side Side of the book to query.
         * @param _px Price level to query.
         * @return Total remaining quantity at the specified side and price.
         */
        Qty qtyAt(Side _side, Price _px) const
        {
            return _side == Side::Buy ? levelQty(m_bids, _px) : levelQty(m_asks, _px);
        }

        /**
         * @brief Returns the number of orders currently resting in the book.
         * @return Number of indexed resting orders.
         */
        std::size_t orderCount() const
        {
            return m_index.size();
        }

        /**
         * @brief Asserts that the book, order index, and uncrossed-book rules agree.
         *
         * This diagnostic check has no effect when assertions are disabled.
         */
        void checkInvariants() const
        {
            std::size_t n = checkSide(m_bids, Side::Buy) + checkSide(m_asks, Side::Sell);
            assert(n == m_index.size()); // Every resting order is indexed and vice versa

            if (!m_bids.empty() && !m_asks.empty())
            {
                assert(m_bids.begin()->first < m_asks.begin()->first); // Book is never crossed
            }

            (void)n;
        }

        /**
         * @brief Checks whether an order ID belongs to a currently resting order.
         * @param _id Order ID to look up.
         * @return True if the order is currently present in the book.
         */
        bool contains(OrderId _id) const
        {
            return m_index.count(_id) != 0;
        }

    private:
        /**
         * @brief Validates, acknowledges, matches, and possibly rests a new order.
         * @param _msg New order message to process.
         * @param _isMarket Whether the order is a market order.
         * @param _out Receives acknowledgment, rejection, and trade events.
         */
        void onNew(const Message &_msg, bool _isMarket, std::vector<Event> &_out)
        {
            if (_msg.m_qty == 0 || m_index.count(_msg.m_orderId) != 0)
            {
                _out.push_back(Event{EventType::Reject, _msg.m_orderId, 0, 0, 0});
                return;
            }

            _out.push_back(Event{EventType::Ack, _msg.m_orderId, 0, _msg.m_price, _msg.m_qty});

            Price limit = _msg.m_price;
            if (_isMarket)
            {
                limit = (_msg.m_side == Side::Buy) ? std::numeric_limits<Price>::max() : std::numeric_limits<Price>::min();
            }

            const Qty remaining = (_msg.m_side == Side::Buy) ? match(m_asks, _msg.m_side, limit, _msg.m_qty, _msg.m_orderId, _out) : match(m_bids, _msg.m_side, limit, _msg.m_qty, _msg.m_orderId, _out);

            if (remaining > 0 && !_isMarket)
            {
                rest(_msg.m_side, _msg.m_price, _msg.m_orderId, remaining);
            }
        }

        /**
         * @brief Determines whether a resting price satisfies an aggressor's limit.
         * @param _aggressor Side of the incoming order.
         * @param _limit Limit price of the incoming order.
         * @param _resting Price of the resting order.
         * @return True if the incoming order can trade at the resting price.
         */
        static bool crosses(Side _aggressor, Price _limit, Price _resting)
        {
            return _aggressor == Side::Buy ? _resting <= _limit : _resting >= _limit;
        }

        /**
         * @brief Matches an incoming order against eligible orders in a book.
         * @tparam Map Ordered price map containing the resting side.
         * @param _book Book to match against; filled orders and empty levels are removed.
         * @param _aggressor Side of the incoming order.
         * @param _limit Limit price used to determine eligible levels.
         * @param _qty Incoming quantity to match.
         * @param _aggressorId ID recorded as the aggressor in generated trade events.
         * @param _out Receives trade events for completed fills.
         * @return Quantity left unfilled.
         */
        template <class Map>
        Qty match(Map &_book, Side _aggressor, Price _limit, Qty _qty, OrderId _aggressorId, std::vector<Event> &_out)
        {
            while (_qty > 0 && !_book.empty()) // moves to next level only when the current one is empty
            {
                auto levelIt = _book.begin();
                const Price px = levelIt->first;

                if (!crosses(_aggressor, _limit, px))
                {
                    break;
                }

                Level &q = levelIt->second;

                while (_qty > 0 && !q.empty()) // drains one price level in time order
                {
                    Resting &r = q.front();
                    const Qty fill = std::min(_qty, r.m_qty);
                    _qty -= fill;
                    r.m_qty -= fill;
                    _out.push_back(Event{EventType::Trade, _aggressorId, r.m_orderId, px, fill});

                    if (r.m_qty == 0)
                    {
                        m_index.erase(r.m_orderId);
                        q.pop_front();
                    }
                }

                if (q.empty())
                {
                    _book.erase(levelIt);
                }
            }

            return _qty;
        }

        /**
         * @brief Adds an unfilled order quantity to its price-time queue and index.
         * @param _side Side on which the order rests.
         * @param _px Price level at which the order rests.
         * @param _id Order ID to store.
         * @param _qty Remaining order quantity.
         */
        void rest(Side _side, Price _px, OrderId _id, Qty _qty)
        {
            if (_side == Side::Buy)
            {
                m_bids[_px].push_back(Resting{_id, _qty});
            }
            else
            {
                m_asks[_px].push_back(Resting{_id, _qty});
            }

            m_index[_id] = Locator{_side, _px};
        }

        /**
         * @brief Removes an order from a specified price level.
         * @tparam Map Ordered price map containing the candidate level.
         * @param _book Book from which to remove the order.
         * @param _px Price level to search.
         * @param _id ID of the order to remove.
         * @return True if the order was found and removed; otherwise false.
         */
        template <class Map>
        bool eraseFrom(Map &_book, Price _px, OrderId _id)
        {
            auto lvl = _book.find(_px);

            if (lvl == _book.end())
            {
                return false;
            }

            Level &q = lvl->second;
            auto it = std::find_if(q.begin(), q.end(), [&](const Resting &_r)
                                   { return _r.m_orderId == _id; });

            if (it == q.end())
            {
                return false;
            }

            q.erase(it);

            if (q.empty())
            {
                _book.erase(lvl);
            }

            return true;
        }

        /**
         * @brief Cancels a resting order and emits a cancel acknowledgment or rejection.
         * @param _id ID of the order to cancel.
         * @param _out Receives the resulting cancel acknowledgment or rejection.
         */
        void onCancel(OrderId _id, std::vector<Event> &_out)
        {
            auto it = m_index.find(_id);

            if (it == m_index.end())
            {
                _out.push_back(Event{EventType::Reject, _id, 0, 0, 0});
                return;
            }

            const Locator loc = it->second;
            const bool ok = (loc.m_side == Side::Buy) ? eraseFrom(m_bids, loc.m_price, _id) : eraseFrom(m_asks, loc.m_price, _id);

            assert(ok);
            (void)ok; // Silences the unused warning when asserts are compiled out
            m_index.erase(it);
            _out.push_back(Event{EventType::CancelAck, _id, 0, 0, 0});
        }

        /**
         * @brief Calculates the total quantity stored at a price level.
         * @tparam Map Ordered price map to query.
         * @param _book Book containing the price level.
         * @param _px Price level to query.
         * @return Total remaining quantity at the level, or zero if it is absent.
         */
        template <class Map>
        static Qty levelQty(const Map &_book, Price _px)
        {
            auto it = _book.find(_px);

            if (it == _book.end())
            {
                return 0;
            }

            Qty total = 0;

            for (const Resting &r : it->second)
            {
                total += r.m_qty;
            }

            return total;
        }

        /**
         * @brief Checks resting orders on one side and returns their count.
         *
         * Asserts that levels are nonempty, quantities are positive, and each
         * order has a matching index entry with the expected side and price.
         * @tparam Map Ordered price map to inspect.
         * @param _book Book side to inspect.
         * @param _side Side expected for every order in the book.
         * @return Number of resting orders inspected.
         */
        template <class Map>
        std::size_t checkSide(const Map &_book, Side _side) const
        {
            std::size_t count = 0;

            for (const auto &[px, q] : _book)
            {
                assert(!q.empty());

                for (const Resting &r : q)
                {
                    assert(r.m_qty > 0);
                    auto it = m_index.find(r.m_orderId);
                    assert(it != m_index.end());
                    assert(it->second.m_side == _side && it->second.m_price == px);
                    (void)_side;
                    (void)it;
                    ++count;
                }
            }

            return count;
        }
    };

}

#endif /* AD113F7B_E5B0_4EAD_A19F_570898FA5891 */
