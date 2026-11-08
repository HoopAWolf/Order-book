|                Situation               |                        Events, in order                        |
|:--------------------------------------:|:--------------------------------------------------------------:|
| New limit order accepted               | <mark>Ack</mark>, then <mark>Trade</mark> as it matches       |
| New market order                       | <mark>Ack</mark>, then <mark>Trade</mark>; any unfilled remainder is silently discarded |
| Order rests                            | No extra event (the earlier <mark>Ack</mark> covers it)                     |
| Cancel of a live order                 | <mark>CancelAck</mark>                                                      |
| Cancel of an unknown ID                | <mark>Reject</mark>                                                           |
| New order with qty 0 or a duplicate ID | <mark>Reject</mark>  , no book change                                         |