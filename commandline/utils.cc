#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <algorithm>

#include "utils.h"

// Use a relatively small read buffer. We're dealing with a fairly slow bus.
static size_t kReadBufferSize = 512;

void SetError(IECStatus::IECStatusCode status_code, const std::string& context,
              IECStatus* status) {
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
  }
  if (!context.empty()) {
    status->message = context + ": " + status->message;
  }
}

void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string& context, IECStatus* status) {
  SetError(status_code, context, status);
  status->message = status->message + ": " + strerror(errno);
}

bool BufferedReadWriter::ReadTerminatedString(char term_symbol,
                                              size_t max_length,
                                              std::string* result,
                                              IECStatus* status) {
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
           std::string("couldn't find '") + term_symbol + "'", status);
  return false;
}

bool BufferedReadWriter::WriteString(const std::string& content,
                                     IECStatus* status) {
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
