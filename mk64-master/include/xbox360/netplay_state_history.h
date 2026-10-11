#ifndef MK64_NETPLAY_STATE_HISTORY_H
#define MK64_NETPLAY_STATE_HISTORY_H
#include "netplay_protocol.h"

namespace mknet {
/* STATE_SYNC uses the same frame acknowledgement as Stream4. A guest sends
 * its hash only after applying that frame's snapshot. Keep immutable bytes
 * so an old snapshot can be replayed after the host enters the race. */
struct CrossStateHistory {
    enum { CAPACITY = 32 };
    struct Slot { bool present; uint32_t frame; uint8_t data[CROSS_STATE_BYTES]; };
    Slot slots[CAPACITY];
    bool present;
    uint32_t latest;
    uint32_t replayNext[MAX_PLAYERS];

    void reset() { memset(this, 0, sizeof(*this)); }
    const uint8_t *find(uint32_t frame) const {
        const Slot &slot = slots[frame % CAPACITY];
        return slot.present && slot.frame == frame ? slot.data : 0;
    }
    void save(uint32_t frame, const uint8_t *data) {
        if (find(frame)) return;
        Slot &slot = slots[frame % CAPACITY];
        slot.frame = frame;
        memcpy(slot.data, data, CROSS_STATE_BYTES);
        slot.present = true;
        latest = frame;
        present = true;
    }
    unsigned replay_frames(uint32_t peerFrame, uint32_t out[3], unsigned peerSlot = 1) {
        if (!present || peerFrame > latest || peerSlot >= MAX_PLAYERS) return 0;
        unsigned count = 0;
        /* frame 0 needs replay even before the guest's first hash arrives.
         * Otherwise peerFrame+1 is the next possibly missing snapshot. */
        if (find(peerFrame)) out[count++] = peerFrame;
        /* Input acknowledgements can also be lost. Rotate through the entire
         * unacknowledged interval, not just peerFrame+1, or a guest already
         * beyond that frame could wait forever for an intermediate snapshot. */
        uint32_t first = peerFrame + 1;
        if (latest >= CAPACITY && first <= latest - CAPACITY)
            first = latest - CAPACITY + 1;
        if (first < latest) {
            uint32_t &next = replayNext[peerSlot];
            if (next < first || next >= latest) next = first;
            if (find(next)) out[count++] = next;
            ++next;
        }
        if ((!count || out[count - 1] != latest) && find(latest)) out[count++] = latest;
        return count;
    }
};
}
#endif
