// Utility functions for communication and data processing related to
// IEC bus and Arduino communication.

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <unistd.h>

// Use a relatively small read buffer. We're dealing with a fairly slow bus.
// This constant is not the read buffer size, but the maximum amount of read
// ahead that may be specified when calling ReadTerminatedString.
// The buffer size is derived from this value to be 2 * kMaxReadAhead - 1.
const int kMaxReadAhead = 1024;

struct IECStatus {
  IECStatus() : status_code(OK) {}

  // Reset the status object.
  void Clear() {
    status_code = OK;
    message.clear();
  }

  // Returns true if the status was OK.
  bool ok() const { return status_code == OK; };

  enum IECStatusCode {
    OK = 0x00,
    UNIMPLEMENTED = 0x01,
    CONNECTION_FAILURE = 0x02,
    INVALID_ARGUMENT = 0x03,
    IEC_CONNECTION_FAILURE = 0x04,
    DRIVE_ERROR = 0x05,
    END_OF_FILE = 0x06,
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

// Unescape source into target, which is cleared first. Returns true if
// successful.
// In case of an error, returns false and sets status.
bool UnescapeString(const std::string &source, std::string *target,
                    IECStatus *status);

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
  // kMaxReadAhead.
  bool ReadTerminatedString(char term_symbol, size_t max_length,
                            std::string *result, IECStatus *status);

  // Reads at least min_length characters and up to max_length characters, set
  // result to the
  // read string. Returns true if successful (reading zero bytes doesn't
  // constitute an error),
  // sets status otherwise. This method will block until at least min_length
  // characters have
  // been read or an error occurred.
  bool ReadUpTo(size_t min_length, size_t max_length, std::string *result,
                IECStatus *status);

  // Writes the specified content, with no terminator such as the null character
  // or newline. Returns true if successful, sets status otherwise.
  bool WriteString(const std::string &content, IECStatus *status);

  // Returns true if some data is currently in the buffer, false otherwise.
  bool HasBufferedData() const { return data_end_ - data_start_ > 0; }

private:
  // Looks for a terminator within [search_from, search_to).
  // Constraints: search_from >= data_start_ and search_to <= data_end_.
  // Code will check-fail if the constraints are violated.
  // Returns -1 if the terminator wasn't found.
  // Returns the position of the terminator otherwise.
  ssize_t FindTerminatorFrom(char term_symbol, size_t search_from,
                             size_t search_to);

  // Consume the buffer from [data_start_, consume_to) and copy its content to
  // result. If ignore_additional_bytes > 0, consumes the specified amount of
  // extra bytes without copying them to result. This method might decide to
  // move buffer content to reuse unused space at the beginning of the buffer,
  // so it might modify both data_start_ and data_end_. Constraint: consume_to +
  // ignore_additional_bytes <= data_end_.
  void ConsumeData(size_t consume_to, size_t ignore_additional_bytes,
                   std::string *result);

  // Our buffer must support a maximum read ahead of kMaxReadAhead,
  // and we want to be able to read up to kMaxReadAhead bytes of
  // additional data. We move the second half of the buffer into
  // the first half in case the first half is fully used.
  static const int kBufferSize = 2 * kMaxReadAhead - 1;

  // File descriptor to read from.
  int fd_;
  // The buffer we use to cache read results.
  char buffer_[kBufferSize];

  // Pointer to start of buffered, but unprocessed data (inclusive).
  // It always points into the first hald of the buffer.
  // Invariant: 0 <= data_start_ < kMaxReadAhead.
  size_t data_start_ = 0;

  // Pointers to end of buffered, but unprocessed data (exclusive).
  // Invariant: 0 <= data_end_ < kBufferSize.
  size_t data_end_ = 0;
};

#endif // UTILS_H
