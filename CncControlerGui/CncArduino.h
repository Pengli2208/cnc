#ifndef CNC_ARDUINO_INCLUDES
#define CNC_ARDUINO_INCLUDES

#include <string>
#include "C:\@Development\@Projekte\@StepperControl\StepperControl_1\cv.h"

///////////////////////////////////////////////////////////////////////////////////////////
class ArduinoCMDs {
	public:
		static std::string cmds[MAX_CMDS];
		static std::string ret;
		
		/////////////////////////////////////////////////////////////////////////
		ArduinoCMDs() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		~ArduinoCMDs() {
		} 
		
		/////////////////////////////////////////////////////////////////////////
		static void init();
		
		/////////////////////////////////////////////////////////////////////////
		static const char* getCMDLabel(unsigned int id);
};

///////////////////////////////////////////////////////////////////////////////////////////
class ArduinoPIDs {
	public:
		static std::string pids[MAX_PIDS];
		static std::string ret;
		
		/////////////////////////////////////////////////////////////////////////
		ArduinoPIDs() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		~ArduinoPIDs() {
		} 
		
		/////////////////////////////////////////////////////////////////////////
		static void init();
		
		/////////////////////////////////////////////////////////////////////////
		static const char* getPIDLabel(unsigned int id);
};

///////////////////////////////////////////////////////////////////////////////////////////
class ArduinoErrorCodes {
	public:
		static std::string errorCodes[MAX_ERROR_CODES];
		static std::string ret;
		
		/////////////////////////////////////////////////////////////////////////
		ArduinoErrorCodes() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		~ArduinoErrorCodes() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		static void init();
		
		/////////////////////////////////////////////////////////////////////////
		static const char* getECLabel(unsigned int id);

};

///////////////////////////////////////////////////////////////////////////////////////////
class ArduinoDigitalPins {
	public:
		static std::string pins[MAX_PINS];
		static std::string ret;
		
		/////////////////////////////////////////////////////////////////////////
		ArduinoDigitalPins() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		~ArduinoDigitalPins() {
		} 
		
		/////////////////////////////////////////////////////////////////////////
		static void init();
		
		/////////////////////////////////////////////////////////////////////////
		static const char* getPinLabel(unsigned int id);
};

///////////////////////////////////////////////////////////////////////////////////////////
class ArduinoAnalogPins {
	public:
		static std::string pins[MAX_PINS];
		static std::string ret;
		
		/////////////////////////////////////////////////////////////////////////
		ArduinoAnalogPins() {
		}
		
		/////////////////////////////////////////////////////////////////////////
		~ArduinoAnalogPins() {
		} 
		
		/////////////////////////////////////////////////////////////////////////
		static void init();
		
		/////////////////////////////////////////////////////////////////////////
		static const char* getPinLabel(unsigned int id);
};

///////////////////////////////////////////////////////////////////////////////////////////
class Helper {
  
  public:
    static const char* cnvToBinStr(unsigned char c, std::string ret) {
      for(int k = 7; k >=0 ; --k) {
        if( c & (1 << k) )  ret[7-k] = '1';
        else                ret[7-k] = '0'; 
      }
      ret[8] = '\0';
      return ret.c_str();
    }
};


#endif