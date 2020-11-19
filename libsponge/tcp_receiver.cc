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

    // move window
    if (absolute_seqno >= incoming_seqno) {
        // +1 for syn
        auto next_ackno = _reassembler.stream_out().bytes_written()+1;
        if (_reassembler.stream_out().input_ended()) {
            ++next_ackno;
        }

        _next_ackno = wrap(next_ackno, _isn);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _next_ackno; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
