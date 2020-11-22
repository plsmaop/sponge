#include "byte_stream.hh"

#include <iostream>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), _buf(string(capacity, ' ')) {}

size_t ByteStream::write(const string &data) {
    auto gap = remaining_capacity();
    if (gap == 0) {
        return 0;
    }

    auto byte_written = gap > data.length() ? data.length() : gap;
    auto start = _producer_index % _capacity;
    auto end = start + byte_written > _capacity ? _capacity : start + byte_written;

    data.copy(_buf.data() + start, end - start, 0);
    if (_capacity < start + byte_written) {
        data.copy(_buf.data(), (start + byte_written) - _capacity, end - start);
    }

    _producer_index += byte_written;
    return byte_written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len > buffer_size()) {
        throw std::runtime_error("len too long");
    }

    std::string s(len, ' ');
    auto start = _consumer_index % _capacity;
    auto end = start + len > _capacity ? _capacity : start + len;
    _buf.copy(s.data(), end - start, start);
    if (start + len > _capacity) {
        _buf.copy(s.data() + (end - start), (start + len) - _capacity, 0);
    }

    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > buffer_size()) {
        throw std::runtime_error("len too long");
    }

    _consumer_index += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto s = peek_output(len);
    pop_output(len);
    return s;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _producer_index - _consumer_index; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_end && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _producer_index; }

size_t ByteStream::bytes_read() const { return _consumer_index; }

size_t ByteStream::remaining_capacity() const { return _capacity - (_producer_index - _consumer_index); }
