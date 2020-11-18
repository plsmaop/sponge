#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (_is_conn_close) {
        return;
    }

    if (seg.header().rst) {
        _close_conn();
        return;
    }

    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0) {
        _send(false);
    }
}

bool TCPConnection::active() const { return !_is_conn_close; }

size_t TCPConnection::write(const string &data) {
    if (!active()) {
        return 0;
    }

    auto written_bytes = _sender.stream_in().write(data);
    _send(false);

    return written_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _send(false);
}

void TCPConnection::end_input_stream() {
    if (!active() || !_receiver.stream_out().eof() || !(_sender.stream_in().eof() && _sender.bytes_in_flight() == 0)) {
        return;
    }

    // Prereq #4
    // Option A
    
}

void TCPConnection::connect() { _send(false); }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _send(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send(bool set_rst) {
    if (!active()) {
        return;
    }

    _sender.fill_window();
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    auto seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size() > static_cast<uint64_t>(numeric_limits<uint16_t>::max())
                               ? numeric_limits<uint16_t>::max()
                               : static_cast<uint16_t>(_receiver.window_size());
    }

    if (set_rst || _sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // set rst and abort coneection
        _close_conn();
        seg.header().rst = true;
    }

    _segments_out.push(seg);
}

void TCPConnection::_close_conn() {
    _sender.stream_in().set_error();
    _sender.stream_in().end_input();
    _receiver.stream_out().set_error();
    _receiver.stream_out().end_input();
    _is_conn_close = true;
}