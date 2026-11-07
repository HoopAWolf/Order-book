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
        void on_mesage(const Message &_m, std::vector<Event> &_out)
        {
            m_book.process(_m, _out);
        }

        BookT book()
        {
            return m_book;
        }

    private:
        BookT m_book;
    };
}

#endif /* A2ECE5E9_375D_4CB0_82AB_6A8443BB8E0C */
