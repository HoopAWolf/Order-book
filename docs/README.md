# Order book
An order book holds resting buy orders (bids) and sell orders (asks) for one instrument. A matching engine takes incoming orders and either trades them against the opposite side or rests them in the book.

Scope for v1:
• Single instrument, single thread, in-memory only
• Order types: limit (GTC), market, cancel, and modify (cancel/replace)
• Matching rule: price-time priority (best price first; at the same price, earliest arrival first)
• Input: a stream of messages. Output: a stream of events (ack, trade, cancel-ack, reject)