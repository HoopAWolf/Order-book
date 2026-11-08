#ifndef A2ECE5E9_375D_4CB0_82AB_6A8443BB8E0C
#define A2ECE5E9_375D_4CB0_82AB_6A8443BB8E0C

#include <vector>
#include <ob/events.hpp>

namespace ob
{
    template <class BookT>
    class Engine
    {
    public:
        /**
         * @brief Passes a message to the managed order book.
         * @param _m Message to process.
         * @param _out Receives the events produced while processing the message.
         */
        void onMessage(const Message &_m, std::vector<Event> &_out)
        {
            m_book.process(_m, _out);
        }

        /**
         * @brief Provides access to the managed order book.
         * @return Mutable reference to the order book instance.
         */
        BookT &getBook()
        {
            return m_book;
        }

    private:
        BookT m_book;
    };
}

#endif /* A2ECE5E9_375D_4CB0_82AB_6A8443BB8E0C */
