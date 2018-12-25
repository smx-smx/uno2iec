// Utility functions for communication and data processing related to
// IEC bus and Arduino communication.

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <unistd.h>

// Use a relatively small read buffer. We're dealing with a fairly slow bus.
const int kReadBufferSize = 512;

struct IECStatus {
  IECStatus() : status_code(OK) {}

  enum IECStatusCode {
    OK = 0x00,
    UNIMPLEMENTED = 0x01,
    CONNECTION_FAILURE = 0x02,
    INVALID_ARGUMENT = 0x03,
  };
  IECStatusCode status_code;
  std::string message; // A status message describing the status.
};

// Sets status according to the specified status_code and context, which will be
// used to populate the status message.
void SetError(IECStatus::IECStatusCode status_code, const std::string &context,
              IECStatus *status);

// Sets status according to the specified status_code and context, and appends a
// textual representation of the current errno.
void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string &context, IECStatus *status);

// BufferedReadWriter can be used to read both terminated and fixed
// character amounts from a file handle. It buffers reads internally,
// writes are executed immediately. Note that file handle ownership is not
// transferred. The handle will not be closed upon destruction of a
// BufferedReadWriter.
class BufferedReadWriter {
public:
  // Construct a BufferedReadWriter from a file descriptor.
  BufferedReadWriter(int fd);

  // Reads up to max_length characters until term_symbol is found and set result
  // to the read string (not including term_symbol). Returns true if successful,
  // sets status otherwise. Note that the maximum value for max_length is
  // kReadBufferSize.
  bool ReadTerminatedString(char term_symbol, size_t max_length,
                            std::string *result, IECStatus *status);

  // Writes the specified content, with no terminator such as the null character
  // or newline. Returns true if successful, sets status otherwise.
  bool WriteString(const std::string &content, IECStatus *status);

private:
  // File descriptor to read from.
  int fd_;
  // The buffer we use to cache read results.
  char buffer_[kReadBufferSize];

  // Pointers to start of buffered, but unprocessed data (inclusive).
  // Invariant: 0 <= data_start_ <= kReadBufferSize.
  size_t data_start_ = 0;

  // Pointers to end of buffered, but unprocessed data (exclusive).
  // Invariant: 0 <= data_end_ <= kReadBufferSize.
  size_t data_end_ = 0;
};

#endif // UTILS_H
