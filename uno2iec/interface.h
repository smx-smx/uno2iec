#ifndef INTERFACE_H
#define INTERFACE_H

#include "iec_driver.h"
#ifdef USE_LED_DISPLAY
#include <max7219.h>
#endif

#include "cbmdefines.h"

// Serial interface documentation
// ==============================
//
// The serial interface can be operated either in host mode or in device mode.
// In host mode, the Arduino receives commands via serial line and translates them
// into appropriate IEC bus signals.
//
// In device mode, the Arduino listens for commands on the IEC bus and translates
// them into requests sent out via serial line.
//
// To be able to easily tell those commands apart, host mode uses lower case
// characters while device mode uses upper case characters.
//
// Host mode commands
// ------------------
//
// 'r': Perform a bus reset on the IEC bus.
// 'o': Open a channel. The following bytes are <device number>, <channel>,
//      <num data bytes>, <data to send to the channel>
// 'c': Close a channel. The following bytes are <device number>, <channel>.
//
// Host mode responses
// -------------------
//
// 'D': Debug / Logging output. Terminated by '\r'. (same as device mode).
// '!': Register logging facility. (same as device mode).
//
// Escaping rules:
//  Response data is escaped and terminated by '\r' to avoid having to specify
//  the size of the data upfront. We only use this mechanism when sending data
//  from the Arduino to the serial line, because buffer space on the Arduino
//  is very limited.
//
//  The escape character is '\'. These are the
//  valid escape sequences:
//   '\' + '\r': Represents ASCII code 0x0D (carriage return) when contained
//               in the data stream.
//   '\' + '\':  Represents ASCII code 0x5C (backslash) when contained in the
//               data stream.
//
// TODO(aeckleder): Document device mode requests.

/*
enum  {
	IS_FAIL = 0xFF, // IFail: SD card or fat not ok
	IS_NATIVE = 0,			// Regular file system file state
	// states 1 -> NumInterfaceStates are also valid, representing what's open
	IS_D64 = 1,
	IS_T64 = 2,
	IS_M2I = 3,
	IS_PRG = 4,
	NumInterfaceStates
};
*/

enum OpenState {
	O_NOTHING,			// Nothing to send / File not found error
	O_INFO,					// User issued a reload sd card
	O_FILE,					// A program file is opened
	O_DIR,					// A listing is requested
	O_FILE_ERR,			// Incorrect file format opened
	O_SAVE_REPLACE	// Save-with-replace is requested
};

// The base pointer of basic.
#define C64_BASIC_START 0x0801

class Interface
{
public:
	Interface(IEC& iec);
	virtual ~Interface() {}

	// The handler returns the current IEC state, see the iec_driver.hpp for possible states.
        // It is called repeatedly in a loop and will either poll the IEC bus (in device mode) or the serial line (in host mode)
        // for commands.
	byte handler(void);

	// Keeping the system date and time as set on a specific moment. The millis() will then keep the elapsed time since
	// moment the time was set.
	void setDateTime(word year, byte month, byte day, byte hour, byte minute, byte second);
	// retrieve the date and time as strings. Current time will be updated according to the elapsed millis before formatting.
	// String will be of format "yyyymmdd hhmmss", if timeOnly is true only the time part will be returned as
	// "hhmmss", this fits the TIME$ variable of cbm basic 2.0 and later.
	char* dateTimeString(char* dest, bool timeOnly);

#ifdef USE_LED_DISPLAY
	void setMaxDisplay(Max7219* pDisplay);
#endif

private:
        // Poll the IEC bus for new commands.
        byte deviceModeHandler(void);
        // Poll the serial line for new commands.
        byte hostModeHandler(void);
  
  
        // Reset device state.
	void reset(void);

        //
        // The following methods are host mode specific.
        //
        
        // Handle an open request coming in via serial line.
        // Reads remaining arguments from the serial line
        // and sends a corresponding request to the bus.
        void handleOpenRequest();
        
        // Handle a close request coming in via serial line.
        // Reads remaining arguments from the serial line
        // and sends a corresponding request to the bus.        
        void handleCloseRequest();

        //
        // The following methods are device mode specific.
        //

	void saveFile();
	void sendFile();
	void sendListing(/*PFUNC_SEND_LISTING sender*/);
	void sendStatus(void);
	bool removeFilePrefix(void);
	void sendLine(byte len, char* text, word &basicPtr);

	// handler helpers.
	void handleATNCmdCodeOpen(IEC::ATNCmd &cmd);
	void handleATNCmdCodeDataTalk(byte chan);
	void handleATNCmdCodeDataListen();
	void handleATNCmdClose();

	void updateDateTime();

	// our iec low level driver:
	IEC& m_iec;
	// This var is set after an open command and determines what to send next
	byte m_openState;			// see OpenState
	byte m_queuedError;

	// time and date and moment of setting.
	word m_year;
	byte m_month, m_day, m_hour, m_minute, m_second;
	ulong m_timeOfSet;

	// atn command buffer struct
	IEC::ATNCmd& m_cmd;
#ifdef USE_LED_DISPLAY
	Max7219* m_pDisplay;
#endif
};

#endif
