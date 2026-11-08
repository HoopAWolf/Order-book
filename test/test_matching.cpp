#include <gtest/gtest.h>
#include <ob/engine.hpp>
#include <ob/reference_book.hpp>

using namespace ob;

/**
 * @brief Creates a limit-order message for test scenarios.
 * @param _id Order ID.
 * @param _side Order side.
 * @param _price Limit price.
 * @param _qty Order quantity.
 * @return A new limit-order message.
 */
static Message limit(OrderId _id, Side _side, Price _price, Qty _qty)
{
    return {MsgType::NewLimit, _id, _side, _price, _qty};
}

/**
 * @brief Creates a market-order message for test scenarios.
 * @param _id Order ID.
 * @param _side Order side.
 * @param _qty Order quantity.
 * @return A new market-order message.
 */
static Message market(OrderId _id, Side _side, Qty _qty)
{
    return {MsgType::NewMarket, _id, _side, 0, _qty};
}

/**
 * @brief Creates a cancellation message for test scenarios.
 * @param _id ID of the order to cancel.
 * @return A new cancellation message.
 */
static Message cancel(OrderId _id)
{
    return {MsgType::Cancel, _id, Side::Buy, 0, 0};
}

/**
 * @brief Creates an acknowledgment event for expected test output.
 * @param _id Acknowledged order ID.
 * @param _price Order price.
 * @param _qty Order quantity.
 * @return The expected acknowledgment event.
 */
static Event ack(OrderId _id, Price _price, Qty _qty)
{
    return {EventType::Ack, _id, 0, _price, _qty};
}

/**
 * @brief Creates a trade event for expected test output.
 * @param _price Execution price.
 * @param _qty Executed quantity.
 * @param _aggressorId ID of the incoming order.
 * @param _passiveId ID of the resting order.
 * @return The expected trade event.
 */
static Event trade(Price _price, Qty _qty, OrderId _aggressorId, OrderId _passiveId)
{
    return {EventType::Trade, _aggressorId, _passiveId, _price, _qty};
}

/**
 * @brief Creates a cancellation acknowledgment for expected test output.
 * @param _id ID of the cancelled order.
 * @return The expected cancellation acknowledgment event.
 */
static Event cancelAck(OrderId _id)
{
    return {EventType::CancelAck, _id, 0, 0, 0};
}

/**
 * @brief Creates a rejection event for expected test output.
 * @param _id ID of the rejected order or cancellation.
 * @return The expected rejection event.
 */
static Event reject(OrderId _id)
{
    return {EventType::Reject, _id, 0, 0, 0};
}

class BookTest : public ::testing::Test
{
protected:
    Engine<ReferenceBook> m_eng;

    /**
     * @brief Processes a sequence of messages and checks book invariants.
     * @param _msgs Messages to process in order.
     * @return Events emitted while processing the messages.
     */
    std::vector<Event> run(std::initializer_list<Message> _msgs)
    {
        std::vector<Event> out;

        for (const Message &m : _msgs)
        {
            m_eng.onMessage(m, out);
            m_eng.getBook().checkInvariants();
        }

        return out;
    }

    /**
     * @brief Provides access to the fixture's order book.
     * @return Mutable reference to the fixture's reference book.
     */
    ReferenceBook &getBook()
    {
        return m_eng.getBook();
    }
};

using Events = std::vector<Event>;

// Resting and priority
/**
 * @brief Verifies that a non-crossing limit order rests without trading.
 */
TEST_F(BookTest, WhenLimitBuyBelowBestAskWithRestingSellWillRestWithoutTrade)
{
    auto out = run({
        limit(1, Side::Sell, 101, 5),
        limit(2, Side::Buy, 100, 5),
    });
    EXPECT_EQ(out, (Events{ack(1, 101, 5), ack(2, 100, 5)}));
    EXPECT_EQ(getBook().bestBid(), 100);
    EXPECT_EQ(getBook().bestAsk(), 101);
    EXPECT_EQ(getBook().orderCount(), 2u);
}

/**
 * @brief Verifies FIFO priority between sell orders at the same price.
 */
TEST_F(BookTest, WhenBuyCrossesWithTwoSellsAtSamePriceWillFillEarlierArrivalFirst)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        limit(2, Side::Sell, 100, 5),
        limit(3, Side::Buy, 100, 5),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 100, 5),
                       ack(3, 100, 5),
                       trade(100, 5, 3, 1), // order 1 arrived first
                   }));
    EXPECT_EQ(getBook().qtyAt(Side::Sell, 100), 5); // order 2 still resting
}

/**
 * @brief Verifies that better price priority precedes arrival-time priority.
 */
TEST_F(BookTest, WhenBuyCrossesWithEarlierWorsePriceAndLaterBetterPriceWillFillBetterPriceFirst)
{
    auto out = run({
        limit(1, Side::Sell, 101, 5), // arrives first, worse price
        limit(2, Side::Sell, 100, 5), // arrives second, better price
        limit(3, Side::Buy, 101, 5),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 101, 5),
                       ack(2, 100, 5),
                       ack(3, 101, 5),
                       trade(100, 5, 3, 2), // price beats time
                   }));
    EXPECT_EQ(getBook().bestAsk(), 101);
}

// Matching
/**
 * @brief Verifies that an equal-sized crossing order completely clears both orders.
 */
TEST_F(BookTest, WhenBuyMatchesRestingSellWithEqualQtyWillFillCompletelyAndClearBook)
{
    auto out = run({
        limit(1, Side::Sell, 100, 10),
        limit(2, Side::Buy, 100, 10),
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 10), ack(2, 100, 10), trade(100, 10, 2, 1)}));
    EXPECT_FALSE(getBook().bestBid().has_value());
    EXPECT_FALSE(getBook().bestAsk().has_value());
    EXPECT_EQ(getBook().orderCount(), 0u);
}

/**
 * @brief Verifies partial fills preserve the resting order's queue position.
 */
TEST_F(BookTest, WhenSmallBuyHitsLargeRestingSellWithPartialFillWillKeepQueuePositionWithReducedQty)
{
    auto out = run({
        limit(1, Side::Sell, 100, 10),
        limit(2, Side::Sell, 100, 5),
        limit(3, Side::Buy, 100, 4), // partially fills order 1
        limit(4, Side::Buy, 100, 6), // must finish order 1 BEFORE touching order 2
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 10),
                       ack(2, 100, 5),
                       ack(3, 100, 4),
                       trade(100, 4, 3, 1),
                       ack(4, 100, 6),
                       trade(100, 6, 4, 1),
                   }));
    EXPECT_EQ(getBook().qtyAt(Side::Sell, 100), 5); // only order 2 remains
    EXPECT_EQ(getBook().orderCount(), 1u);
}

/**
 * @brief Verifies that unfilled limit-order quantity rests at the limit price.
 */
TEST_F(BookTest, WhenLargeBuyHitsSmallRestingSellWithRemainderWillRestRemainderAtLimitPrice)
{
    auto out = run({
        limit(1, Side::Sell, 100, 4),
        limit(2, Side::Buy, 102, 10),
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 4), ack(2, 102, 10), trade(100, 4, 2, 1)}));
    EXPECT_EQ(getBook().bestBid(), 102);
    EXPECT_EQ(getBook().qtyAt(Side::Buy, 102), 6);
    EXPECT_FALSE(getBook().bestAsk().has_value());
}

/**
 * @brief Verifies a limit order matches across multiple price levels.
 */
TEST_F(BookTest, WhenBuyCrossesMultipleLevelsWithLimitCoveringBothWillTradeAtEachLevelsPrice)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        limit(2, Side::Sell, 101, 5),
        limit(3, Side::Buy, 101, 8),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 101, 5),
                       ack(3, 101, 8),
                       trade(100, 5, 3, 1),
                       trade(101, 3, 3, 2),
                   }));
    EXPECT_EQ(getBook().qtyAt(Side::Sell, 101), 2);
    EXPECT_FALSE(getBook().bestBid().has_value());
}

/**
 * @brief Verifies executions use resting prices rather than the incoming limit.
 */
TEST_F(BookTest, WhenBuyLimitIsAboveRestingPriceWithFullFillWillTradeAtRestingPriceNotLimit)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        limit(2, Side::Buy, 105, 5),
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 5), ack(2, 105, 5), trade(100, 5, 2, 1)}));
}

// Market orders
/**
 * @brief Verifies a market order consumes available liquidity and discards its remainder.
 */
TEST_F(BookTest, WhenMarketBuySweepsWithInsufficientLiquidityWillDiscardRemainder)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        limit(2, Side::Sell, 101, 5),
        market(3, Side::Buy, 12),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 101, 5),
                       ack(3, 0, 12),
                       trade(100, 5, 3, 1),
                       trade(101, 5, 3, 2),
                   }));
    EXPECT_FALSE(getBook().bestAsk().has_value());
    EXPECT_FALSE(getBook().bestBid().has_value()); // the unfilled 2 did NOT rest
    EXPECT_EQ(getBook().orderCount(), 0u);
}

/**
 * @brief Verifies an unfillable market order is acknowledged without resting.
 */
TEST_F(BookTest, WhenMarketOrderArrivesWithEmptyBookWillAckOnly)
{
    auto out = run({market(1, Side::Buy, 5)});
    EXPECT_EQ(out, (Events{ack(1, 0, 5)}));
    EXPECT_EQ(getBook().orderCount(), 0u);
}

// Cancel
/**
 * @brief Verifies cancelling the queue head preserves the order of remaining orders.
 */
TEST_F(BookTest, WhenCancelHeadOfThreeOrderQueueWithLaterSellWillFillMiddleThenTail)
{
    auto out = run({
        limit(1, Side::Buy, 100, 5),
        limit(2, Side::Buy, 100, 5),
        limit(3, Side::Buy, 100, 5),
        cancel(1),
        limit(4, Side::Sell, 100, 10),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 100, 5),
                       ack(3, 100, 5),
                       cancelAck(1),
                       ack(4, 100, 10),
                       trade(100, 5, 4, 2),
                       trade(100, 5, 4, 3),
                   }));
    EXPECT_EQ(getBook().orderCount(), 0u);
}

/**
 * @brief Verifies cancelling a middle order preserves the order of its neighbors.
 */
TEST_F(BookTest, WhenCancelMiddleOfThreeOrderQueueWithLaterSellWillFillHeadThenTail)
{
    auto out = run({
        limit(1, Side::Buy, 100, 5),
        limit(2, Side::Buy, 100, 5),
        limit(3, Side::Buy, 100, 5),
        cancel(2),
        limit(4, Side::Sell, 100, 10),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 100, 5),
                       ack(3, 100, 5),
                       cancelAck(2),
                       ack(4, 100, 10),
                       trade(100, 5, 4, 1),
                       trade(100, 5, 4, 3),
                   }));
}

/**
 * @brief Verifies cancelling the queue tail leaves earlier orders in priority order.
 */
TEST_F(BookTest, WhenCancelTailOfThreeOrderQueueWithLaterSellWillFillHeadThenMiddle)
{
    auto out = run({
        limit(1, Side::Buy, 100, 5),
        limit(2, Side::Buy, 100, 5),
        limit(3, Side::Buy, 100, 5),
        cancel(3),
        limit(4, Side::Sell, 100, 10),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 100, 5),
                       ack(3, 100, 5),
                       cancelAck(3),
                       ack(4, 100, 10),
                       trade(100, 5, 4, 1),
                       trade(100, 5, 4, 2),
                   }));
}

/**
 * @brief Verifies cancelling the sole order at a level removes that level.
 */
TEST_F(BookTest, WhenCancelOnlyOrderAtLevelWithAnotherLevelPresentWillRemoveLevel)
{
    auto out = run({
        limit(1, Side::Buy, 100, 5),
        limit(2, Side::Buy, 99, 5),
        cancel(1),
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 5), ack(2, 99, 5), cancelAck(1)}));
    EXPECT_EQ(getBook().bestBid(), 99); // level 100 is gone
    EXPECT_EQ(getBook().qtyAt(Side::Buy, 100), 0u);
}

/**
 * @brief Verifies cancelling an unknown order ID is rejected.
 */
TEST_F(BookTest, WhenCancelUnknownIdWithEmptyBookWillReject)
{
    auto out = run({cancel(42)});
    EXPECT_EQ(out, (Events{reject(42)}));
}

/**
 * @brief Verifies cancelling an already-filled order is rejected.
 */
TEST_F(BookTest, WhenCancelOrderThatAlreadyFilledWithNoRestingQtyWillReject)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        limit(2, Side::Buy, 100, 5), // fully fills order 1
        cancel(1),
    });
    EXPECT_EQ(out, (Events{
                       ack(1, 100, 5),
                       ack(2, 100, 5),
                       trade(100, 5, 2, 1),
                       reject(1),
                   }));
}

/**
 * @brief Verifies a cancelled order is no longer available for matching.
 */
TEST_F(BookTest, WhenCancelledSellWithLaterCrossingBuyWillNotMatchAndBuyWillRest)
{
    auto out = run({
        limit(1, Side::Sell, 100, 5),
        cancel(1),
        limit(2, Side::Buy, 100, 5),
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 5), cancelAck(1), ack(2, 100, 5)}));
    EXPECT_EQ(getBook().bestBid(), 100);
    EXPECT_FALSE(getBook().bestAsk().has_value());
}

// VALIDATION
/**
 * @brief Verifies a new order reusing a resting order ID is rejected unchanged.
 */
TEST_F(BookTest, WhenNewOrderReusesLiveIdWithRestingOrderWillRejectAndLeaveBookUnchanged)
{
    auto out = run({
        limit(1, Side::Buy, 100, 5),
        limit(1, Side::Buy, 101, 7), // duplicate ID
    });
    EXPECT_EQ(out, (Events{ack(1, 100, 5), reject(1)}));
    EXPECT_EQ(getBook().bestBid(), 100); // the second order changed nothing
    EXPECT_EQ(getBook().qtyAt(Side::Buy, 100), 5u);
    EXPECT_EQ(getBook().orderCount(), 1u);
}

/**
 * @brief Verifies a new limit order with zero quantity is rejected.
 */
TEST_F(BookTest, WhenNewLimitHasZeroQtyWithEmptyBookWillRejectAndRestNothing)
{
    auto out = run({limit(1, Side::Buy, 100, 0)});
    EXPECT_EQ(out, (Events{reject(1)}));
    EXPECT_EQ(getBook().orderCount(), 0u);
}