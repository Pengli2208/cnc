#ifndef SERIAL_FACADE
#define SERIAL_FACADE

#include "SerialSimulatorBase.h"
#include "SerialSpyPort.h"

class SerialSimulatorThread;

/////////////////////////////////////////////////////////////////////////
class SerialSimulatorFacade : public SerialSpyPort {
	
	private:
		SerialSimulatorThread* serialThread;
		
		void createSerialThread();
		void destroySerialThread();
		
	protected:
		wxCriticalSection 	serialThreadCS;
		
		friend class SerialSimulatorThread; 
		
		void pauseSerialThread();
		void resumeSerialThread();
		void wakeUpOnDemand();
		
		virtual void waitDuringRead(unsigned int millis); 
		virtual void sleepMilliseconds(unsigned int millis);
		
	public:
		
		wxMutex 		serialMutex;
		wxCondition 	serialCondition;
		
		//Initialize Serial communication without an acitiv connection 
		SerialSimulatorFacade(CncControl* cnc);
		//Initialize Serial communication with the given COM port
		SerialSimulatorFacade(const char *portName);
		virtual ~SerialSimulatorFacade();
		
		// returns the class name
		virtual const char* getClassName() { return "SerialFacade"; }
		// returns the emulator type
		virtual bool isEmulator() const { return true; }
		// return the port type
		virtual const CncPortType getPortType() const { return CncPORT_SIMU; }
		// indicates if idle message can be requested
		virtual bool canProcessIdle() { return false; }
		// will be released periodically be the main thread
		virtual void onPeriodicallyAppEvent(bool interrupted);
		// simulate connection
		virtual bool connect(const char* portName);
		// close the connection
		virtual void disconnect(void);
		// simulate read
		virtual int readData(void *buffer, unsigned int nbByte);
		// simulate write
		virtual bool writeData(void *buffer, unsigned int nbByte);
};

#endif