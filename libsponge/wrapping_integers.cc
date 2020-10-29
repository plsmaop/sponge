#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    auto isn_raw = static_cast<uint64_t>(isn.raw_value());
    uint32_t wrap_num = (n + isn_raw) % (1ul << 32);
    return WrappingInt32{wrap_num};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    auto raw_n = static_cast<uint64_t>(n.raw_value());
    auto raw_isn = static_cast<uint64_t>(isn.raw_value());
    auto mod = 1ul << 32;
    if (raw_n < raw_isn) {
        raw_n += mod;
    }

    auto mutiply = checkpoint / mod;
    auto remainder = checkpoint % mod;

    auto gap = raw_n - raw_isn;
    if (mutiply == 0 && remainder == 0) {
        return gap;
    }

    if (gap > remainder) {
        if (gap - remainder > remainder + mod - gap) {
            mutiply--;
        }
    }

    if (gap < remainder) {
        if (remainder - gap > gap + mod - remainder) {
            mutiply++;
        }
    }

    return gap + mutiply * mod;
}
