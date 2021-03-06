#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <vector>

#define DEBUG 1

#if DEBUG
#include <iostream>
#endif

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool Timer::tick(const uint64_t ms_since_last_tick) {
    if (!_is_start) {
        return false;
    }

    _cur_tick += ms_since_last_tick;
    if (_cur_tick < _time_out) {
        return false;
    }

    _cur_tick = 0;
    _is_start = false;
    return true;
}

void Timer::set_timeout(const uint64_t timeout) {
    if (_is_start) {
        return;
    }

    _time_out = timeout;
    _is_start = true;
}

void Timer::stop() {
    _time_out = 0;
    _cur_tick = 0;
    _is_start = false;
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer() {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (_recv_zero && !_recv_zero_used) {
        _cur_window = 1;
        _recv_zero_used = true;
    }

    // treat zero window size as one
    while (_cur_window > 0 && !_is_close) {
        uint64_t win_size = static_cast<uint64_t>(_cur_window);
        TCPSegment seg;
        seg.header().seqno = next_seqno();

        if (_next_seqno == 0) {
            --win_size;
            seg.header().syn = true;
        }

        auto payload_size = win_size < _stream.buffer_size() ? win_size : _stream.buffer_size();
        payload_size = payload_size < TCPConfig::MAX_PAYLOAD_SIZE ? payload_size : TCPConfig::MAX_PAYLOAD_SIZE;
        seg.payload() = Buffer(std::move(_stream.read(payload_size)));
        win_size -= payload_size;

        if (_stream.eof() && win_size > 0) {
            seg.header().fin = true;
            _is_close = true;
        }

        if (seg.length_in_sequence_space() == 0) {
            return;
        }

        _segments_out.emplace(seg);
        _outstanding.emplace_back(seg);
        _bytes_in_flight += static_cast<uint64_t>(seg.length_in_sequence_space());
        _next_seqno += static_cast<uint64_t>(seg.length_in_sequence_space());
        _timer.set_timeout(_initial_retransmission_timeout);
        _cur_window -= static_cast<uint64_t>(seg.length_in_sequence_space());
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto checkpoint = _stream.bytes_read();
    auto abs_ackno = unwrap(ackno, _isn, checkpoint);
    if (abs_ackno > _next_seqno) {
        // Impossible ackno (beyond next seqno) is ignored
        return;
    }

    _cur_window = window_size;
    _recv_zero = false;
    if (window_size == 0) {
        _recv_zero = true;
        _recv_zero_used = false;
    }

    std::vector<decltype(_outstanding.begin())> itToErase;
    for (auto it = _outstanding.begin(), end = _outstanding.end(); it != end; ++it) {
        auto abs_seqno = unwrap(it->header().seqno, _isn, checkpoint);
        if (abs_ackno < abs_seqno) {
            break;
        }

        auto seg_size = static_cast<uint64_t>(it->length_in_sequence_space());
        if (abs_ackno >= abs_seqno + seg_size) {
            itToErase.emplace_back(it);
            _bytes_in_flight -= seg_size;
            continue;
        }

        if (abs_seqno <= abs_ackno && abs_ackno < abs_seqno + seg_size) {
            auto w_size = static_cast<uint64_t>(window_size);
            if (abs_ackno + w_size < abs_seqno + seg_size) {
                _cur_window = abs_seqno + seg_size - abs_ackno - w_size;
                continue;
            }

            _cur_window = abs_ackno + w_size - abs_seqno - seg_size;
        }

        break;
    }

    for (auto &it : itToErase) {
        _outstanding.erase(it);
    }

    // successful receipt of newdata
    if (!itToErase.empty()) {
        _timer.stop();
        if (!_outstanding.empty()) {
            _timer.set_timeout(_initial_retransmission_timeout);
        }

        _consecutive_retransmissions = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.tick(ms_since_last_tick)) {
        return;
    }

    _retransmit();
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.emplace(seg);
}

void TCPSender::_retransmit() {
#if DEBUG
    cout << "TIMEOUT, RETRANSMIT\n";
#endif

    // (a) retransmission
    auto it = _outstanding.begin();
    if (it == _outstanding.end()) {
        return;
    }

    _segments_out.emplace(*it);

    auto rto = _timer.get_timout();
    // (b)
    if (!_recv_zero) {
        ++_consecutive_retransmissions;
        rto *= 2;
    }

    // (c) reset RTO
    _timer.stop();
    _timer.set_timeout(rto);
}