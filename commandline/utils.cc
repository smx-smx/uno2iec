#include "utils.h"

#include <algorithm>
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

bool BufferedReadWriter::ReadTerminatedString(char term_symbol,
                                              size_t max_length,
                                              std::string *result,
                                              IECStatus *status) {
  if (max_length > kReadBufferSize) {
    SetError(IECStatus::INVALID_ARGUMENT,
             (boost::format("max_length(%u) > kReadBufferSize(%u)") %
              max_length % kReadBufferSize)
                 .str(),
             status);
    return false;
  }

  char buffer[kReadBufferSize];
  memset(buffer, 0, sizeof(buffer));
  result->clear();
  while (result->size() < max_length) {
    ssize_t res = read(fd_, buffer,
                       std::min(sizeof(buffer), max_length - result->size()));
    if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
      SetErrorFromErrno(IECStatus::CONNECTION_FAILURE, "read", status);
      return false;
    }
    if (res >= 0) {
      ssize_t term_pos = -1;
      for (ssize_t i = 0; i < res; ++i) {
        if (buffer[i] == term_symbol) {
          term_pos = i;
          break;
        }
      }
      if (term_pos != -1) {
        result->append(buffer, term_pos);
        return true;
      }
      // Append what we read and go into the next iteration.
      result->append(buffer, res);
    }
  }
  // Read too much data without finding a terminator.
  SetError(IECStatus::CONNECTION_FAILURE,
           (boost::format("couldn't find '%c'") % term_symbol).str(), status);
  return false;
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
