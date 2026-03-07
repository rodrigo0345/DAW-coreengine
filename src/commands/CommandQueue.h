//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_COMMANDQUEUE_H
#define DAWCOREENGINE_COMMANDQUEUE_H
#include <memory>

#include "Command.h"
#include <atomic>
#include <vector>
#include <optional>

namespace coreengine {
    inline size_t DEFAULT_QUEUE_CAPACITY = 1024;

    class CommandQueue {
    public:
        // Adjust size based on how many simultaneous commands you expect (must be power of 2 for speed)
        explicit CommandQueue(const size_t capacity = DEFAULT_QUEUE_CAPACITY)
            : capacity(capacity), buffer(capacity) {}

        // Called by UI Thread
        bool push(const Command& cmd) {
            size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
            size_t nextWrite = (currentWrite + 1) % capacity;

            if (nextWrite == readIndex.load(std::memory_order_acquire)) {
                return false; // Queue full
            }

            buffer[currentWrite] = cmd;
            writeIndex.store(nextWrite, std::memory_order_release);
            return true;
        }

        // Called by Audio Thread (RenderLoop)
        std::optional<Command> pop() {
            const size_t currentRead = readIndex.load(std::memory_order_relaxed);
            if (currentRead == writeIndex.load(std::memory_order_acquire)) {
                return std::nullopt; // Queue empty
            }

            Command cmd = buffer[currentRead];
            readIndex.store((currentRead + 1) % capacity, std::memory_order_release);
            return cmd;
        }

    private:
        size_t capacity;
        std::vector<coreengine::Command> buffer;
        std::atomic<size_t> writeIndex{0};
        std::atomic<size_t> readIndex{0};
    };

}

#endif //DAWCOREENGINE_COMMANDQUEUE_H