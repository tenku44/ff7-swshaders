#pragma once

#include <cstddef>
#include <cstdint>

#ifndef RH_ARENA_BYTES
#define RH_ARENA_BYTES (4u * 1024u * 1024u)
#endif

static_assert((RH_ARENA_BYTES % 16u) == 0, "RH_ARENA_BYTES must be a multiple of 16");

namespace rh::arena {

// size : payload bytes (multiple of 16); bit 0 set = the block is free
// prev : payload bytes of the block before this one (0 for the first block)
struct Hdr {
    size_t size;
    size_t prev;
};
static_assert(sizeof(Hdr) == 16, "the allocator assumes a 64-bit target");

constexpr size_t kFreeBit = 1;

alignas(16) inline unsigned char g_arena[RH_ARENA_BYTES];
inline bool   g_ready = false;
inline size_t g_used  = 0;   // payload bytes currently handed out
inline size_t g_peak  = 0;   // highest g_used seen

inline size_t Payload(const Hdr* h) { return h->size & ~kFreeBit; }
inline bool   IsFree(const Hdr* h)  { return (h->size & kFreeBit) != 0; }
inline char*  ArenaEnd()            { return reinterpret_cast<char*>(g_arena) + RH_ARENA_BYTES; }
inline Hdr*   FirstBlock()          { return reinterpret_cast<Hdr*>(g_arena); }

inline Hdr* NextOf(Hdr* h) {
    char* n = reinterpret_cast<char*>(h + 1) + Payload(h);
    return n < ArenaEnd() ? reinterpret_cast<Hdr*>(n) : nullptr;
}

inline void Init() {
    Hdr* h = FirstBlock();
    h->size = (RH_ARENA_BYTES - sizeof(Hdr)) | kFreeBit;
    h->prev = 0;
    g_ready = true;
}

inline void* Alloc(size_t bytes) {
    if (!g_ready) Init();
    size_t n = (bytes + 15u) & ~static_cast<size_t>(15u);
    if (n == 0) n = 16;

    for (Hdr* h = FirstBlock(); h; h = NextOf(h)) {
        if (!IsFree(h) || Payload(h) < n) continue;

        const size_t rest = Payload(h) - n;
        if (rest >= sizeof(Hdr) + 16u) {
            // Split: [h: n bytes, in use][t: the remainder, free]
            h->size = n;
            Hdr* t = reinterpret_cast<Hdr*>(reinterpret_cast<char*>(h + 1) + n);
            t->size = (rest - sizeof(Hdr)) | kFreeBit;
            t->prev = n;
            if (Hdr* after = NextOf(t)) after->prev = Payload(t);
        } else {
            h->size = Payload(h);        // take the whole block (clears the free bit)
        }
        g_used += Payload(h);
        if (g_used > g_peak) g_peak = g_used;
        return h + 1;
    }
    return nullptr;                      // arena exhausted or too fragmented
}

inline void Free(void* p) {
    if (!p) return;
    Hdr* h = static_cast<Hdr*>(p) - 1;
    g_used -= Payload(h);
    h->size |= kFreeBit;

    // Merge with the next block.
    if (Hdr* nx = NextOf(h)) {
        if (IsFree(nx)) {
            h->size = (Payload(h) + sizeof(Hdr) + Payload(nx)) | kFreeBit;
            if (Hdr* after = NextOf(h)) after->prev = Payload(h);
        }
    }
    // Merge into the previous block.
    if (reinterpret_cast<char*>(h) != reinterpret_cast<char*>(g_arena)) {
        Hdr* pv = reinterpret_cast<Hdr*>(reinterpret_cast<char*>(h) - h->prev - sizeof(Hdr));
        if (IsFree(pv)) {
            pv->size = (Payload(pv) + sizeof(Hdr) + Payload(h)) | kFreeBit;
            if (Hdr* after = NextOf(pv)) after->prev = Payload(pv);
        }
    }
}

// Walks every block and checks the headers. Returns false if the arena is corrupted.
// Used by tests; cheap enough to call from a debug menu.
inline bool Check() {
    if (!g_ready) return true;
    size_t total = 0, prevPayload = 0;
    bool first = true, prevFree = false;
    for (Hdr* h = FirstBlock(); h; h = NextOf(h)) {
        if (h->prev != (first ? 0 : prevPayload)) return false;
        if ((Payload(h) & 15u) != 0) return false;
        if (!first && prevFree && IsFree(h)) return false;   // two free neighbours were not merged
        total += sizeof(Hdr) + Payload(h);
        prevPayload = Payload(h);
        prevFree = IsFree(h);
        first = false;
    }
    return total == RH_ARENA_BYTES;
}

inline size_t LargestFree() {
    if (!g_ready) return RH_ARENA_BYTES - sizeof(Hdr);
    size_t best = 0;
    for (Hdr* h = FirstBlock(); h; h = NextOf(h))
        if (IsFree(h) && Payload(h) > best) best = Payload(h);
    return best;
}

}  // namespace rh::arena
