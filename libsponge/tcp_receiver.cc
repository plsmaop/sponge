#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();
    if ((_next_ackno && header.syn) || (!_next_ackno && !header.syn) || _reassembler.stream_out().eof()) {
        return;
    }

    if (!_next_ackno) {
        _isn = header.seqno;
        received_in_window[header.seqno.raw_value() % _capacity] = false;
        header.seqno = header.seqno + 1;
        _next_ackno = header.seqno;
    }

    // outside window
    auto absolute_seqno = unwrap(_next_ackno.value(), _isn, _checkpoint);
    auto incoming_seqno = static_cast<uint64_t>(header.seqno.raw_value()) - static_cast<uint64_t>(_isn.raw_value());
    auto payload = seg.payload();
    if (absolute_seqno + window_size() < incoming_seqno) {
        return;
    }

    auto max_size = absolute_seqno + window_size() - incoming_seqno;
    _reassembler.push_substring(payload.copy().substr(0, max_size), incoming_seqno - 1, header.fin);
    _checkpoint = _reassembler.stream_out().bytes_written();

    using namespace std;
    for (uint64_t i = 0; i < payload.size() && i < max_size; ++i) {
        auto ind = static_cast<uint64_t>(header.seqno.raw_value()) + i;
        received_in_window[ind % _capacity] = true;
    }

    if (header.fin && max_size >= payload.size()) {
        received_in_window[(static_cast<uint64_t>(header.seqno.raw_value()) + payload.size()) % _capacity] = true;
    }

    // move window
    if (_next_ackno == header.seqno) {
        auto ind = static_cast<uint64_t>(_next_ackno.value().raw_value());
        auto new_acked = 0;
        while (true) {
            if (!received_in_window[ind % _capacity]) {
                break;
            }
            received_in_window[ind % _capacity] = false;
            ++ind;
            ++new_acked;
        }

        _next_ackno = _next_ackno.value() + new_acked;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _next_ackno; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
