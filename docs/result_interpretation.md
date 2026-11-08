## How to read the results

Each benchmark records the latency of every message, then reports order
statistics over that list, split by operation type.

| Metric | Meaning |
|---|---|
| p50 | Median: half of messages were faster. The typical case. |
| p90 / p99 | 1 in 10 / 1 in 100 messages were slower than this. |
| p99.9 | 1 in 1,000 messages were slower. Primary tail-latency metric. |
| max | The single slowest message. Often OS noise; not used for conclusions. |
| mean | Arithmetic average. More sensitive to rare slow messages than p50. |
| throughput | Messages per second from a separate pass with no per-message timing. |

**Operation categories:** *passive insert* (order rests in the book),
*aggressive* (order crosses the spread and trades), *cancel* (removes a
resting order).

**Methodology notes**
- Timing uses the CPU cycle counter, with the cost of reading it
  subtracted. Per-message timings include a small residual cost from
  serialising instructions, which affects all configurations equally.
- Timestamp resolution on the test machine is about 10 ns, so
  differences smaller than ~20 ns at the median are not reported as
  meaningful.
- Each configuration sees the identical, seeded message stream. A warm-up
  pass is discarded and a fresh engine is used for the measured pass.
- The benchmark thread is pinned to one core. See `bench-env.md` for the
  hardware, OS, compiler and flags used.

**Comparing configurations:** look at p50 for the typical case, p99 and
p99.9 for consistency, and throughput for small overall differences.
Report the workload profile (book depth, price spread) with every result,
since cache behaviour depends on it.