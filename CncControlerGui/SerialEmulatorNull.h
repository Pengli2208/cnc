#ifndef SERIAL_EMULATOR_CLASS
#define SERIAL_EMULATOR_CLASS

#include <ostream>
#include "SerialSpyPort.h"
#include "CncLimitStates.h"
#include "CncArduino.h"
#include "CncSpeedSimulator.h"

struct LastCommand {
	unsigned char cmd 		= CMD_INVALID;
	unsigned int index 		= 0;
		
	void restLastCmd() {
		Serial.reset();
		cmd 				= CMD_INVALID;
		index 				= 0;
	}

	struct SERIAL {
		
		private:
			static const unsigned int maxSize = 2048;
			unsigned char buffer[maxSize];
			
			unsigned char* p = NULL;
			unsigned int pos = 0;
		
		public:
			/////////////////////////////////////////////////////
			SERIAL() {
				p = buffer;
			}
			/////////////////////////////////////////////////////
			unsigned int available() const {
				return maxSize - pos;
			}
			/////////////////////////////////////////////////////
			unsigned int size() const {
				return maxSize;
			}
			/////////////////////////////////////////////////////
			unsigned int length() const {
				return pos;
			}
			/////////////////////////////////////////////////////
			void reset() {
				p = buffer;
				pos = 0;
			}
			/////////////////////////////////////////////////////
			void trace(std::ostream& out) const {
				for ( unsigned int i = 0; i < length(); i++ )
					out << wxString::Format("%02X ", buffer[i]);
					
				out << std::endl;
			}
			/////////////////////////////////////////////////////
			unsigned char get(unsigned int i) {
				if ( i > length() - 1 )
					return 0;
					
				return buffer[i];
			}
			/////////////////////////////////////////////////////
			bool getBytes(unsigned char* b, unsigned int i, unsigned int l) {
				// (l - 1) with respect to the fact that 
				// pos = i is also included
				if ( i + (l - 1) > length() - 1 )
					return false;
					
				if ( b == NULL )
					return false;
					
				memcpy(b, &buffer[i], l);
				return true;
			}
			/////////////////////////////////////////////////////
			int write(const char b) {
				return write((const unsigned char)b);
			}
			/////////////////////////////////////////////////////
			int write(const unsigned char b) {
				buffer[pos] = b;
				p++;
				pos++;
				
				return 1;
			}
			/////////////////////////////////////////////////////
			int write(const char* t) {
				if ( t == NULL )
					return 0;
					
				unsigned int size = strlen(t);
				
				if ( available() < size )
					return 0;
					
				if ( t[size - 1 ] == '\0' )
					size--;
					
				if ( size == 0 )
					return 0;
				
				memcpy(p, t, size);
				p   += size;
				pos += size;
				
				return size;
			}
			/////////////////////////////////////////////////////
			int write(int32_t v) {
				
				if ( available() < sizeof(int32_t) )
					return 0;
					
				memcpy(p, &v, sizeof(int32_t));
				p   += sizeof(int32_t);
				pos += sizeof(int32_t);
				
				return sizeof(int32_t);
			} 
			/////////////////////////////////////////////////////
			int write(int32_t v1, int32_t v2) {

				int ret = 0;
				ret += write(v1);
				ret += write(v2);
				
				return ret;
			} 
			/////////////////////////////////////////////////////
			int write(int32_t v1, int32_t v2, int32_t v3) {
				
				int ret = 0;
				ret += write(v1);
				ret += write(v2);
				ret += write(v3);
				
				return ret;
			} 
			/////////////////////////////////////////////////////
			int write(int32_t v1, int32_t v2, int32_t v3, int32_t v4) {
				
				int ret = 0;
				ret += write(v1);
				ret += write(v2);
				ret += write(v3);
				ret += write(v4);
				
				return ret;
			} 
			
		
	} Serial;
};

class SerialEmulatorNULL : public SerialSpyPort
{
	private:
		
		static const unsigned int defaultLoopDuration = 40;
		
		int32_t posReplyThresholdX;
		int32_t posReplyThresholdY;
		int32_t posReplyThresholdZ;
		
		CncLimitStates limitStates;
		
		CncSpeedSimulator* speedSimulator;

		int32_t positionCounter;
		int32_t stepCounterX;
		int32_t stepCounterY;
		int32_t stepCounterZ;

		int32_t positionOverflowCounter;
		int32_t stepOverflowCounterX;
		int32_t stepOverflowCounterY;
		int32_t stepOverflowCounterZ;

		SetterMap setterMap;
		CncLongPosition targetMajorPos;
		CncLongPosition curEmulatorPos;
		
		inline bool writeMoveCmd(unsigned char *buffer, unsigned int nbByte);
		inline bool renderMove(int32_t dx , int32_t dy , int32_t dz, unsigned char *buffer, unsigned int nbByte);
		inline bool provideMove(int32_t dx , int32_t dy , int32_t dz, unsigned char *buffer, unsigned int nbByte, bool force=false);
		
		inline void reset();
		inline void resetErrorInfo();
		inline void resetCounter();
		inline bool stepAxis(char axis, int32_t dist);
		
		void resetPositionCounter();
		void resetStepCounter();
		
		inline void writerGetterValues(unsigned char pid, int32_t v);
		inline void writerGetterValues(unsigned char pid, int32_t v1, int32_t v2);
		inline void writerGetterValues(unsigned char pid, int32_t v1, int32_t v2, int32_t v3);
		
	protected:
		LastCommand lastCommand;
		unsigned char lastSignal;
		
		struct ErrorInfo {
			unsigned int id			= 0;
			wxString additionalInfo	= _("");
		};
		
		typedef std::vector<ErrorInfo> ErrorList;
		CncNanoTimestamp errorInfoResponseId;
		ErrorList errorList;
		
		void performNextErrorInfoResponseId();
		void addErrorInfo(unsigned char eid, const wxString& text);
		
		virtual void waitDuringRead(unsigned int millis); 
		virtual void sleepMilliseconds(unsigned int millis);
		
		unsigned char getLastSignal() { return lastSignal; }
		
		virtual bool writeGetter(unsigned char *buffer, unsigned int nbByte);
		virtual bool writeSetter(unsigned char *buffer, unsigned int nbByte);
		virtual bool writeMoveCmd(int32_t x , int32_t y , int32_t z, unsigned char *buffer, unsigned int nbByte);
			
		virtual int performSerialBytes(unsigned char *buffer, unsigned int nbByte);
		
		virtual int performConfiguration(unsigned char *buffer, unsigned int nbByte);
		virtual int performErrorInfo(unsigned char *buffer, unsigned int nbByte);
		virtual int performLastErrorInfoResponseId(unsigned char *buffer, unsigned int nbByte);
		
		virtual int performSOT(unsigned char *buffer, unsigned int nbByte, const char* response);
		virtual int performMSG(unsigned char *buffer, unsigned int nbByte, const char* response);
		virtual int performMajorMove(unsigned char *buffer, unsigned int nbByte);
		
		virtual bool evaluatePositions(std::vector<int32_t>& ret);
		virtual bool evaluateLimitStates(std::vector<int32_t>& ret);
		virtual bool evaluateLimitStates();

		// position movement counting
		void incPosistionCounter();
		void incStepCounterX(int32_t dx);
		void incStepCounterY(int32_t dy);
		void incStepCounterZ(int32_t dz);

		void resetEmuPositionCounter();
		int32_t getEmuPositionCounter() { return positionCounter; }
		int32_t getEmuPositionOverflowCounter() { return positionOverflowCounter; }

		void resetEmuStepCounter();
		int32_t getEmuStepCounterX() { return stepCounterX; }
		int32_t getEmuStepCounterY() { return stepCounterY; }
		int32_t getEmuStepCounterZ() { return stepCounterZ; }

		int32_t getEmuStepOverflowCounterX() { return stepOverflowCounterX; }
		int32_t getEmuStepOverflowCounterY() { return stepOverflowCounterY; }
		int32_t getEmuStepOverflowCounterZ() { return stepOverflowCounterZ; }

	public:
		
		//Initialize Serial communication without an acitiv connection 
		SerialEmulatorNULL(CncControl* cnc);
		//Initialize Serial communication with the given COM port
		SerialEmulatorNULL(const char *portName);
		virtual ~SerialEmulatorNULL();
		
		// returns the class name
		virtual const char* getClassName() { return "SerialEmulatorNULL"; }
		// returns the emulator type
		virtual bool isEmulator() const { return true; }
		// return the port type
		virtual const CncPortType getPortType() const { return CncEMU_NULL; }
		// simulate connection
		virtual bool connect(const char* portName) { setConnected(true); return true; }
		// close the connection
		virtual void disconnect(void) { setConnected(false); }
		// simulate read
		virtual int readData(void *buffer, unsigned int nbByte);
		// simulate write
		virtual bool writeData(void *buffer, unsigned int nbByte);
		
		virtual void traceSpeedInformation();
};

#endif