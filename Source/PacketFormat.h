#pragma once

#include <cstdint>
#include <cstddef>

// Wire format for NetStream UDP audio packets.
// Each packet is a fixed header followed by interleaved 32-bit float PCM samples.
namespace NetStream
{
    constexpr uint32_t kMagic = 0x3154534eu; // 'NST1' little-endian

    // Kept small enough that header + payload stays under a standard 1500-byte
    // Ethernet MTU, avoiding IP fragmentation on real networks.
    constexpr int kMaxChannels = 2;
    constexpr int kMaxSamplesPerPacket = 128;

#pragma pack(push, 1)
    struct PacketHeader
    {
        uint32_t magic;
        uint32_t sequence;
        uint32_t sampleRate;
        uint32_t numChannels;
        uint32_t numSamples; // per channel
    };
#pragma pack(pop)

    constexpr size_t kHeaderSize = sizeof (PacketHeader);
    constexpr size_t kMaxPacketSize = kHeaderSize + (size_t) kMaxChannels * (size_t) kMaxSamplesPerPacket * sizeof (float);

    // Small heartbeat/level packet, sent in the opposite direction to audio so
    // each end can show what its peer is doing without needing to look at the
    // peer's own window. Distinguished from PacketHeader purely by magic value
    // (checked first, before assuming either layout), so both types can share
    // the same socket/port.
    constexpr uint32_t kStatusMagic = 0x3253544eu; // 'NST2' little-endian

#pragma pack(push, 1)
    struct StatusPacket
    {
        uint32_t magic;       // kStatusMagic
        float peakLevel;      // sender's local linear peak amplitude (0..~1+)
        uint32_t packetsSeen; // informational running counter
    };
#pragma pack(pop)

    constexpr size_t kStatusPacketSize = sizeof (StatusPacket);
}
