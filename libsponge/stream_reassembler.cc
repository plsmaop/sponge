#include "stream_reassembler.hh"

#include "vector"

#define DEBUG 0

#if DEBUG
#include "iostream"
#endif
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassembledStr() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {

#if DEBUG
    for (const auto &[ind, str, _] : _unassembledStr) {
        std::cout << ind << ":" << str << " <- ";
    }

    std::cout << std::endl;
#endif

    uint64_t startInd = index;
    if (startInd + data.length() < _nextInd) {
        return;
    }

    if (startInd <= _nextInd && startInd + data.length() >= _nextInd) {
        auto writtenLen = _output.write(data.substr(_nextInd - startInd));
        if (writtenLen != startInd + data.length() - _nextInd) {
            // overflow
            _unassembledStr.clear();
            _nextInd += writtenLen;
            _unassembledSize = 0;
            return;
        }

#if DEBUG
        std::cout << "Writen: " << data.substr(_nextInd - startInd) << ", _nextInd become: " << _nextInd + writtenLen << std::endl;
#endif
        _nextInd += writtenLen;
        if (eof) {
            _output.end_input();
            return;
        }

        vector<decltype(_unassembledStr.begin())> dataToRemove;
        for (auto it = _unassembledStr.begin(); it != _unassembledStr.end(); ++it) {
            auto &[ind, str, isEOF] = *it;

#if DEBUG
            std::cout << "_nextInd: " << _nextInd << " curInd: " << ind << std::endl;
#endif
            if (_nextInd < ind) {
                break;
            }

            if (ind + str.length() <= _nextInd) {
                dataToRemove.emplace_back(it);
                continue;
            }

            if (ind <= _nextInd && _nextInd < ind + str.length()) {
                writtenLen = _output.write(str.substr(_nextInd - ind));
                if (writtenLen != ind + str.length() - _nextInd) {
                    // overflow
                    _unassembledStr.clear();
                    _nextInd += writtenLen;
                    _unassembledSize = 0;
                    return;
                }

#if DEBUG
                std::cout << "Writen: " << str.substr(_nextInd - ind) << ", _nextInd become: " << _nextInd + writtenLen << std::endl;
#endif

                _nextInd += writtenLen;
                if (isEOF) {
                    _output.end_input();
                    return;
                }

                dataToRemove.emplace_back(it);
            }
        }

        for (const auto &it : dataToRemove) {
            _unassembledSize -= it->str.length();
            _unassembledStr.erase(it);
        }

        return;
    }

    auto prevIt = _unassembledStr.end();
    std::vector<decltype(prevIt)> dataToRemove;
    for (auto it = _unassembledStr.begin(); it != _unassembledStr.end(); ++it) {
        auto &[ind, str, isEOF] = *it;
        if (ind <= startInd) {
            if (ind + str.length() >= startInd + data.length()) {
                return;
            }

            prevIt = it;
            continue;
        }

        if (ind > startInd && startInd + data.length() >= ind + str.length()) {
            _unassembledSize -= str.length();
            dataToRemove.emplace_back(it);
            continue;
        }

        if (prevIt == _unassembledStr.end()) {
            if (startInd + data.length() >= ind) {
                // concat
                str = data.substr(0, ind - startInd) + str;
                _unassembledSize += (ind - startInd);
                ind = startInd;
                break;
            }

            _unassembledStr.insert(it,
                                   indStrPair{
                                       startInd,
                                       data,
                                       eof,
                                   });
            _unassembledSize += data.length();

            break;
        }

        auto &[prevInd, prevStr, prevIsEOF] = *prevIt;
        auto prevStrLen = prevStr.length();
        if (prevInd + prevStrLen >= startInd && startInd + data.length() >= ind) {
            // concat prev + data + cur
            prevStr = prevStr + data.substr(prevInd + prevStrLen - startInd) +
                      str.substr(startInd + data.length() - ind);
            _unassembledSize += (ind - prevInd - prevStrLen);
            prevIsEOF = isEOF;
            _unassembledStr.erase(it);
            prevIt = _unassembledStr.end();
            break;
        }

        if (prevInd + prevStrLen >= startInd) {
            // concat prev + data
            prevStr = prevStr + data.substr(prevInd + prevStrLen - startInd);
            _unassembledSize += (data.length() - (prevInd + prevStrLen - startInd));
            prevIt = _unassembledStr.end();
            break;
        }

        if (startInd + data.length() >= ind) {
            // concat data + cur
            str = data.substr(0, ind - startInd) + str;
            _unassembledSize += (ind - startInd);
            ind = startInd;
            prevIt = _unassembledStr.end();
            break;
        }

        _unassembledStr.insert(it,
                               indStrPair{
                                   startInd,
                                   data,
                                   eof,
                               });

        _unassembledSize += data.length();
        prevIt = _unassembledStr.end();
        break;
    }

    for (const auto &it : dataToRemove) {
        _unassembledStr.erase(it);
    }

    if (_unassembledStr.size() == 0) {
        _unassembledStr.emplace_back(indStrPair{
            startInd,
            data,
            eof,
        });

        _unassembledSize += data.length();
        return;
    }

    if (prevIt != _unassembledStr.end()) {
        auto &[ind, str, _] = *prevIt;
        if (ind + str.length() >= startInd) {
            auto prevStrLen = str.length();
            str += data.substr(ind + prevStrLen - startInd);
            _unassembledSize += (data.length() - (ind + prevStrLen - startInd));
        } else {
            _unassembledStr.insert(++prevIt,
                                   indStrPair{
                                       startInd,
                                       data,
                                       eof,
                                   });
            _unassembledSize += data.length();
        }
    }

    if (_unassembledSize <= _capacity - _output.buffer_size()) {
        return;
    }

    auto it = _unassembledStr.rbegin();
    std::vector<decltype(it)> dataToRemoveForOverCapacity;
    auto remainSizeForUnassembled = _capacity - _output.buffer_size();
    for (; it != _unassembledStr.rend(); it++) {
        auto &[ind, str, _] = *it;
        if (_unassembledSize - str.length() <= remainSizeForUnassembled) {
            auto szShouldRemove = _unassembledSize - remainSizeForUnassembled;
            str = str.substr(0, str.length() - szShouldRemove);
            _unassembledSize = remainSizeForUnassembled;
        }

        dataToRemoveForOverCapacity.emplace_back(it);
        _unassembledSize -= str.length();
    }

    for (const auto &rit : dataToRemoveForOverCapacity) {
        // erase reverse iterator
        _unassembledStr.erase(std::next(rit).base());
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembledSize; }

bool StreamReassembler::empty() const { return _unassembledSize == 0; }
