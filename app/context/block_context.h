#pragma once

#include "warp_context.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Emulator
{

class GlobalContext;

// Reusable cyclic barrier for intra-block warp synchronization.
// Handles early-exiting warps: call warp_done() when a warp finishes so that
// other warps blocked in sync() are not held waiting for it.
class BlockBarrier
{
    int total_active_;
    int arrived_;
    int generation_;
    std::mutex mutex_;
    std::condition_variable cv_;

  public:
    explicit BlockBarrier(int n) : total_active_(n), arrived_(0), generation_(0) {}

    void sync()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const int gen = generation_;
        if (++arrived_ >= total_active_)
        {
            arrived_ = 0;
            ++generation_;
            cv_.notify_all();
        }
        else
        {
            cv_.wait(lock, [this, gen] { return generation_ != gen; });
        }
    }

    void warp_done()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        --total_active_;
        if (total_active_ > 0 && arrived_ >= total_active_)
        {
            arrived_ = 0;
            ++generation_;
            cv_.notify_all();
        }
    }
};

class BlockContext : public std::enable_shared_from_this<BlockContext>
{
  private:
    std::weak_ptr<GlobalContext> global_context_;

    std::vector<std::shared_ptr<WarpContext>> warps_;
    std::vector<uint8_t> shared_memory_;

    std::unordered_map<std::string, void*> shared_symbols_;
    size_t shared_offset_ = 0;
    mutable std::mutex shared_mutex_;

    std::unique_ptr<BlockBarrier> block_barrier_;

  public:
    BlockContext() = default;

    void Init(const std::shared_ptr<GlobalContext>& global_context,
              const dim3& gridDim,
              const dim3& gridId,
              const dim3& blockDim,
              size_t sharedMem);
    std::vector<std::shared_ptr<WarpContext>> GetWarps() const;
    uint64_t GetBasicBlockOffset(const std::string& func_name, const std::string& sym) const;
    std::string GetBasicBlockAt(const std::string& func_name, uint64_t pc) const;
    void* GetParamPtr(const std::string& name) const;
    void RegisterSharedSymbol(const std::string& name, size_t size, size_t align);
    void* GetSharedPtr(const std::string& name) const;
    void* GetSharedBase() const;

    void InitBarrier(int n);
    void SyncBarrier();
    void WarpDone();
};

} // namespace Emulator
