#include <gtest/gtest.h>
#include <ob/events.hpp>

/**
 * @brief Verifies that events with identical field values compare equal.
 */
TEST(Events, WhenComparingTwoEventsWithNewlyCreatedEventsOfSameDataWillReturnOk)
{
    ob::Event a{ob::EventType::Ack, 1, 0, 100, 10};
    ob::Event b = a;

    EXPECT_TRUE(a == b);
}

/**
 * @brief Verifies that events differing in a field do not compare equal.
 */
TEST(Events, WhenComparingTwoEventsWithNewlyCreatedEventsOfDifferentDataWillReturnFailed)
{
    ob::Event a{ob::EventType::Ack, 1, 0, 100, 10};
    ob::Event b{ob::EventType::Ack, 1, 0, 50, 10};

    EXPECT_FALSE(a == b);
}