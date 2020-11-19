#include "tcp_connection.hh"

#include <iostream>
#include <limits>

#define DEBUG 0

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _tick_time - _last_segment_received_time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (_is_conn_close) {
        return;
    }

    if (seg.header().rst) {
        _abort_conn();
        return;
    }

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0) {
        _receiver.segment_received(seg);
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }

        _send(false);
    }

    _last_segment_received_time = _tick_time;

#if DEBUG
    cout << "_last_segment_received_time = " << _last_segment_received_time << endl;
#endif
}

bool TCPConnection::active() const { return !_is_conn_close; }

size_t TCPConnection::write(const string &data) {
    if (!active()) {
        return 0;
    }

    auto written_bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    _send(false);

    return written_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

#if DEBUG
    cout << "ms_since_last_tick: " << ms_since_last_tick << ", _tick_time: " << _tick_time << endl;
#endif

    _tick_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    _sender.fill_window();
    _send(false);
    _try_end_conn();
}

void TCPConnection::end_input_stream() {
    if (inbound_stream().eof() && !_sender.stream_in().eof()) {

#if DEBUG
    cout << "set _linger_after_streams_finish false and ";
#endif

        _linger_after_streams_finish = false;
    }

#if DEBUG
    cout << "end_input_stream" << endl;
#endif

    _sender.stream_in().end_input();
    _sender.fill_window();
    _send(false);
}

void TCPConnection::connect() { 
    _sender.fill_window();
    _send(false);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.fill_window();
            if (_sender.segments_out().empty()) {
                _sender.send_empty_segment();
            }
            _send(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send(const bool set_rst) {
    if (!active() || _sender.segments_out().empty()) {
        return;
    }

    auto seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    if (_receiver.ackno().has_value()) {

#if DEBUG
    cout << "_receiver.ackno().value(): " << _receiver.ackno().value() << endl;
#endif

        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size() > static_cast<uint64_t>(numeric_limits<uint16_t>::max())
                               ? numeric_limits<uint16_t>::max()
                               : static_cast<uint16_t>(_receiver.window_size());
    }

    if (set_rst || _sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // set rst and abort coneection
        _abort_conn();
        seg.header().rst = true;
    }

#if DEBUG
    cout << "seg ackno: " << seg.header().ackno << endl;
#endif

    _segments_out.push(seg);
#if DEBUG
    cout << "_segments_out size: " << _segments_out.size() << ", front ackno: " << _segments_out.front().header().ackno << endl;
#endif
}

void TCPConnection::_close_conn() {
    _sender.stream_in().end_input();
    _receiver.stream_out().end_input();
    _is_conn_close = true;
}

void TCPConnection::_abort_conn() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _close_conn();
}

bool TCPConnection::_is_fin_acked() {
    auto outbound = _sender.stream_in();
    return outbound.eof() && _sender.next_seqno_absolute() == outbound.bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

void TCPConnection::_try_end_conn() {
    if (!active()) {
        return;
    }

    if (!inbound_stream().eof() || !_is_fin_acked()) {
        return;
    }

    // Prereq #4
    // Option A
    if (_linger_after_streams_finish) {
        if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {

#if DEBUG
    cout << "time_since_last_segment_received: " << time_since_last_segment_received() << endl;
#endif
            _close_conn();
        }

        return;
    }

    // Option B
    _close_conn();
}
