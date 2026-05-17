#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

struct StereoFrame {
  float left = 0.0f;
  float right = 0.0f;
};

class SpscFrameRingBuffer {
public:
  void resize(std::size_t frameCapacity) {
    frames.resize(frameCapacity + 1);
    clear();
  }

  void clear() {
    for (auto &frame : frames) {
      frame = {};
    }
    readIndex.store(0, std::memory_order_release);
    writeIndex.store(0, std::memory_order_release);
    overrunFrames.store(0, std::memory_order_release);
    underrunFrames.store(0, std::memory_order_release);
  }

  bool push(const StereoFrame &frame) {
    // SPSC ownership: producer/capture is the only writer of writeIndex.
    // It may read readIndex to detect fullness, but it must never advance it.
    const std::size_t write = writeIndex.load(std::memory_order_relaxed);
    const std::size_t nextWrite = increment(write);
    if (nextWrite == readIndex.load(std::memory_order_acquire)) {
      overrunFrames.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    frames[write] = frame;
    writeIndex.store(nextWrite, std::memory_order_release);
    return true;
  }

  StereoFrame pop() {
    // SPSC ownership: consumer/playback is the only writer of readIndex.
    // It may read writeIndex to detect emptiness, but it must never advance it.
    const std::size_t read = readIndex.load(std::memory_order_relaxed);
    if (read == writeIndex.load(std::memory_order_acquire)) {
      underrunFrames.fetch_add(1, std::memory_order_relaxed);
      return {};
    }

    const StereoFrame frame = frames[read];
    readIndex.store(increment(read), std::memory_order_release);
    return frame;
  }

  std::size_t overrunCount() const {
    return overrunFrames.load(std::memory_order_acquire);
  }

  std::size_t underrunCount() const {
    return underrunFrames.load(std::memory_order_acquire);
  }

private:
  std::size_t increment(std::size_t index) const {
    ++index;
    return index == frames.size() ? 0 : index;
  }

private:
  std::vector<StereoFrame> frames;
  std::atomic_size_t readIndex = 0;
  std::atomic_size_t writeIndex = 0;
  std::atomic_size_t overrunFrames = 0;
  std::atomic_size_t underrunFrames = 0;
};
