#include "block_context.h"
#include "instructions.h"
#include "warp_context.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace Emulator;
using namespace Emulator::Ptx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Launch n threads that each run fn(thread_index), join all, and return the
// wall-clock duration. Useful for detecting deadlocks in tests.
template <typename Fn>
static std::chrono::milliseconds run_threads(int n, Fn fn)
{
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        threads.emplace_back([fn, i] { fn(i); });
    }
    for (auto& t : threads)
    {
        t.join();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
}

// Create a WarpContext with a BlockContext whose barrier is initialized to n_warps.
// The BlockContext::Init() chain is bypassed intentionally — only the barrier and
// the warp->block link are needed for syncBarrier() tests.
static std::shared_ptr<WarpContext> makeWarpWithBarrier(const std::shared_ptr<BlockContext>& block, uint32_t tid = 0)
{
    auto wc = std::make_shared<WarpContext>();
    dim3 gridDim{1, 1, 1};
    dim3 gridId{0, 0, 0};
    dim3 blockDim{32, 1, 1};
    std::vector<dim3> tids{{tid, 0, 0}};
    wc->Init(block, gridDim, gridId, blockDim, tids);
    wc->pc = 0;
    return wc;
}

// ============================================================================
// BlockBarrier — basic synchronization
// ============================================================================

// A 1-warp barrier must complete without blocking.
TEST(BlockBarrier, SingleWarpCompletesImmediately)
{
    BlockBarrier barrier(1);
    EXPECT_NO_THROW(barrier.sync());
    EXPECT_NO_THROW(barrier.sync());
    EXPECT_NO_THROW(barrier.sync());
}

// All N threads must arrive before any of them proceeds past sync().
TEST(BlockBarrier, AllWarpsSync)
{
    constexpr int N = 8;
    BlockBarrier barrier(N);
    std::atomic<int> inside{0};
    std::atomic<int> after{0};

    run_threads(N,
                [&](int)
                {
                    ++inside;
                    barrier.sync();
                    // By the time any thread reaches here, all N must have arrived.
                    EXPECT_EQ(inside.load(), N);
                    ++after;
                });

    EXPECT_EQ(after.load(), N);
}

// Barrier is reusable across multiple rounds without deadlock.
// After each sync(), every thread has incremented counter at least (r+1)*N times
// total; a fast thread may already be ahead on the next round, so we check >=.
TEST(BlockBarrier, MultipleRounds)
{
    constexpr int N = 4;
    constexpr int ROUNDS = 6;
    BlockBarrier barrier(N);
    std::atomic<int> counter{0};

    run_threads(N,
                [&](int)
                {
                    for (int r = 0; r < ROUNDS; ++r)
                    {
                        ++counter;
                        barrier.sync();
                        EXPECT_GE(counter.load(), (r + 1) * N);
                    }
                });

    EXPECT_EQ(counter.load(), N * ROUNDS);
}

// ============================================================================
// BlockBarrier — warp_done() unblocks waiting warps
// ============================================================================

// N-1 warps are blocked in sync(). The Nth warp calls warp_done() instead
// (it finished execution without hitting bar.sync). All blocked warps must
// be released.
TEST(BlockBarrier, WarpDoneUnblocksWaiters)
{
    constexpr int N = 4;
    BlockBarrier barrier(N);
    std::atomic<int> unblocked{0};

    // N-1 threads wait at the barrier.
    std::vector<std::thread> waiters;
    for (int i = 0; i < N - 1; ++i)
    {
        waiters.emplace_back(
            [&]
            {
                barrier.sync();
                ++unblocked;
            });
    }

    // Give waiters time to block, then signal done.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(unblocked.load(), 0); // none released yet

    barrier.warp_done();

    for (auto& t : waiters)
    {
        t.join();
    }
    EXPECT_EQ(unblocked.load(), N - 1);
}

// Multiple warps call warp_done() concurrently — no deadlock or crash.
TEST(BlockBarrier, ConcurrentWarpDone)
{
    constexpr int N = 8;
    BlockBarrier barrier(N);

    run_threads(N, [&](int) { barrier.warp_done(); });
    // Just checking no deadlock / crash.
}

// ============================================================================
// BlockBarrier — PDOM divergence scenario
// ============================================================================

// Simulates the real bug: PDOM causes warp 0 to call bar.sync extra times
// after all other warps have finished.
//
// Scenario (4 warps):
//   1. All 4 warps sync once (the real __syncthreads()).
//   2. Warps 1-3 call warp_done() (they have no more barriers to hit).
//   3. Warp 0 calls sync() 3 more times due to PDOM pending frames.
//      Each of those calls must complete immediately (total_active_ == 1).
TEST(BlockBarrier, PdomExtraBarriersAfterWarpsDone)
{
    constexpr int N = 4;
    BlockBarrier barrier(N);
    std::atomic<bool> warps_done{false};

    // Warps 1-3: one sync then warp_done.
    std::vector<std::thread> others;
    for (int i = 1; i < N; ++i)
    {
        others.emplace_back(
            [&]
            {
                barrier.sync(); // the one real barrier all warps share
                barrier.warp_done();
            });
    }

    // Warp 0: one real sync + three PDOM-injected extra syncs.
    barrier.sync(); // shared barrier — must wait for all 4
    // After this point warps 1-3 have called (or will call) warp_done().
    for (auto& t : others)
    {
        t.join();
    }
    warps_done = true;

    // These extra syncs happen when total_active_ == 1, so each must return
    // immediately without blocking.
    for (int extra = 0; extra < 3; ++extra)
    {
        EXPECT_NO_THROW(barrier.sync());
    }
    EXPECT_TRUE(warps_done.load());
}

// ============================================================================
// BlockBarrier — warp_done() during an active barrier round
// ============================================================================

// A warp calls warp_done() while other warps are still blocked at sync().
// The remaining waiters should be released when the last active warp either
// arrives (sync) or exits (warp_done).
TEST(BlockBarrier, WarpDoneWhileOthersWaiting)
{
    constexpr int N = 3;
    BlockBarrier barrier(N);
    std::atomic<int> released{0};

    // Warp 0 and Warp 1 block at sync.
    std::thread t0(
        [&]
        {
            barrier.sync();
            ++released;
        });
    std::thread t1(
        [&]
        {
            barrier.sync();
            ++released;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // Warp 2 exits without syncing.
    barrier.warp_done();

    t0.join();
    t1.join();
    EXPECT_EQ(released.load(), 2);
}

// ============================================================================
// barInstruction — integration with BlockBarrier through WarpContext
// ============================================================================

// bar.sync with no BlockContext must remain a no-op (regression guard).
TEST(BarInstructionBarrier, NoopWithoutBlockContext)
{
    auto wc = std::make_shared<WarpContext>();
    wc->execution_mask = 0x1;
    wc->thread_regs.resize(32);
    wc->spr_regs.resize(32);
    wc->pc = 10;

    auto instr = barInstruction::Make("bar.sync 0;");
    EXPECT_NO_THROW(instr->Execute(wc));
    EXPECT_EQ(wc->pc, 11U);
}

// bar.sync with a single-warp block must complete immediately.
TEST(BarInstructionBarrier, SingleWarpBlockCompletesImmediately)
{
    auto block = std::make_shared<BlockContext>();
    block->InitBarrier(1);

    auto wc = makeWarpWithBarrier(block);
    auto instr = barInstruction::Make("bar.sync 0;");
    EXPECT_NO_THROW(instr->Execute(wc));
    EXPECT_EQ(wc->pc, 1U);
}

// Two warps on the same block must both reach bar.sync before either proceeds.
TEST(BarInstructionBarrier, TwoWarpsBlockUntilBothArrive)
{
    auto block = std::make_shared<BlockContext>();
    block->InitBarrier(2);

    auto wc0 = makeWarpWithBarrier(block, 0);
    auto wc1 = makeWarpWithBarrier(block, 1);

    std::atomic<int> arrived{0};

    auto run = [&](std::shared_ptr<WarpContext> wc)
    {
        auto instr = barInstruction::Make("bar.sync 0;");
        ++arrived;
        instr->Execute(wc);
        // Both warps must have arrived before either exits Execute().
        EXPECT_EQ(arrived.load(), 2);
    };

    std::thread t0([&] { run(wc0); });
    std::thread t1([&] { run(wc1); });
    t0.join();
    t1.join();
}

// bar.sync called extra times after WarpDone() must not deadlock.
// Models the PDOM divergence scenario through the full instruction path.
TEST(BarInstructionBarrier, ExtraBarSyncAfterWarpDone)
{
    auto block = std::make_shared<BlockContext>();
    block->InitBarrier(2);

    auto wc0 = makeWarpWithBarrier(block, 0);
    auto wc1 = makeWarpWithBarrier(block, 1);

    auto instr = barInstruction::Make("bar.sync 0;");

    // Warp 1 syncs once then signals done.
    std::thread t1(
        [&]
        {
            instr->Execute(wc1);
            block->WarpDone();
        });

    // Warp 0 syncs once (the real barrier) then twice more (PDOM extras).
    instr->Execute(wc0);
    t1.join();

    // total_active_ == 1 now; these must complete without blocking.
    EXPECT_NO_THROW(instr->Execute(wc0));
    EXPECT_NO_THROW(instr->Execute(wc0));
}
