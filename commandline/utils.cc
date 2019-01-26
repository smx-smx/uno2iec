#include "utils.h"

#include <algorithm>
#include <cassert>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "boost/format.hpp"

void SetError(IECStatus::IECStatusCode status_code, const std::string &context,
              IECStatus *status) {
  status->status_code = status_code;
  switch (status_code) {
  case IECStatus::OK:
    status->message = "OK";
    break;
  case IECStatus::UNIMPLEMENTED:
    status->message = "Unimplemented";
    break;
  case IECStatus::CONNECTION_FAILURE:
    status->message = "Connection failure";
    break;
  case IECStatus::INVALID_ARGUMENT:
    status->message = "Invalid argument";
    break;
  case IECStatus::IEC_CONNECTION_FAILURE:
    status->message = "IEC connection failure";
    break;
  case IECStatus::DRIVE_ERROR:
    status->message = "Drive error";
    break;    
  }
  if (!context.empty()) {
    status->message = context + ": " + status->message;
  }
}

void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string &context, IECStatus *status) {
  SetError(status_code, context, status);
  status->message = status->message + ": " + strerror(errno);
}

BufferedReadWriter::BufferedReadWriter(int fd) : fd_(fd) {
  memset(buffer_, 0, sizeof(buffer_));
}

ssize_t BufferedReadWriter::FindTerminatorFrom(char term_symbol,
                                               size_t search_from,
                                               size_t search_to) {
  assert(search_from >= data_start_);
  assert(search_to <= data_end_);
  for (size_t i = search_from; i < search_to; ++i) {
    if (buffer_[i] == term_symbol) {
      return i;
    }
  }
  return -1;
}

void BufferedReadWriter::ConsumeData(size_t consume_to,
                                     size_t ignore_additional_bytes,
                                     std::string *result) {
  assert(consume_to + ignore_additional_bytes <= data_end_);
  result->clear();
  result->append(&buffer_[data_start_], consume_to - data_start_);
  data_start_ = consume_to + ignore_additional_bytes;
  if (data_start_ >= kMaxReadAhead) {
    // The first half of the buffer is now unused.
    // Move data into it, thus reusing the unused space at the
    // beginning. The regions will never overlap, so it is safe
    // to use the standard memcpy here.
    memcpy(buffer_, &buffer_[data_start_], data_end_ - data_start_);
    data_end_ -= data_start_;
    data_start_ = 0;
  }
}

bool BufferedReadWriter::ReadTerminatedString(char term_symbol,
                                              size_t max_length,
                                              std::string *result,
                                              IECStatus *status) {
  if (max_length > kMaxReadAhead) {
    SetError(IECStatus::INVALID_ARGUMENT,
             (boost::format("max_length(%u) > kRMaxeadAhead(%u)") % max_length %
              kMaxReadAhead)
                 .str(),
             status);
    return false;
  }

  // Start looking for the terminator at data_start_. We'll change this during
  // retries to only look within newly read data.
  // TODO(aeckleder): Update for retries.
  size_t search_from = data_start_;
  while (true) {
    // Let's see if we can find the terminator in the currently buffered data.
    size_t search_to = std::min(data_end_, data_start_ + max_length);
    ssize_t found_pos = FindTerminatorFrom(term_symbol, search_from, search_to);
    if (found_pos != -1) {
      // We found our terminator. Copy all the data that comes before it and
      // update data_start_, possibly moving buffers to create space.
      ConsumeData(found_pos, /*ignore_additional_bytes=*/1, result);
      return true;
    }
    if (search_to < data_end_) {
      // We looked everywhere within max_length but couldn't find anything.
      // We're done here.
      SetError(IECStatus::CONNECTION_FAILURE,
               (boost::format("couldn't find 0x%02x") %
                static_cast<int>(term_symbol))
                   .str(),
               status);
      return false;
    }
    // Still in the game. Let's read additional data that may have become
    // available.
    size_t read_max = kBufferSize - data_end_;
    ssize_t res = -1;
    while (res <= 0) {
      ssize_t res = read(fd_, &buffer_[data_end_], read_max);
      if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "read", status);
        return false;
      }
      if (res > 0) {
        // We obtained some new data. Update data_end_ and try to find the
        // terminator within the newly read data (outer loop).
        data_end_ += res;
        break;
      }
      // We didn't read any extra data.
      // TODO(aeckleder): Block until more data becomes available.
    }
  }
}

bool BufferedReadWriter::ReadUpTo(size_t min_length, size_t max_length,
                                  std::string *result, IECStatus *status) {
  if (min_length > max_length) {
    SetError(IECStatus::INVALID_ARGUMENT,
             (boost::format("min_length(%u) > max_length(%u)") % min_length %
              max_length)
                 .str(),
             status);
    return false;
  }
  // Start with an empty string.
  result->clear();

  // Add any already cached data.
  size_t read_from_buffer = std::min(data_end_ - data_start_, max_length);
  ConsumeData(data_start_ + read_from_buffer,
              /*ignore_additional_bytes=*/0, result);
  // We already read the maximum number of characters from the buffer,
  // so we're done (and might even have some data left in the buffer).
  if (result->size() == max_length)
    return true;

  // If we get here we have consumed all of the existing buffer.
  // We'll be using this buffer to read additional data, but we'll never
  // read more than we return in result.
  assert(data_end_ - data_start_ == 0);

  ssize_t res = -1;
  while (result->size() < min_length && res <= 0) {
    size_t read_max =
        std::min(static_cast<size_t>(kBufferSize), max_length - result->size());
    ssize_t res = read(fd_, buffer_, read_max);
    if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
      SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "read", status);
      return false;
    }
    if (res > 0) {
      // Append new data to the string and do another iteration through the
      // loop without waiting for additional data. We might exit the loop as
      // a result, in case we have read at least min_length bytes.
      result->append(buffer_, res);
      continue;
    }
    // We didn't read any extra data.
    // TODO(aeckleder): Block until more data becomes available.
  }
  return true;
}

bool BufferedReadWriter::WriteString(const std::string &content,
                                     IECStatus *status) {
  if (content.empty()) {
    return true;
  }
  size_t pos = 0;
  while (pos < content.size()) {
    ssize_t result = write(fd_, &content.c_str()[pos], content.size() - pos);
    if (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
      SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "write", status);
      return false;
    }
    if (result >= 0) {
      pos += result;
    }
  }
  return true;
}

bool UnescapeString(const std::string &source, std::string *target,
                    IECStatus *status) {
  target->clear();
  for (auto it = source.begin(); it != source.end(); ++it) {
    switch (*it) {
    case '\\':
      if (++it != source.end()) {
        switch (*it) {
        case 'r':
          target->append(1, '\r');
          break;
        case '\\':
          target->append(1, '\\');
          break;
        default:
          SetError(
              IECStatus::INVALID_ARGUMENT,
              (boost::format("Invalid escape sequence '\\%c'") % *it).str(),
              status);
          return false;
        }
      } else {
        SetError(IECStatus::INVALID_ARGUMENT,
                 (boost::format("Incomplete escape sequence in string '%s'") %
                  source)
                     .str(),
                 status);
        return false;
      }
      break;
    default:
      target->append(1, *it);
      break;
    }
  }
  return true;
}
