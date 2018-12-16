// Utility functions for communication and data processing related to
// IEC bus and Arduino communication.

#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <string>

struct IECStatus {
  IECStatus() : status_code(OK) {}

  enum IECStatusCode {
    OK = 0x00,
    UNIMPLEMENTED = 0x01,
    CONNECTION_FAILURE = 0x02
  };
  IECStatusCode status_code;
  std::string message;  // A status message describing the status.
};

// Sets status according to the specified status_code and context, which will be
// used to populate the status message.
void SetError(IECStatus::IECStatusCode status_code, const std::string& context,
              IECStatus* status);

// Sets status according to the specified status_code and context, and appends a
// textual representation of the current errno.
void SetErrorFromErrno(IECStatus::IECStatusCode status_code,
                       const std::string& context, IECStatus* status);

// Reads up to max_length characters from fd until term_symbol is found and
// set result to the read string (not including term_symbol). Returns true if
// successful, sets status otherwise.
bool ReadTerminatedString(int fd, char term_symbol, size_t max_length,
                          std::string* result, IECStatus* status);

// Writes the specified content to fd, with no terminator such as the null
// character or newline. Returns true if successful, sets status otherwise.
bool WriteString(int fd, const std::string& content, IECStatus* status);

#endif  // UTILS_H
