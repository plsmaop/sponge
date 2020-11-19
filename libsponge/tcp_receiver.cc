#include "tcp_receiver.hh"
#include <iostream>

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
        header.seqno = header.seqno + 1;
        _next_ackno = header.seqno;
    }

    // outside window
    auto _checkpoint = _reassembler.stream_out().bytes_written();
    auto absolute_seqno = unwrap(_next_ackno.value(), _isn, _checkpoint);
    auto incoming_seqno = unwrap(header.seqno, _isn, _checkpoint);
    auto payload = seg.payload();
    if (incoming_seqno == 0 || absolute_seqno + window_size() < incoming_seqno) {
        return;
    }

    auto max_size = absolute_seqno + window_size() - incoming_seqno;
    _reassembler.push_substring(payload.copy().substr(0, max_size), incoming_seqno - 1, header.fin);

    for (uint64_t i = 0; i < payload.size() && i < max_size; ++i) {
        decltype(incoming_seqno) ind = incoming_seqno + i;
        received_in_window[ind % _capacity] = true;
    }

    if (header.fin && max_size >= payload.size()) {
        received_in_window[(incoming_seqno + payload.size()) % _capacity] = true;
    }

    // move window
    if (absolute_seqno >= incoming_seqno) {
        auto ind = absolute_seqno;
        while (received_in_window[ind % _capacity]) {
            received_in_window[ind % _capacity] = false;
            ++ind;
        }

        _next_ackno = wrap(ind, _isn);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _next_ackno; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
