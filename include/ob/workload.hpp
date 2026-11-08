#ifndef F67AB0FE_2021_4798_A335_01633D14A977
#define F67AB0FE_2021_4798_A335_01633D14A977

#include <algorithm>
#include <random>
#include <vector>
#include <ob/events.hpp>
#include <ob/reference_book.hpp>

namespace ob
{
    struct WorkloadConfig
    {
        uint64_t m_seed{42};
        std::size_t m_count{1000000};
        Price m_mid{10000};     // Reference price in ticks
        double m_pCancel{0.45}; // Cancel a live order
        double m_pPassive{0.5}; // New Limit that resets
        double m_geoP{0.3};     // Higher = activity hugs the touch more tightly
        Price m_maxOffset{50};
        Qty m_maxQty{100};
    };

    /**
     * @brief Generates a deterministic sequence of limit orders and cancellations.
     * @param _cfg Seed and parameters controlling workload generation.
     * @return Generated messages in processing order.
     */
    inline std::vector<Message> generateWorkload(const WorkloadConfig &_cfg)
    {
        std::mt19937_64 rng(_cfg.m_seed); // Fixed seed to make sure test sees the identical message stream
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        std::geometric_distribution<int> geo(_cfg.m_geoP);
        std::uniform_int_distribution<Qty> qtyDist(1, _cfg.m_maxQty);
        std::bernoulli_distribution coin(0.5);

        ReferenceBook sim; // Simulator
        std::vector<Event> scratch;
        std::vector<Message> msgs;
        msgs.reserve(_cfg.m_count);
        std::vector<OrderId> live;
        OrderId nextId = 1;

        /**
         * @brief Draws a positive price offset bounded by the configured maximum.
         * @return Generated price offset.
         */
        auto offset = [&]
        { return std::min<Price>(1 + geo(rng), _cfg.m_maxOffset); };

        while (msgs.size() < _cfg.m_count)
        {
            const double r = unit(rng);
            Message m{};

            if (r < _cfg.m_pCancel)
            {
                bool made = false;

                while (!live.empty())
                {
                    const std::size_t i = rng() % live.size();
                    const OrderId id = live[i];

                    live[i] = live.back();
                    live.pop_back(); // O(1) removal

                    if (sim.contains(id)) // Skip ones that already filled
                    {
                        m = Message{MsgType::Cancel, id, Side::Buy, 0, 0};
                        made = true;
                        break;
                    }
                }

                if (!made) // Nothing to cancel yet, redraw
                {
                    continue;
                }
            }
            else
            {
                const Side side = coin(rng) ? Side::Buy : Side::Sell;
                Price px;

                if (r < _cfg.m_pCancel + _cfg.m_pPassive) // Passive: Stays behind the touch
                {
                    px = (side == Side::Buy) ? _cfg.m_mid - offset() : _cfg.m_mid + offset();
                }
                else // Aggressive: Crosses the spread
                {
                    px = (side == Side::Buy) ? _cfg.m_mid + offset() : _cfg.m_mid - offset();
                }

                m = Message{MsgType::NewLimit, nextId++, side, px, qtyDist(rng)};
            }

            scratch.clear();
            sim.process(m, scratch);

            if (m.m_msgType == MsgType::NewLimit && sim.contains(m.m_orderId))
            {
                live.push_back(m.m_orderId);
            }

            msgs.push_back(m);
        }

        return msgs;
    }
}

#endif /* F67AB0FE_2021_4798_A335_01633D14A977 */
