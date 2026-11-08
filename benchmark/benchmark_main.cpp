#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
#include <ob/engine.hpp>
#include <ob/reference_book.hpp>
#include <ob/timing.hpp>
#include <ob/workload.hpp>

using namespace ob;

enum Cat : uint8_t
{
    kPassive,
    kAggressive,
    kCancel,
    kNumCats
};
static const char *kCatName[] = {"passive insert", "aggressive", "cancel"};

static volatile uint64_t g_sink; // Consume results so the compiler can't delete the work

/**
 * @brief Attempts to bind the process to a CPU core.
 * @param _core CPU core index to use.
 *
 * On unsupported systems or when binding fails, a warning is printed and
 * execution continues without affinity.
 */
static void pinToCore(int _core)
{
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(_core, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0)
        std::fprintf(stderr, "warning: could not pin to core %d\n", _core);
#else
    (void)_core;
    std::fprintf(stderr, "warning: core pinning not supported on this OS\n");
#endif
}

struct Samples
{
    std::vector<uint64_t> m_ticks;   // Per-message latency
    std::vector<uint16_t> m_nevents; // Events produced (used to classify afterwards)
};

/**
 * @brief Measures the per-message latency and event count for a workload.
 * @tparam BookT Order-book implementation to benchmark.
 * @param _msgs Messages to process in order.
 * @return Per-message timing samples and emitted-event counts.
 */
template <class BookT>
Samples runLatency(const std::vector<Message> &_msgs)
{
    Samples s;
    s.m_ticks.resize(_msgs.size()); // Preallocated: nothing allocates in the loop
    s.m_nevents.resize(_msgs.size());
    Engine<BookT> eng;
    std::vector<Event> out;
    out.reserve(1024);
    uint64_t sink = 0;

    for (std::size_t i = 0; i < _msgs.size(); ++i)
    {
        out.clear(); // Before the timer: clear() is not what we measure
        const uint64_t t0 = ticksNow();
        eng.onMessage(_msgs[i], out);
        const uint64_t t1 = ticksNow();
        s.m_ticks[i] = t1 - t0;
        s.m_nevents[i] = static_cast<uint16_t>(out.size());
        sink += out.size();
    }

    g_sink = sink;
    return s;
}

/**
 * @brief Measures aggregate message-processing throughput for a workload.
 * @tparam BookT Order-book implementation to benchmark.
 * @param _msgs Messages to process in order.
 * @param _tpn Estimated timer ticks per nanosecond.
 * @return Throughput in messages per second.
 */
template <class BookT>
double runThroughput(const std::vector<Message> &_msgs, double _tpn)
{
    Engine<BookT> eng;
    std::vector<Event> out;
    out.reserve(1024);
    uint64_t sink = 0;
    const uint64_t s = ticksNow();

    for (const Message &m : _msgs)
    {
        out.clear();
        eng.onMessage(m, out);
        sink += out.size();
    }

    const uint64_t e = ticksNow();
    g_sink = sink;
    return _msgs.size() / ((e - s) / _tpn * 1e-9); // Messages per second
}

/**
 * @brief Selects an approximate percentile from sorted samples.
 * @param _sorted Samples sorted in ascending order.
 * @param _p Percentile to select, from 0 to 100.
 * @return Sample at the requested percentile, or zero for an empty vector.
 */
static uint64_t percentile(const std::vector<uint64_t> &_sorted, double _p)
{
    if (_sorted.empty())
    {
        return 0;
    }

    const std::size_t idx = std::min(_sorted.size() - 1,
                                     static_cast<std::size_t>(_p / 100.0 * _sorted.size()));
    return _sorted[idx];
}

/**
 * @brief Classifies a message as a cancellation, aggressive order, or passive insert.
 * @param _msg Message being classified.
 * @param _nevents Number of events produced for the message.
 * @return Category used to group latency measurements.
 */
static Cat classify(const Message &_msg, uint16_t _nevents)
{
    if (_msg.m_msgType == MsgType::Cancel)
        return kCancel;
    return _nevents > 1 ? kAggressive : kPassive;
}

/**
 * @brief Generates a workload, benchmarks it, and prints latency and throughput.
 * @param _argc Number of command-line arguments.
 * @param _argv Arguments: message count, seed, CPU core, and optional CSV output path.
 * @return Zero on success, or one if the optional CSV file cannot be opened.
 */
int main(int _argc, char **_argv)
{
    WorkloadConfig cfg;
    if (_argc > 1)
        cfg.m_count = std::strtoull(_argv[1], nullptr, 10);
    if (_argc > 2)
        cfg.m_seed = std::strtoull(_argv[2], nullptr, 10);
    const int core = (_argc > 3) ? std::atoi(_argv[3]) : 2;
    pinToCore(core);

    std::printf("generating %zu messages (seed %llu)...\n",
                cfg.m_count, static_cast<unsigned long long>(cfg.m_seed));
    const std::vector<Message> msgs = generateWorkload(cfg);

    const double tpn = ticksPerNs();
    const uint64_t overhead = timerOverheadTicks();
    std::printf("ticks/ns = %.3f   timer overhead = %llu ticks (%.1f ns)\n",
                tpn, static_cast<unsigned long long>(overhead), overhead / tpn);

    runLatency<ReferenceBook>(msgs);                   // warm-up pass: discarded
    const Samples s = runLatency<ReferenceBook>(msgs); // measured pass: fresh engine

    // Split into categories (done after timing, so it costs the measurement nothing)
    std::vector<uint64_t> by_cat[kNumCats];
    std::vector<uint64_t> all;
    all.reserve(msgs.size());
    for (std::size_t i = 0; i < msgs.size(); ++i)
    {
        const uint64_t net = s.m_ticks[i] > overhead ? s.m_ticks[i] - overhead : 0;
        const Cat c = classify(msgs[i], s.m_nevents[i]);
        by_cat[c].push_back(net);
        all.push_back(net);
    }

    /**
     * @brief Prints summary latency percentiles for one sample category.
     * @param name Label for the category.
     * @param v Latency samples; sorted in place before reporting.
     */
    auto report = [&](const char *name, std::vector<uint64_t> &v)
    {
        std::sort(v.begin(), v.end());
        /**
         * @brief Converts a selected sample percentile to nanoseconds.
         * @param p Percentile to select.
         * @return Selected latency in nanoseconds.
         */
        auto ns = [&](double p)
        { return percentile(v, p) / tpn; };
        std::printf("%-15s n=%-8zu p50=%7.0f  p90=%7.0f  p99=%8.0f  p99.9=%9.0f  max=%10.0f  (ns)\n",
                    name, v.size(), ns(50), ns(90), ns(99), ns(99.9),
                    v.empty() ? 0.0 : v.back() / tpn);
    };

    std::printf("\n");
    report("ALL", all);
    for (int c = 0; c < kNumCats; ++c)
    {
        report(kCatName[c], by_cat[c]);
    }

    std::printf("\nthroughput: %.2f M msgs/s (separate pass, no per-message timing)\n",
                runThroughput<ReferenceBook>(msgs, tpn) / 1e6);

    if (_argc > 4)
    {
        FILE *f = std::fopen(_argv[4], "w");

        if (!f)
        {
            std::fprintf(stderr, "could not open %s\n", _argv[4]);
            return 1;
        }

        std::fprintf(f, "category,ns\n");
        for (std::size_t i = 0; i < msgs.size(); ++i)
        {
            const uint64_t net = s.m_ticks[i] > overhead ? s.m_ticks[i] - overhead : 0;
            const Cat c = classify(msgs[i], s.m_nevents[i]);
            std::fprintf(f, "%s,%.1f\n", kCatName[c], net / tpn);
        }

        std::fclose(f);
    }
    return 0;
}