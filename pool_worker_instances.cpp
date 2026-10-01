#include "pool_worker_instances.hpp"

#include <algorithm>
#include <cstddef>

namespace
{
constexpr uintptr_t POOL_WORKER_LIST_OFFSET = 0x60;
constexpr uintptr_t WORKER_POOL_LINK_OFFSET = 0x10;
constexpr uintptr_t WORKER_THREAD_ID_OFFSET = 0x28;
constexpr uintptr_t WORKER_POOL_OFFSET = 0x30;
constexpr uintptr_t WORKER_INSTANCE_OFFSET = 0x38;
constexpr SIZE_T MAX_POOL_WORKERS = 0x10000;

struct TpPoolWorkerListView
{
    BYTE Prefix00[0x48];
    SRWLOCK WorkerListLock;       // +0x48
    BYTE Unknown50[0x10];        // +0x50
    LIST_ENTRY WorkerListHead;   // +0x60
};

struct RemoteListEntry
{
    uintptr_t flink;
    uintptr_t blink;
};

static_assert(sizeof(void*) == 8,
              "TP_POOL worker-list traversal requires a 64-bit build");
static_assert(offsetof(TpPoolWorkerListView, WorkerListHead) ==
                  POOL_WORKER_LIST_OFFSET,
              "Unexpected TP_POOL worker-list offset");

bool ReadRemoteExact(HANDLE process, uintptr_t address, void* destination,
                     SIZE_T size)
{
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(process, reinterpret_cast<const void*>(address),
                             destination, size, &bytesRead) &&
           bytesRead == size;
}

bool IsAlignedRemotePointer(uintptr_t address)
{
    return address >= 0x10000 && (address & (sizeof(void*) - 1)) == 0;
}
}

std::vector<uintptr_t> FindRemoteTpCallbackInstanceSlotsFromPool(
    HANDLE process, uintptr_t poolAddress)
{
    std::vector<uintptr_t> instances;
    if (!process || !IsAlignedRemotePointer(poolAddress))
        return instances;

    const uintptr_t head = poolAddress + POOL_WORKER_LIST_OFFSET;

    RemoteListEntry originalHead{};
    if (!ReadRemoteExact(process, head, &originalHead, sizeof(originalHead)) ||
        !IsAlignedRemotePointer(originalHead.flink) ||
        !IsAlignedRemotePointer(originalHead.blink))
    {
        return instances;
    }

    uintptr_t node = originalHead.flink;
    std::vector<uintptr_t> visitedNodes;

    while (node != head && visitedNodes.size() < MAX_POOL_WORKERS)
    {
        if (!IsAlignedRemotePointer(node) ||
            std::find(visitedNodes.begin(), visitedNodes.end(), node) !=
                visitedNodes.end())
        {
            instances.clear();
            return instances;
        }

        RemoteListEntry links{};
        uintptr_t nextBlink = 0;
        uintptr_t previousFlink = 0;
        if (!ReadRemoteExact(process, node, &links, sizeof(links)) ||
            !IsAlignedRemotePointer(links.flink) ||
            !IsAlignedRemotePointer(links.blink) ||
            !ReadRemoteExact(process, links.flink + sizeof(uintptr_t),
                             &nextBlink, sizeof(nextBlink)) ||
            !ReadRemoteExact(process, links.blink, &previousFlink,
                             sizeof(previousFlink)) ||
            nextBlink != node || previousFlink != node)
        {
            instances.clear();
            return instances;
        }

        const uintptr_t workerBlock = node - WORKER_POOL_LINK_OFFSET;
        DWORD threadId = 0;
        uintptr_t storedPool = 0;
        if (!ReadRemoteExact(process, workerBlock + WORKER_THREAD_ID_OFFSET,
                             &threadId, sizeof(threadId)) ||
            !ReadRemoteExact(process, workerBlock + WORKER_POOL_OFFSET,
                             &storedPool, sizeof(storedPool)) ||
            threadId == 0 || storedPool != poolAddress)
        {
            instances.clear();
            return instances;
        }

        visitedNodes.push_back(node);
        instances.push_back(workerBlock + WORKER_INSTANCE_OFFSET);
        node = links.flink;
    }

    if (node != head)
    {
        instances.clear();
        return instances;
    }


    RemoteListEntry finalHead{};
    if (!ReadRemoteExact(process, head, &finalHead, sizeof(finalHead)) ||
        finalHead.flink != originalHead.flink ||
        finalHead.blink != originalHead.blink)
    {
        instances.clear();
        return instances;
    }

    return instances;
}
