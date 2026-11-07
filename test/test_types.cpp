#include <gtest/gtest.h>
#include <ob/events.hpp>

TEST(Events, Equality)
{
    ob::Event a{ob::EventType::Ack, 1, 0, 100, 10};
    ob::Event b = a;

    EXPECT_TRUE(a == b);
    b.m_qty = 5;
    EXPECT_FALSE(a == b);
}