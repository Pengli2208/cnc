
#include <iostream>
#include <sstream>
#include <cfloat>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/slider.h>
#include <wx/dataview.h>
#include <wx/propgrid/manager.h>
#include <wx/evtloop.h>
#include "OSD/CncAsyncKeyboardState.h"
#include "DataControlModel.h"
#include "SerialPort.h"
#include "SerialSimulatorFacade.h"
#include "SerialEmulatorNull.h"
#include "SerialEmulatorFile.h"
#include "SerialEmulatorSVG.h"
#include "CncMotionMonitor.h"
#include "CncCommon.h"
#include "CncControl.h"
#include "CncFileNameService.h"
#include "MainFrame.h"

static CommandTemplates CMDTPL;

///////////////////////////////////////////////////////////////////
CncControl::CncControl(CncPortType pt) 
: currentClientId(-1)
, setterMap()
, portType(pt)
, serialPort(NULL)
, cncConfig(NULL)
, zeroPos(0,0,0)
, startPos(0,0,0)
, curAppPos(0,0,0)
, controllerPos(0,0,0)
, speedType(CncSpeedRapid)
, configuredFeedSpeed_MM_MIN(0.0)
, currentFeedSpeed_MM_MIN(MIN_LONG)
, defaultFeedSpeedRapid_MM_MIN(GBL_CONFIG->getDefaultRapidSpeed_MM_MIN())
, defaultFeedSpeedWork_MM_MIN(GBL_CONFIG->getDefaultRapidSpeed_MM_MIN())
, durationCounter(0)
, interruptState(false)
, positionOutOfRangeFlag(false)
, powerOn(false)
, toolUpdateState(true)
, stepDelay(0)
, guiCtlSetup(NULL)
, positionCheck(true)
, drawPaneMargin(30)
, speedMonitorMode(DM_2D)
{
//////////////////////////////////////////////////////////////////
	if      ( pt == CncPORT ) 			serialPort = new SerialSpyPort(this);
	else if ( pt == CncPORT_SIMU )		serialPort = new SerialSimulatorFacade(this);
	else if ( pt == CncEMU_NULL )		serialPort = new SerialEmulatorNULL(this);
	else if ( pt == CncEMU_SVG )		serialPort = new SerialEmulatorSVG(this);
	else 								serialPort = new SerialSpyPort(this);
	
	serialPort->enableSpyOutput();
	
	// create default config
	cncConfig = CncConfig::getGlobalCncConfig();
	
	// init pen handler
	penHandler.reset();
}
///////////////////////////////////////////////////////////////////
CncControl::~CncControl() {
///////////////////////////////////////////////////////////////////
	assert(serialPort);
	
	if ( getToolState() == true )
		switchToolOff();
	
	// safty
	disconnect();

	delete serialPort;
}
//////////////////////////////////////////////////////////////////
const CncDoublePosition CncControl::getStartPosMetric() {
//////////////////////////////////////////////////////////////////
	CncDoublePosition retValue;
	retValue.setXYZ(startPos.getX() * cncConfig->getDisplayFactX(),
				    startPos.getY() * cncConfig->getDisplayFactY(),
	                startPos.getZ() * cncConfig->getDisplayFactZ());
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncDoublePosition CncControl::getCurPosMetric() {
//////////////////////////////////////////////////////////////////
	CncDoublePosition retValue;
	retValue.setXYZ(curAppPos.getX() * cncConfig->getDisplayFactX(),
				    curAppPos.getY() * cncConfig->getDisplayFactY(),
	                curAppPos.getZ() * cncConfig->getDisplayFactZ());
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncLongPosition CncControl::getMinPositions() {
//////////////////////////////////////////////////////////////////
	CncLongPosition retValue;
	retValue.setXYZ(curAppPos.getXMin(), curAppPos.getYMin(), curAppPos.getZMin());
	return retValue;
}//////////////////////////////////////////////////////////////////
const CncLongPosition CncControl::getMaxPositions() {
//////////////////////////////////////////////////////////////////
	CncLongPosition retValue;
	retValue.setXYZ(curAppPos.getXMax(), curAppPos.getYMax(), curAppPos.getZMax());
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncDoublePosition CncControl::getMinPositionsMetric() {
//////////////////////////////////////////////////////////////////
	CncDoublePosition retValue;
	retValue.setXYZ(curAppPos.getXMin() * cncConfig->getDisplayFactX(),
					curAppPos.getYMin() * cncConfig->getDisplayFactY(),
					curAppPos.getZMin() * cncConfig->getDisplayFactZ());
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncDoublePosition CncControl::getMaxPositionsMetric() {
//////////////////////////////////////////////////////////////////
	CncDoublePosition retValue;
	retValue.setXYZ(curAppPos.getXMax() * cncConfig->getDisplayFactX(),
					curAppPos.getYMax() * cncConfig->getDisplayFactY(),
					curAppPos.getZMax() * cncConfig->getDisplayFactZ());
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncLongPosition::Watermarks CncControl::getWaterMarks() {
//////////////////////////////////////////////////////////////////
	CncLongPosition::Watermarks retValue;
	curAppPos.getWatermarks(retValue);
	return retValue;
}
//////////////////////////////////////////////////////////////////
const CncDoublePosition::Watermarks CncControl::getWaterMarksMetric() {
//////////////////////////////////////////////////////////////////
	CncDoublePosition::Watermarks retValue;

	CncLongPosition::Watermarks xyz = CncControl::getWaterMarks();
	retValue.xMin = xyz.xMin * cncConfig->getDisplayFactX();
	retValue.xMax = xyz.xMax * cncConfig->getDisplayFactX();

	retValue.yMin = xyz.yMin * cncConfig->getDisplayFactY();
	retValue.yMax = xyz.yMax * cncConfig->getDisplayFactY();
	
	retValue.zMin = xyz.zMin * cncConfig->getDisplayFactZ();
	retValue.zMax = xyz.zMax * cncConfig->getDisplayFactZ();

	return retValue;
}
///////////////////////////////////////////////////////////////////
bool CncControl::processSetter(unsigned char id, int32_t value) {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() )
		return false;

	if ( isConnected() == false )
		return false;
	
	if ( id != PID_SEPARATOR ) {
		
		if ( GBL_CONFIG->getAvoidDupSetterValuesFlag() ) {
			auto it = setterMap.find((int)id);
			if ( it != setterMap.end() ) {
				// value dosen't changed
				if ( it->second == value )
					return true;
			}
		}
			
		if ( serialPort->processSetter(id, value) == false ) {
			std::cerr << std::endl << "CncControl::processSetterList: Setter failed." << std::endl;
			std::cerr << " Id:    " << ArduinoPIDs::getPIDLabel((int)id) << std::endl;
			std::cerr << " Value: " << value << std::endl;

			return false;
		}
		
		// store
		setterMap[id] = value;
	}
	
	// publish setter event
	typedef UpdateManagerThread::Event Event;
	static Event evt;
	
	if ( GET_GUI_CTL(mainFrame) )
		GET_GUI_CTL(mainFrame)->umPostEvent(evt.SetterEvent(id, value));

	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::processSetterList(std::vector<SetterTuple>& setup) {
///////////////////////////////////////////////////////////////////
	for ( auto it = setup.begin(); it != setup.end(); ++it) {
		if ( processSetter((*it).id, (*it).value) == false ) {
			return false;
		}
	}
	
	return true;
}
///////////////////////////////////////////////////////////////////
void CncControl::resetSetterMap() {
///////////////////////////////////////////////////////////////////
	setterMap.clear();
}
///////////////////////////////////////////////////////////////////
bool CncControl::setup(bool doReset) {
///////////////////////////////////////////////////////////////////
	wxASSERT(serialPort);
	wxASSERT(cncConfig);
	
	// always reset the map here to definitly reinitianlize the controller
	resetSetterMap();
	
	if ( serialPort->isConnected() == false ) 
		return false;
	
	// reste controller on demand
	if ( doReset == true ) {
		if ( reset() == false ) {
			std::cerr << " CncControl::setup: reset controller failed!\n";
			return false;
		}
			
		// Firmware check
		std::stringstream ss;
		processCommand(CMD_PRINT_VERSION, ss);
		std::cout << " Firmware check . . . [Available: " << ss.str() << "; Required: " << FIRMWARE_VERSION << "] . . .";
		
		if ( wxString(FIRMWARE_VERSION) == ss.str().c_str() )	std::clog << " OK" << std::endl;
		else													cnc::cex1 << " Firmware is possibly not compatible!" << std::endl;
	}
	
	// evaluate limit states
	evaluateLimitState();
	
	std::cout << " Starting controller initialization . . . \n";
	wxTextCtrl* logger = GBL_CONFIG->getTheApp()->GetLogger(); wxASSERT( logger != NULL );
	long logPos = logger->GetLastPosition();
	
	// setup probe mode
	bool ret = true;
	if ( GBL_CONFIG->isProbeMode() )	ret = processCommand(CMD_ENABLE_PROBE_MODE, std::cerr);
	else								ret = processCommand(CMD_DISABLE_PROBE_MODE, std::cerr);
	
	if ( ret == false ) {
		std::cerr << " CncControl::setup: Probe mode configuration failed!\n";
		return false;
	}
	
	// process initial setters
	std::vector<SetterTuple> setup;
	setup.push_back(SetterTuple(PID_SEPARATOR, SEPARARTOR_SETUP));
	
	setup.push_back(SetterTuple(PID_STEPS_X, cncConfig->getStepsX()));
	setup.push_back(SetterTuple(PID_STEPS_Y, cncConfig->getStepsY()));
	setup.push_back(SetterTuple(PID_STEPS_Z, cncConfig->getStepsZ()));
	
	setup.push_back(SetterTuple(PID_PITCH_X, convertDoubleToCtrlLong(PID_PITCH_X, cncConfig->getPitchX())));
	setup.push_back(SetterTuple(PID_PITCH_Y, convertDoubleToCtrlLong(PID_PITCH_Y, cncConfig->getPitchY())));
	setup.push_back(SetterTuple(PID_PITCH_Z, convertDoubleToCtrlLong(PID_PITCH_Z, cncConfig->getPitchZ())));
	
	setup.push_back(SetterTuple(PID_PULSE_WIDTH_OFFSET_X, cncConfig->getPulsWidthOffsetX()));
	setup.push_back(SetterTuple(PID_PULSE_WIDTH_OFFSET_Y, cncConfig->getPulsWidthOffsetY()));
	setup.push_back(SetterTuple(PID_PULSE_WIDTH_OFFSET_Z, cncConfig->getPulsWidthOffsetZ()));
	
	setup.push_back(SetterTuple(PID_POS_REPLY_THRESHOLD_X, cncConfig->getReplyThresholdStepsX()));
	setup.push_back(SetterTuple(PID_POS_REPLY_THRESHOLD_Y, cncConfig->getReplyThresholdStepsY()));
	setup.push_back(SetterTuple(PID_POS_REPLY_THRESHOLD_Z, cncConfig->getReplyThresholdStepsZ()));
	
	if ( processSetterList(setup) == false) {
		std::cerr << " CncControl::setup: Calling processSetterList() failed!\n";
		return false;
	}
	
	// speed setup
	changeSpeedToDefaultSpeed_MM_MIN(CncSpeedRapid);
		
	// check if some output was logged in between, if not 
	// remove last '\n' and put 'Ready' at the end of the
	// same line as the starting the initialization hint
	if ( logPos == logger->GetLastPosition() ) {
		logger->Remove(logPos - 1, logPos);
	}
	
	std::clog << "Ready - OK\n";
	return true;
}
///////////////////////////////////////////////////////////////////
long CncControl::convertDoubleToCtrlLong(unsigned char id, double d) { 
///////////////////////////////////////////////////////////////////
	if ( d <= MIN_LONG / DBL_FACT ) {
		std::cerr << "CncControl::convertDoubleToCtrlLong(): Invalid double value: '" << d << "' for PID: " << ArduinoPIDs::getPIDLabel(id) << std::endl;
		return MIN_LONG; 
	}
		
	if ( d >= MAX_LONG / DBL_FACT ) {
		std::cerr << "CncControl::convertDoubleToCtrlLong(): Invalid double value: '" << d << "' for PID: " << ArduinoPIDs::getPIDLabel(id) << std::endl;
		return MAX_LONG; 
	}
		
	return d * DBL_FACT; 
}
///////////////////////////////////////////////////////////////////
bool CncControl::disconnect() {
///////////////////////////////////////////////////////////////////
	if ( serialPort->isConnected() ) {
		wxTextCtrl* logger = GBL_CONFIG->getTheApp()->GetLogger(); wxASSERT( logger != NULL );
		
		std::cout << " Disconnecting serial port . . .\n";
		long logPos = logger->GetLastPosition();
		
		serialPort->disconnect();
		
		if ( logPos == logger->GetLastPosition() )
			logger->Remove(logPos - 1, logPos);
	
		std::clog << " Disconnected\n";
	}
	
	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::connect(const char * portName) {
///////////////////////////////////////////////////////////////////
	wxASSERT(serialPort);
	disconnect();
	
	std::clog << "Try to connect to: " << serialPort->getClassName() << "("<< portName << ")" << std::endl;
	bool ret = serialPort->connect(portName);
	if ( ret == true ) {
		std::cout << " . . . Connection established -";
		std::clog << " OK" << std::endl;
	}

	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::isConnected() {
///////////////////////////////////////////////////////////////////
	wxASSERT(serialPort);
	return serialPort->isConnected();
}
///////////////////////////////////////////////////////////////////
void CncControl::onPeriodicallyAppEvent() {
///////////////////////////////////////////////////////////////////
	wxASSERT(serialPort);
	serialPort->onPeriodicallyAppEvent(isInterrupted());
}
///////////////////////////////////////////////////////////////////
bool CncControl::processCommand(const unsigned char c, std::ostream& txtCtl) {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() == true )
		return false;
	
	wxASSERT(serialPort);
	return serialPort->processCommand(c, txtCtl, curAppPos);
}
///////////////////////////////////////////////////////////////////
bool CncControl::processCommand(const char* cmd, std::ostream& txtCtl) {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() == true )
		return false;

	wxASSERT(serialPort);
	return serialPort->processCommand(cmd, txtCtl, curAppPos);
}
///////////////////////////////////////////////////////////////////
bool CncControl::processMoveXYZ(int32_t x1, int32_t y1, int32_t z1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() == true )
		return false;

	wxASSERT(serialPort);
	return serialPort->processMoveXYZ(x1, y1, z1, alreadyRendered, curAppPos);
}
///////////////////////////////////////////////////////////////////
void CncControl::resetDrawControlInfo() {
///////////////////////////////////////////////////////////////////	
	penHandler.reset();
}
///////////////////////////////////////////////////////////////////
void CncControl::updateDrawControl() {
///////////////////////////////////////////////////////////////////
	updatePreview3D(true); 
}
///////////////////////////////////////////////////////////////////
inline void CncControl::setValue(wxTextCtrl *ctl, int32_t val) {
///////////////////////////////////////////////////////////////////
	if ( ctl != NULL ) {
		ctl->ChangeValue(wxString::Format(wxT("%i"),val));
	}
}
///////////////////////////////////////////////////////////////////
inline void CncControl::setValue(wxTextCtrl *ctl, double val) {
///////////////////////////////////////////////////////////////////
	if ( ctl != NULL ) {
		ctl->ChangeValue(wxString::Format(wxT("%4.3f"),val));
	}
}
///////////////////////////////////////////////////////////////////
inline void CncControl::setValue(wxTextCtrl *ctl, const char* val) {
///////////////////////////////////////////////////////////////////
	if ( ctl != NULL ) {
		ctl->ChangeValue(wxString::Format(wxT("%s"),val));
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::setGuiControls(GuiControlSetup* gcs) {
///////////////////////////////////////////////////////////////////
	assert(gcs);
	guiCtlSetup = gcs;
	toolState.setControl(GET_GUI_CTL(toolState));
	setToolState(true);
}
///////////////////////////////////////////////////////////////////
void CncControl::setZeroPosX() {
///////////////////////////////////////////////////////////////////
	curAppPos.setX(0);
	zeroPos.setX(0);
	startPos.setX(0);
	
	postAppPosition(PID_XYZ_POS_MAJOR);
}
///////////////////////////////////////////////////////////////////
void CncControl::setZeroPosY() {
///////////////////////////////////////////////////////////////////
	curAppPos.setY(0);
	zeroPos.setY(0);
	startPos.setY(0);
	
	postAppPosition(PID_XYZ_POS_MAJOR);
}
///////////////////////////////////////////////////////////////////
void CncControl::setZeroPosZ() {
///////////////////////////////////////////////////////////////////
	wxASSERT( guiCtlSetup );
	
	int32_t val = 0L;
	
	if ( cncConfig->getReferenceIncludesWpt() == true )
		val = (long)round(cncConfig->getWorkpieceThickness() * cncConfig->getCalculationFactZ());
	
	curAppPos.setZ(val);
	zeroPos.setZ(val);
	startPos.setZ(val);
	
	postAppPosition(PID_XYZ_POS_MAJOR);
}
///////////////////////////////////////////////////////////////////
void CncControl::setZeroPos() {
///////////////////////////////////////////////////////////////////
	setZeroPosX();
	setZeroPosY();
	setZeroPosZ();
}
///////////////////////////////////////////////////////////////////
void CncControl::setStartPos() {
///////////////////////////////////////////////////////////////////
	startPos = curAppPos;
}
///////////////////////////////////////////////////////////////////
void CncControl::interrupt() {
///////////////////////////////////////////////////////////////////
	std::cerr << "CncControl: Interrupted" << std::endl;
	interruptState = true;
	switchToolOff(true);
}
///////////////////////////////////////////////////////////////////
bool CncControl::isReadyToRun() {
///////////////////////////////////////////////////////////////////
	if ( isConnected() == false ) {
		std::cerr << "CncControl::isReadyToRun: The controller isn't connected!" << std::endl;
		return false;
	}
	
	if ( isInterrupted() == true ) {
		std::cerr << "CncControl::isReadyToRun: The controller is interrupted. A reset is required!" << std::endl;
		return false;
	}
	
	// query the serial port
	std::vector<int32_t> list;
	getSerial()->processGetter(PID_QUERY_READY_TO_RUN, list);
		
	if ( list.size() != 1 ) {
		std::cerr << "CncControl::isReadyToRun: Unable to a correcponding state from teh serial port:" << std::endl;
		return false;
	}
	
	return ( list.at(0) == 1L );
}
///////////////////////////////////////////////////////////////////
bool CncControl::isInterrupted() {
///////////////////////////////////////////////////////////////////
	return interruptState;
}
///////////////////////////////////////////////////////////////////
void CncControl::resetInterrupt() {
///////////////////////////////////////////////////////////////////
	resetDurationCounter();
	interruptState = false;
}
///////////////////////////////////////////////////////////////////
bool CncControl::resetWatermarks() {
///////////////////////////////////////////////////////////////////
	zeroPos.resetWatermarks();
	startPos.resetWatermarks();
	curAppPos.resetWatermarks();
	curCtlPos.resetWatermarks();
	controllerPos.resetWatermarks();
	
	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::reset() {
///////////////////////////////////////////////////////////////////
	getSerial()->purge();
	resetInterrupt();
	resetPositionOutOfRangeFlag();
	
	wxTextCtrl* logger = GBL_CONFIG->getTheApp()->GetLogger(); wxASSERT( logger != NULL );
	std::cout << " Try to reset the controller . . .\n";
	long logPos = logger->GetLastPosition();
	
	bool ret = processCommand(CMD_RESET_CONTROLLER, std::cerr);
	if ( logPos == logger->GetLastPosition() )
		logger->Remove(logPos - 1, logPos);
		
	if ( ret == true ) {
		std::clog << " Controller reseted - OK\n";
	} else {
		std::cerr << " Controller reset failed\n";
		return false;
	}
	
	// do this after the controller reset, because setZeroPos will determine a new controller position on demand
	setZeroPos();
	
	curCtlPos = getControllerPos();
	postCtlPosition(PID_XYZ_POS_MAJOR);
	
	evaluateLimitState();
	switchToolOff();
	
	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::resetErrorInfo() {
////////////checkForErrosAndResetErrorInfo/////////////////////////////////////////
	bool ret = processCommand(CMD_RESET_ERRORINFO, std::cerr);
	if ( ret == false) {
		std::cerr << " CncControl::resetErrorInfo: Reset error information failed!\n";
		return false;
	}
	
	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::checkForErrosAndResetErrorInfo(int32_t& preveErrorCount) {
///////////////////////////////////////////////////////////////////
	// first of all purge the port to reinitialize the protocol here
	// important in case of previous error sitiuations
	// getSerial()->purge(); will be done by getControllerErrorCount() below
	
	// ... then determine the current error count and request 
	// error information on demand
	preveErrorCount = getControllerErrorCount();
	if ( preveErrorCount > 0 ) {
		if ( processCommand(CMD_PRINT_ERRORINFO, std::cerr) == false )
			std::cerr << " CncControl::resetErrorInfo: Request error information failed!\n";
	}
	
	// ... and last reset the error information
	return resetErrorInfo();
}
///////////////////////////////////////////////////////////////////
unsigned int CncControl::getDurationCount() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	return cncConfig->getDurationCount();
}
///////////////////////////////////////////////////////////////////
bool CncControl::hasNextDuration() {
///////////////////////////////////////////////////////////////////
	return ( durationCounter < getDurationCount() );
}
///////////////////////////////////////////////////////////////////
void CncControl::resetDurationCounter() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	durationCounter = 0;
	penHandler.reset();
	
	if ( GET_GUI_CTL(passingTrace) && toolUpdateState == true )
		GET_GUI_CTL(passingTrace)->SetValue(wxString() << durationCounter);
}
///////////////////////////////////////////////////////////////////
void CncControl::initNextDuration() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	penHandler.initNextDuration();
	getSerial()->beginDuration(getDurationCounter());
	
	durationCounter++;
	
	if ( GET_GUI_CTL(passingTrace) && toolUpdateState == true )
		GET_GUI_CTL(passingTrace)->SetValue(wxString() << durationCounter);
}
///////////////////////////////////////////////////////////////////
unsigned int CncControl::getDurationCounter() {
///////////////////////////////////////////////////////////////////
	return durationCounter;
}
///////////////////////////////////////////////////////////////////
bool CncControl::isFirstDuration() {
///////////////////////////////////////////////////////////////////
	return (durationCounter == 1);
}
///////////////////////////////////////////////////////////////////
bool CncControl::isLastDuration() {
///////////////////////////////////////////////////////////////////
	return (durationCounter == getDurationCount());
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToBottom() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	
	double curZPos = curAppPos.getZ() * cncConfig->getDisplayFactZ(); // we need it as mm
	double moveZ   = curZPos * (-1);
	
	bool ret = true;
	if ( prepareSimpleMove() == true ) {
		if ( moveRelMetricZ(moveZ) == false ) {
			std::cerr << "CncControl: Move Z to bottom error"<< std::endl;
			ret = false;
		}
	} else {
		ret = false;
	}
	reconfigureSimpleMove(false);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToTop() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	
	double topZPos = cncConfig->getCurZDistance(); //cncConfig->getMaxZDistance();
	double curZPos = curAppPos.getZ() * cncConfig->getDisplayFactZ(); // we need it as mm
	double moveZ   = topZPos - curZPos;
	
	bool ret = true;
	if ( prepareSimpleMove() == true ) {
		if ( moveRelMetricZ(moveZ) == false ) {
			std::cerr << "CncControl: Move Z to top error"<< std::endl;
			ret = false;
		}
	} else {
		ret = false;
	}
	
	reconfigureSimpleMove(false);
	return ret;
}
///////////////////////////////////////////////////////////////////
const wxString& CncControl::getSpeedTypeAsString() {
///////////////////////////////////////////////////////////////////
	static wxString ret;

	switch( speedType ) {
		case CncSpeedWork: 	ret.assign("W"); break;
		case CncSpeedRapid:	ret.assign("R"); break;
	}
	
	return ret;
}
///////////////////////////////////////////////////////////////////
void CncControl::changeCurrentFeedSpeedXYZ_MM_MIN(CncSpeed s, double value) {
///////////////////////////////////////////////////////////////////
	// always reset the current speed value
	currentFeedSpeed_MM_MIN = MIN_LONG;
	
	if ( value <= 0.0 )
		return;
	
	if ( speedType == s && configuredFeedSpeed_MM_MIN == value )
		return;
		
	speedType = s;
	configuredFeedSpeed_MM_MIN = value;
	processSetter(PID_SPEED_MM_MIN, (long)(configuredFeedSpeed_MM_MIN * DBL_FACT));
}
///////////////////////////////////////////////////////////////////
void CncControl::changeCurrentFeedSpeedXYZ_MM_SEC(CncSpeed s, double value) {
///////////////////////////////////////////////////////////////////
	changeCurrentFeedSpeedXYZ_MM_MIN(s, value * 60);
}
///////////////////////////////////////////////////////////////////
void CncControl::changeSpeedToDefaultSpeed_MM_MIN(CncSpeed s) {
///////////////////////////////////////////////////////////////////
	double value = 0.0;
	
	switch( s ) {
		case CncSpeedWork: 	value = GBL_CONFIG->getDefaultWorkSpeed_MM_MIN(); 	break;
		case CncSpeedRapid:	value = GBL_CONFIG->getDefaultRapidSpeed_MM_MIN();	break;
	}
	
	changeCurrentFeedSpeedXYZ_MM_MIN(s, value);
}
///////////////////////////////////////////////////////////////////
void CncControl::setDefaultRapidSpeed_MM_MIN(double s) { 
///////////////////////////////////////////////////////////////////
	if ( s <= 0.0)
		return;
		
	if ( s > GBL_CONFIG->getMaxSpeedXYZ_MM_MIN() )
		return;
		
	defaultFeedSpeedRapid_MM_MIN = s; 
}
///////////////////////////////////////////////////////////////////
void CncControl::setDefaultWorkSpeed_MM_MIN(double s)  { 
///////////////////////////////////////////////////////////////////
	if ( s <= 0.0)
		return;
		
	if ( s > GBL_CONFIG->getMaxSpeedXYZ_MM_MIN() )
		return;

	defaultFeedSpeedWork_MM_MIN  = s; 
}
///////////////////////////////////////////////////////////////////
bool CncControl::isPositionOutOfRange(const CncLongPosition& pos, bool trace) {
///////////////////////////////////////////////////////////////////
	if ( positionCheck == false )
		return false;
		
	// will only be done for emulation ports. It didn't makes sense for a cnc run
	// see below
	if ( serialPort->getPortType() != CncPORT ) {
		// in the real cnc life this is not good enough. the check has to be done by
		// the end switches at last.
		bool error = false;
		CncLongPosition::Watermarks wm;
		pos.getWatermarks(wm);
		
		if ( (wm.xMax - wm.xMin)/cncConfig->getCalculationFactX() > cncConfig->getMaxDimensionX() ) error = true;
		if ( (wm.yMax - wm.yMin)/cncConfig->getCalculationFactY() > cncConfig->getMaxDimensionY() ) error = true;
		if ( (wm.zMax - wm.zMin)/cncConfig->getCalculationFactZ() > cncConfig->getMaxDimensionZ() ) error = true;
	
		if ( error == true && trace == true ) {
			std::cerr << "Position out of range!" << std::endl;
			std::cerr << " Max valid X dimension: " << cncConfig->getMaxDimensionX() << std::endl;
			std::cerr << " Max valid Y dimension: " << cncConfig->getMaxDimensionY() << std::endl;
			std::cerr << " Max valid Z dimension: " << cncConfig->getMaxDimensionZ() << std::endl;
			std::cerr << " Pos: " << pos << std::endl;
			std::cerr << " Min Watermark: " << wm.xMin << "," << wm.yMin << "," << wm.zMin << "," << std::endl;
			std::cerr << " Max Watermark: " << wm.xMax << "," << wm.yMax << "," << wm.zMax << "," << std::endl;
			std::cerr << " Calculated spread X :" <<  (wm.xMax - wm.xMin)/cncConfig->getCalculationFactX() << std::endl;
			std::cerr << " Calculated spread Y :" <<  (wm.yMax - wm.yMin)/cncConfig->getCalculationFactY() << std::endl;
			std::cerr << " Calculated spread Z :" <<  (wm.zMax - wm.zMin)/cncConfig->getCalculationFactZ() << std::endl;
			
			return true;
		}
	}
	
	return false;
}
///////////////////////////////////////////////////////////////////
void CncControl::monitorPosition(const CncLongPosition& pos) {
///////////////////////////////////////////////////////////////////
	// motion monitor
	static GLI::VerticeLongData vd;
	static CncLongPosition prevPos;

	if ( pos != prevPos ) {
		
		if ( IS_GUI_CTL_VALID(motionMonitor) ) {
			vd.setVertice(getClientId(), getCurrentSpeedType(), pos);
			GET_GUI_CTL(motionMonitor)->appendVertice(vd);
			
			updatePreview3D(false);
		}
		
		prevPos = pos;
		
#warning - to do: move flag to configuration
		if ( false ) {
			if ( isPositionOutOfRange(pos, true) == true )
				interrupt();
		} else {
			if ( isPositionOutOfRange(pos, false) == true )
				positionOutOfRangeFlag = true;
		}
	}
}
///////////////////////////////////////////////////////////////////
bool CncControl::SerialMessageCallback(const ControllerMsgInfo& cmi) {
///////////////////////////////////////////////////////////////////
	wxDateTime now = wxDateTime::UNow();
	wxString msg(cmi.message.str().c_str());
	char type = (char)msg[0];
	msg = msg.SubString(1, msg.length());
	
	int p1 = wxNOT_FOUND, p2 = wxNOT_FOUND;
	if ( (p1 = msg.Find("{[")) != wxNOT_FOUND ) {
		if ( (p2 = msg.Find("]}")) != wxNOT_FOUND && p2 >= p1 + 2) {
			wxString idStr = msg.SubString(p1+2, p2-1);
			long id;
			idStr.ToLong(&id);
			wxString errorCode = ArduinoErrorCodes::getECLabel(id);
			wxString replace("{[");
			replace << idStr;
			replace << "]}";
			
			msg.Replace(replace, errorCode);
		}
	}
	
	if ( GET_GUI_CTL(mainFrame) != NULL )
		GET_GUI_CTL(mainFrame)->displayNotification(type, "Controller Callback", msg, (type == 'E' ? 8 : 4));
	
	switch ( type ) {
		
		case 'W':	
					cnc::msg.logWarning(now.Format("Warning Message received: %H:%M:%S.%l\n"));
					cnc::msg.logWarning(msg);
					break;
					
		case 'E':	cnc::msg.logError(now.Format("Error Message received: %H:%M:%S.%l\n"));
					cnc::msg.logError(msg);
					break;
					
		default:	cnc::msg.logInfo(now.Format("Info Message received: %H:%M:%S.%l\n"));
					cnc::msg.logInfo(msg);
	}
	
	cnc::msg.setTextColour(wxColour(128, 128, 0));
	cnc::msg << "-------------------------------------------------------------------------------------------\n";
	cnc::msg.resetTextAttr();

	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::SerialControllerCallback(const ContollerInfo& ci) {
///////////////////////////////////////////////////////////////////
	// Event handling, enables the interrrpt functionality
	if ( cncConfig->isAllowEventHandling() )
		THE_APP->dispatchAll();
		
	if ( isInterrupted() )
		return false;
	
	switch ( ci.infoType ) {
		// --------------------------------------------------------
		case CITHeartbeat:
			if ( ci.command == 'T' ) {
				std::stringstream ss;
				ss << "Heartbeat received - Value: " << ci.heartbeatValue << std::endl;
				cnc::trc.logInfoMessage(ss);
			}
			break;
			
		// --------------------------------------------------------
		case CITPosition:
			// update controller position
			switch ( ci.posType ) {
				case PID_X_POS: 		curCtlPos.setX(ci.xCtrlPos); break;
				case PID_Y_POS: 		curCtlPos.setY(ci.yCtrlPos); break;
				case PID_Z_POS: 		curCtlPos.setZ(ci.zCtrlPos); break;
				
				case PID_XYZ_POS:
				case PID_XYZ_POS_MAJOR:
				case PID_XYZ_POS_DETAIL:
				default:				curCtlPos.setXYZ(ci.xCtrlPos, ci.yCtrlPos, ci.zCtrlPos);
										currentFeedSpeed_MM_MIN = ci.feedSpeed;
			}
			
			// display controller coordinates
			postCtlPosition(ci.posType);
			
			// motion monitor
			monitorPosition(curCtlPos);
			
			break;
			
		// --------------------------------------------------------
		case CITSetterInfo:
			//if ( getSerial()->isSpyOutput() == true )
			//	cnc::spy << "Setter: " << ArduinoPIDs::getPIDLabel((int)ci.setterId) << ": " << ci.setterValue << std::endl;
			break;
		
		// --------------------------------------------------------
		case CITLimitInfo:
			//std::clog << "::L: " << ci.xLimit << ", " << ci.yLimit << ", " << ci.zLimit << std::endl;
			evaluateLimitState(ci.xLimit, ci.yLimit, ci.zLimit);
			break;
		
		// --------------------------------------------------------
		default:
			std::cerr << "CncControl::SerialControllerCallback:" << std::endl;
			std::cerr << " No handler defined for controller info type:" << ci.infoType << std::endl;
	}

	return true;
}
///////////////////////////////////////////////////////////////////
bool CncControl::SerialCallback(int32_t cmdCount) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	wxASSERT(guiCtlSetup);

	if ( isInterrupted() ) {
		std::cerr << "SerialCallback: Interrupt detected"<< std::endl;
		return false;
	}

	// Event handling, enables the interrupt functionallity
	if ( cncConfig->isAllowEventHandling() )
		THE_APP->dispatchAll();
	
	// display application coordinates
	postAppPosition(PID_XYZ_POS_MAJOR);
	
	if ( CncAsyncKeyboardState::isEscapePressed() != 0 ) {
		if ( GBL_CONFIG->getTheApp()->GetBtnEmergenyStop()->IsEnabled() == true ) {
			std::cerr << "SerialCallback: ESCAPE key detected" << std::endl;
			interrupt();
		}
	}
	
	return !isInterrupted();
}
///////////////////////////////////////////////////////////////////
double CncControl::getCurrentFeedSpeed_MM_MIN() {
///////////////////////////////////////////////////////////////////
	return currentFeedSpeed_MM_MIN;
}
///////////////////////////////////////////////////////////////////
void CncControl::postAppPosition(unsigned char pid) {
///////////////////////////////////////////////////////////////////
	static CncLongPosition lastAppPos;
	
	if ( cncConfig->isOnlineUpdateCoordinates() ) {
		// application position
		typedef UpdateManagerThread::Event Event;
		static Event evt;
		
		// app positions are always fron the type major
		// so || pid == PID_XYZ_POS_MAJOR isn't necessary
		// the compairison below is necessary, because this method is also called
		// from the serialCallback(...) which not only detects pos changes
		if ( lastAppPos != curAppPos ) {
			if ( GET_GUI_CTL(mainFrame) )
				GET_GUI_CTL(mainFrame)->umPostEvent(evt.AppPosEvent(pid, getClientId(), getSpeedTypeAsString(), getConfiguredFeedSpeed_MM_MIN(), getCurrentFeedSpeed_MM_MIN(), curAppPos));
		}
	}
	
	lastAppPos.set(curAppPos);
}
///////////////////////////////////////////////////////////////////
void CncControl::postCtlPosition(unsigned char pid) {
///////////////////////////////////////////////////////////////////
	if ( cncConfig->isOnlineUpdateCoordinates() ) {
		// application position
		typedef UpdateManagerThread::Event Event;
		static Event evt;
		
		// a position compairsion isn't necessay here because the serialControllerCallback(...)
		// call this method only on position changes
		if ( GET_GUI_CTL(mainFrame) )
			GET_GUI_CTL(mainFrame)->umPostEvent(evt.CtlPosEvent(pid, getClientId(), getSpeedTypeAsString(), getConfiguredFeedSpeed_MM_MIN(), getCurrentFeedSpeed_MM_MIN(), curCtlPos));
	}
}
///////////////////////////////////////////////////////////////////
bool CncControl::simpleMoveXYToZeroPos(CncDimensions dim) {
///////////////////////////////////////////////////////////////////
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveXYToZeroPos(dim);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::simpleMoveXYZToZeroPos(CncDimensions dim) {
///////////////////////////////////////////////////////////////////
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveXYZToZeroPos(dim);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::simpleMoveZToZeroPos() {
///////////////////////////////////////////////////////////////////
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveZToZeroPos();
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveXYToZeroPos(CncDimensions dim) {
///////////////////////////////////////////////////////////////////
	bool ret = true;
	if ( curAppPos != zeroPos ) {
		int32_t moveX=0, moveY=0;
		moveX = zeroPos.getX() - curAppPos.getX(); 
		moveY = zeroPos.getY() - curAppPos.getY();
		
		if ( dim == CncDimension2D ) {
			ret = moveRelLinearStepsXY(moveX, moveY, false);
			
		} else {
			if ( moveRelLinearStepsXY(moveX, 0, false) == false ) 
				return false;
				
			if ( moveRelLinearStepsXY(0, moveY, false) == false )
				return false;
		}
	}
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveXYZToZeroPos(CncDimensions dim) {
///////////////////////////////////////////////////////////////////
	bool ret = true;
	if ( curAppPos != zeroPos ) {
		int32_t moveX=0, moveY=0, moveZ=0;
		moveX = zeroPos.getX() - curAppPos.getX(); 
		moveY = zeroPos.getY() - curAppPos.getY();
		moveZ = zeroPos.getZ() - curAppPos.getZ();
		
		if ( dim == CncDimension3D ) {
			ret = moveRelLinearStepsXYZ(moveX, moveY, moveZ, false);
			
		} else if ( dim == CncDimension2D ) {
			if ( moveRelLinearStepsXYZ(0, 0, moveZ, false) == false ) 
				return false;
				
			if ( moveRelLinearStepsXYZ(moveX, moveY, 0, false) == false ) 
				return false;
			
		} else {
			if ( moveRelLinearStepsXYZ(moveX, 0, 0, false) == false ) 
				return false;
				
			if ( moveRelLinearStepsXYZ(0, moveY, 0, false) == false ) 
				return false;
				
			if ( moveRelLinearStepsXYZ(0, 0, moveZ, false) == false ) 
				return false;
		}
	}
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToZeroPos() {
///////////////////////////////////////////////////////////////////
	bool ret = true;
	if ( curAppPos != zeroPos ) {
		int32_t moveZ=0;
		moveZ = zeroPos.getZ() - curAppPos.getZ();
		ret = moveRelLinearStepsXYZ(0, 0, moveZ, false);
	}
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveToStartPos() {
///////////////////////////////////////////////////////////////////
	bool ret = true;
	if ( curAppPos != startPos ) {
		int32_t moveX=0, moveY=0;
		moveX = startPos.getX() - curAppPos.getX(); 
		moveY = startPos.getY() - curAppPos.getY();
		moveRelLinearStepsXY(moveX, moveY, false);
	}
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelStepsZ(int32_t z) {
///////////////////////////////////////////////////////////////////
	if ( z == 0 )
		return true;
	// z moves are always linear, as a consequence alreadyRendered can be true
	// but to see the detail positions use true
	return serialPort->processMoveZ(z, false, curAppPos);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelLinearStepsXY(int32_t x1, int32_t y1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	//avoid empty steps
	if ( x1 == 0 && y1 == 0 )
		return true;
	
	return serialPort->processMoveXY(x1, y1, false, curAppPos);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelLinearStepsXYZ(int32_t x1, int32_t y1, int32_t z1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	//avoid empty steps
	if ( x1 == 0 && y1 == 0 && z1 == 0 )
		return true;
	
	return serialPort->processMoveXYZ(x1, y1, z1, alreadyRendered, curAppPos);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelMetricZ(double z) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sZ = z * cncConfig->getCalculationFactZ();
	
	return moveRelStepsZ((int32_t)round(sZ));
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelLinearMetricXY(double x1, double y1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX1 = x1 * cncConfig->getCalculationFactX();
	double sY1 = y1 * cncConfig->getCalculationFactY();
	
	return moveRelLinearStepsXY((int32_t)round(sX1), 
	                            (int32_t)round(sY1),
	                            alreadyRendered);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveRelLinearMetricXYZ(double x1, double y1, double z1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX1 = x1 * cncConfig->getCalculationFactX();
	double sY1 = y1 * cncConfig->getCalculationFactY();
	double sZ1 = z1 * cncConfig->getCalculationFactZ();
	
	return moveRelLinearStepsXYZ((int32_t)round(sX1), 
	                             (int32_t)round(sY1),
	                             (int32_t)round(sZ1),
	                             alreadyRendered);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveAbsMetricZ(double z) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sZ = z * cncConfig->getCalculationFactZ();
	
	return moveRelStepsZ( (int32_t)round(sZ) - curAppPos.getZ() );
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveAbsLinearMetricXY(double x1, double y1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX1 = x1 * cncConfig->getCalculationFactX();
	double sY1 = y1 * cncConfig->getCalculationFactY();
	
	return moveRelLinearStepsXY((int32_t)round(sX1) - curAppPos.getX(), 
	                            (int32_t)round(sY1) - curAppPos.getY(),
	                            alreadyRendered);
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveAbsLinearMetricXYZ(double x1, double y1, double z1, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX1 = x1 * cncConfig->getCalculationFactX();
	double sY1 = y1 * cncConfig->getCalculationFactY();
	double sZ1 = z1 * cncConfig->getCalculationFactZ();
	
	return moveRelLinearStepsXYZ((int32_t)round(sX1) - curAppPos.getX(),
	                             (int32_t)round(sY1) - curAppPos.getY(),
	                             (int32_t)round(sZ1) - curAppPos.getZ(),
	                             alreadyRendered);
}
///////////////////////////////////////////////////////////////////
void CncControl::setToolState(bool defaultStyle) {
///////////////////////////////////////////////////////////////////
	if ( defaultStyle == true ) {
		toolState.setState(CncToolStateControl::red);
	} else {
		if ( toolUpdateState == true ) {
			if ( powerOn == true ) {
				toolState.setState(CncToolStateControl::green);
			} else {
				toolState.setState(CncToolStateControl::red);
			}
		}
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::switchToolOn() {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() )
		return;

	if ( powerOn == false ) { 
		if ( processSetter(PID_ROUTER_SWITCH, 1) ) {
			powerOn = true;
			setToolState();
		}
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::switchToolOff(bool force) {
///////////////////////////////////////////////////////////////////
	if ( isInterrupted() )
		return;

	if ( powerOn == true || force == true ) {
		if ( processSetter(PID_ROUTER_SWITCH, 0) ) {
			powerOn = false;
			setToolState();
		}
	}
}
///////////////////////////////////////////////////////////////////
const int32_t CncControl::getControllerErrorCount(bool withPurge) {
///////////////////////////////////////////////////////////////////
	if ( withPurge == true ) {
		// first of all purge the port to reinitialize the protocol here
		// important in case of previous error sitiuations
		getSerial()->purge();
	}
	
	std::vector<int32_t> list;
	
	if ( isInterrupted() == false )
		getSerial()->processGetter(PID_ERROR_COUNT, list);
		
	if ( list.size() != 1 ){
		if ( isInterrupted() == false ) {
			std::cerr << "CncControl::getControllerErrorCount: Unable to evaluate error count:" << std::endl;
			std::cerr << " Received value count: " << list.size() << std::endl;
		}
		
		return -1;
	}

	return list.at(0);
}
///////////////////////////////////////////////////////////////////
bool CncControl::requestErrorInformation(bool withPurge) {
///////////////////////////////////////////////////////////////////
	if ( withPurge == true ) {
		// first of all purge the port to reinitialize the protocol here
		// important in case of previous error sitiuations
		getSerial()->purge();
	}
	
	bool ret = processCommand(CMD_PRINT_ERRORINFO, std::cerr);
	
	if ( GET_GUI_CTL(controllerErrorInfo) ) {
		if ( GET_GUI_CTL(controllerErrorInfo)->GetItemCount() > 1 )	GET_GUI_CTL(controllerErrorInfo)->SetForegroundColour(*wxRED);
		else														GET_GUI_CTL(controllerErrorInfo)->SetForegroundColour(*wxBLACK);
	}
	
	return ret;
}
///////////////////////////////////////////////////////////////////
const CncLongPosition CncControl::getControllerPos() {
///////////////////////////////////////////////////////////////////
	std::vector<int32_t> list;

	if ( isConnected() == true && isInterrupted() == false )
		getSerial()->processGetter(PID_XYZ_POS, list);
		
	if ( list.size() != 3 ){
		controllerPos.setX(0);
		controllerPos.setY(0);
		controllerPos.setZ(0);
		
		if ( isConnected() == true && isInterrupted() == false ) {
			std::cerr << "CncControl::getControllerPos: Unable to evaluate controllers position:" << std::endl;
			std::cerr << " Received value count: " << list.size() << ", expected: 3" << std::endl;
		}
	} else {
		controllerPos.setX(list.at(0));
		controllerPos.setY(list.at(1));
		controllerPos.setZ(list.at(2));
	}
	
	return controllerPos;
}
///////////////////////////////////////////////////////////////////
const CncLongPosition CncControl::getControllerLimitState() {
///////////////////////////////////////////////////////////////////
	std::vector<int32_t> list;

	if ( isInterrupted() == false )
		getSerial()->processGetter(PID_LIMIT, list);
	
	if ( list.size() != 3 ){
		if ( isInterrupted() == false ) {
			std::cerr << "CncControl::getControllerLimitState: Unable to evaluate controllers limit state:" << std::endl;
			std::cerr << " Received value count: " << list.size() << ", expected: 3" << std::endl;
		}
	} else {
		return {list.at(0), list.at(1), list.at(2)};
	}
	
	return {0, 0, 0};
}
///////////////////////////////////////////////////////////////////
bool CncControl::validateAppAgainstCtlPosition() {
///////////////////////////////////////////////////////////////////
	CncLongPosition ctlPos = getControllerPos();
	return ( curAppPos == ctlPos );
}
///////////////////////////////////////////////////////////////////
void CncControl::updateCncConfigTrace() {
///////////////////////////////////////////////////////////////////
	typedef UpdateManagerThread::Event Event;
	static Event evt;
	
	if ( GET_GUI_CTL(mainFrame) )
		GET_GUI_CTL(mainFrame)->umPostEvent(evt.ConfigUpdateEvent());
}
///////////////////////////////////////////////////////////////////
void CncControl::enableStepperMotors(bool s) {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( isInterrupted() )
		return;
		
	if ( isConnected() == false )
		return;
	
	bool ret = false;
	if ( s == true ) 	ret = processCommand("E", std::cout);
	else				ret = processCommand("e", std::cout);
	
	if ( ret == false ) {
		std::cerr << "CncControl::enableStepperMotors" << std::endl;
		std::cerr << " Error while enabling stepper motors. State=" << s << std::endl;
		return;
	}
	
	if ( GET_GUI_CTL(motorState) )
		GET_GUI_CTL(motorState)->Check(s);
}
///////////////////////////////////////////////////////////////////
void CncControl::evaluateLimitState() {
///////////////////////////////////////////////////////////////////
	CncLongPosition ls = getControllerLimitState();
	evaluateLimitState(ls.getX(), ls.getY(), ls.getZ());
}
///////////////////////////////////////////////////////////////////
wxString& CncControl::getLimitInfoString(wxString& ret) {
///////////////////////////////////////////////////////////////////
	return limitStates.getLimitInfoString(ret);
}
///////////////////////////////////////////////////////////////////
void CncControl::evaluateLimitState(long x, long y, long z) {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	limitStates.setXLimit(x);
	limitStates.setYLimit(y);
	limitStates.setZLimit(z);
	
	displayLimitState(GET_GUI_CTL(xMinLimit), limitStates.getXMinLimit());
	displayLimitState(GET_GUI_CTL(xMaxLimit), limitStates.getXMaxLimit());
	displayLimitState(GET_GUI_CTL(yMinLimit), limitStates.getYMinLimit());
	displayLimitState(GET_GUI_CTL(yMaxLimit), limitStates.getYMaxLimit());
	displayLimitState(GET_GUI_CTL(zMinLimit), limitStates.getZMinLimit());
	displayLimitState(GET_GUI_CTL(zMaxLimit), limitStates.getZMaxLimit());
	
	limitStates.displayLimitState();
}
///////////////////////////////////////////////////////////////////
void CncControl::displayLimitState(wxStaticText* ctl, bool value) {
///////////////////////////////////////////////////////////////////
	if ( ctl != NULL ) {
		if ( value == true ) {
			ctl->SetLabel(wxString("1"));
			ctl->SetBackgroundColour(wxColour(255,128,128));
			ctl->SetForegroundColour(*wxWHITE);
			
		} else {
			ctl->SetLabel(wxString("0"));
			ctl->SetBackgroundColour(wxColour(181,230,29));
			ctl->SetForegroundColour(*wxBLACK);

		}
		ctl->Refresh();
		ctl->Update();
	}
}
///////////////////////////////////////////////////////////////////
bool CncControl::meassureDimension(const char axis, wxCheckBox* min, wxCheckBox* max, double& result) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	
	double maxSteps 	= (axis != 'Z' ? -450.0 : -100.0); // mm
	bool ret 			= false;
	long minPos 		= 0l;
	long maxPos 		= 0l;
	result 				= -DBL_MAX; // invalid Values
	
	if ( min ) min->SetValue(false);
	if ( max ) max->SetValue(false);
	
	if ( prepareSimpleMove() == true ) {
		
		// move to min position
		switch ( axis ) {
			case 'X': 	ret = moveRelLinearMetricXY(maxSteps, 0.0, true);
						if ( ret ) 
							minPos = curCtlPos.getX();
						break;
						
			case 'Y': 	ret = moveRelLinearMetricXY(0.0, maxSteps, true);
						if ( ret ) 
							minPos = curCtlPos.getY();
						break;
						
			case 'Z': 	ret = moveRelMetricZ(maxSteps);
						if ( ret ) 
							minPos = curCtlPos.getZ();
						break;
		}
		
		// move to max position
		if ( ret ) {
			if ( min ) min->SetValue(true);
			maxSteps *= -1;
			
			switch ( axis ) {
				case 'X': 	ret = moveRelLinearMetricXY(maxSteps, 0.0, true);
							if ( ret ) 
								maxPos = curCtlPos.getX();
							break;
							
				case 'Y': 	ret = moveRelLinearMetricXY(0.0, maxSteps, true);
							if ( ret ) 
								maxPos = curCtlPos.getY();
							break;
							
				case 'Z': 	ret = moveRelMetricZ(maxSteps);
							if ( ret ) 
								maxPos = curCtlPos.getZ();
							break;
			}
		}
		
		// free end position
		if ( ret ) {
			if ( max ) max->SetValue(true);
			
			switch ( axis ) {
				case 'X': 	ret = moveRelLinearMetricXY(-endSwitchStepBackMertic, 0.0, true);
							break;
							
				case 'Y': 	ret = moveRelLinearMetricXY(0.0, -endSwitchStepBackMertic, true);
							break;
							
				case 'Z': 	ret = moveRelMetricZ(-endSwitchStepBackMertic);
							break;
			}
		}
		
		// calculate result
		if ( ret ) {

			switch ( axis ) {
				case 'X': 	result = (maxPos - minPos) * cncConfig->getDisplayFactX();
							break;
							
				case 'Y': 	result = (maxPos - minPos) * cncConfig->getDisplayFactY();
							break;
							
				case 'Z': 	result = (maxPos - minPos) * cncConfig->getDisplayFactZ();
							break;
			}
		}
	}
	
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveXToMinLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionX() * (-1);
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(maxSteps, 0.0, true);
		if (ret)
			ret = moveRelLinearMetricXY(+endSwitchStepBackMertic, 0.0, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveXToMaxLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionX();
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(maxSteps, 0.0, true);
		if (ret)
			ret = moveRelLinearMetricXY(-endSwitchStepBackMertic, 0.0, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveYToMinLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionY() * (-1);
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(0.0, maxSteps, true);
		if (ret)
			ret = moveRelLinearMetricXY(0.0, +endSwitchStepBackMertic, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveYToMaxLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionY();
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(0.0, maxSteps, true);
		if (ret)
			ret = moveRelLinearMetricXY(0.0, -endSwitchStepBackMertic, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToMinLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	double maxSteps = cncConfig->getMaxDimensionZ() * (-1);
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelMetricZ(maxSteps);
		if (ret)
			ret = moveRelMetricZ(+endSwitchStepBackMertic);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToMaxLimit() {
// This function move the max distiance and will be latest stopped by the end switch
// However, the PC and controller postions are not equal at the end!
// the call of reconfigureSimpleMove(true) will correct that
///////////////////////////////////////////////////////////////////
	double maxSteps = cncConfig->getMaxDimensionZ();
	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelMetricZ(maxSteps);
		if (ret)
			ret = moveRelMetricZ(-endSwitchStepBackMertic);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveXToMid() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionX();
	bool ret = false;

	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(maxSteps, 0.0, true);
		if ( ret )
			ret = moveRelLinearMetricXY(-maxSteps/2, 0.0, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveYToMid() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionY();

	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelLinearMetricXY(0.0, maxSteps, true);
		if ( ret )
			ret = moveRelLinearMetricXY(0.0, -maxSteps/2, true);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::moveZToMid() {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double maxSteps = cncConfig->getMaxDimensionZ();

	bool ret = false;
	if ( prepareSimpleMove() == true ) {
		ret = moveRelMetricZ(maxSteps);
		if ( ret )
			ret = moveRelMetricZ(-maxSteps/2);
	}
	reconfigureSimpleMove(ret);
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::manualSimpleMoveMetric(double x, double y, double z, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX = x * cncConfig->getCalculationFactX();
	double sY = y * cncConfig->getCalculationFactY();
	double sZ = z * cncConfig->getCalculationFactZ();
	
	return manualSimpleMoveSteps((int32_t)round(sX), (int32_t)round(sY), (int32_t)round(sZ), alreadyRendered);
}
///////////////////////////////////////////////////////////////////
bool CncControl::manualSimpleMoveMetric3D(double x, double y, double z, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	wxASSERT(cncConfig);
	double sX = x * cncConfig->getCalculationFactX();
	double sY = y * cncConfig->getCalculationFactY();
	double sZ = z * cncConfig->getCalculationFactZ();
	
	return manualSimpleMoveSteps3D((int32_t)round(sX), (int32_t)round(sY), (int32_t)round(sZ), alreadyRendered);
}
///////////////////////////////////////////////////////////////////
bool CncControl::manualSimpleMoveSteps(int32_t x, int32_t y, int32_t z, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	bool ret = true;

	if ( x != 0 || y != 0 ) {
		ret = false;
		if ( prepareSimpleMove(false) == true ) {
			ret = moveRelLinearStepsXY(x, y, alreadyRendered );
			if ( limitStates.hasLimit() ) {
				
				if ( x != 0 ) {
					if ( limitStates.isXLimitStateValid() && limitStates.getXMinLimit() )
						ret = moveRelLinearMetricXY(+endSwitchStepBackMertic, 0.0, false);
						
					if ( limitStates.isXLimitStateValid() && limitStates.getXMaxLimit() )
						ret = moveRelLinearMetricXY(-endSwitchStepBackMertic, 0.0, false);
				}
				
				if ( y != 0 ) {
					if ( limitStates.isYLimitStateValid() && limitStates.getYMinLimit() )
						ret = moveRelLinearMetricXY(0.0, +endSwitchStepBackMertic, false);
						
					if ( limitStates.isYLimitStateValid() && limitStates.getYMaxLimit() )
						ret = moveRelLinearMetricXY(0.0, -endSwitchStepBackMertic, false);
				}
			}
				
			reconfigureSimpleMove(ret);
		}
	}
	
	if ( z != 0 ) {
		ret = false;
		if ( prepareSimpleMove(false) == true ) {
			ret = moveRelStepsZ(z);
			if ( ret && limitStates.hasLimit() ) {
				
				if ( limitStates.isZLimitStateValid() && limitStates.getZMinLimit() )
					ret = moveRelMetricZ(+endSwitchStepBackMertic);
					
				if ( limitStates.isZLimitStateValid() && limitStates.getZMaxLimit() )
					ret = moveRelMetricZ(-endSwitchStepBackMertic);
			}
				
			reconfigureSimpleMove(ret);
		}
	}
	
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::manualSimpleMoveSteps3D(int32_t x, int32_t y, int32_t z, bool alreadyRendered) {
///////////////////////////////////////////////////////////////////
	bool ret = true;
	
	if ( x != 0 || y != 0 || z != 0 ) {
		ret = false;
		if ( prepareSimpleMove(false) == true ) {
			ret = moveRelLinearStepsXYZ(x, y, z, alreadyRendered );
			
			if ( limitStates.hasLimit() ) {
				
				if ( x != 0 ) {
					if ( limitStates.isXLimitStateValid() && limitStates.getXMinLimit() )
						ret = moveRelLinearMetricXYZ(+endSwitchStepBackMertic, 0.0, 0.0, false);
						
					if ( limitStates.isXLimitStateValid() && limitStates.getXMaxLimit() )
						ret = moveRelLinearMetricXYZ(-endSwitchStepBackMertic, 0.0, 0.0, false);
				}
				
				if ( y != 0 ) {
					if ( limitStates.isYLimitStateValid() && limitStates.getYMinLimit() )
						ret = moveRelLinearMetricXYZ(0.0, +endSwitchStepBackMertic, 0.0,false);
						
					if ( limitStates.isYLimitStateValid() && limitStates.getYMaxLimit() )
						ret = moveRelLinearMetricXYZ(0.0, -endSwitchStepBackMertic, 0.0, false);
				}
				
				if ( z != 0 ) {
					if ( limitStates.isZLimitStateValid() && limitStates.getZMinLimit() )
						ret = moveRelLinearMetricXYZ(0.0, 0.0, +endSwitchStepBackMertic, false);
						
					if ( limitStates.isZLimitStateValid() && limitStates.getZMaxLimit() )
						ret = moveRelLinearMetricXYZ(0.0, 0.0, -endSwitchStepBackMertic, false);
				}
			}
				
			reconfigureSimpleMove(ret);
		}
	}
	
	return ret;
}
///////////////////////////////////////////////////////////////////
bool CncControl::prepareSimpleMove(bool enaleEventHandling) {
///////////////////////////////////////////////////////////////////
	initNextDuration();
	cncConfig->setAllowEventHandling(enaleEventHandling);
	activatePositionCheck(false);
	enableStepperMotors(true);
	
	// currently no checks implemented, if checks necessary do it here
	return true;
}
///////////////////////////////////////////////////////////////////
void CncControl::reconfigureSimpleMove(bool correctPositions) {
///////////////////////////////////////////////////////////////////
	enableStepperMotors(false);
	activatePositionCheck(true);
	resetDurationCounter();
	
	if ( validateAppAgainstCtlPosition() == false && correctPositions == true ) {
		curAppPos = getControllerPos();
	}
}
///////////////////////////////////////////////////////////////////
bool CncControl::hasControllerConfigControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	return ( GET_GUI_CTL(controllerConfig) != NULL );
}
///////////////////////////////////////////////////////////////////
void CncControl::clearControllerConfigControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerConfigControl() ) 
		GET_GUI_CTL(controllerConfig)->DeleteAllItems();
}
///////////////////////////////////////////////////////////////////
void CncControl::appendPidKeyValueToControllerConfig(int pid, const char* key, const char* value) {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerConfigControl() ) {
		DcmItemList rows;

		DataControlModel::addNumKeyValueRow(rows, pid, key, value);
		GET_GUI_CTL(controllerConfig)->Freeze();
		for (DcmItemList::iterator it = rows.begin(); it != rows.end(); ++it) {
			GET_GUI_CTL(controllerConfig)->AppendItem(*it);
		}
		GET_GUI_CTL(controllerConfig)->Thaw();
	}
}
///////////////////////////////////////////////////////////////////
bool CncControl::hasControllerPinControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	return ( GET_GUI_CTL(controllerPinReport) != NULL );
}
///////////////////////////////////////////////////////////////////
bool CncControl::hasControllerErrorControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	return ( GET_GUI_CTL(controllerErrorInfo) != NULL );
}
///////////////////////////////////////////////////////////////////
void CncControl::clearControllerPinControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerErrorControl() )
		GET_GUI_CTL(controllerPinReport)->DeleteAllItems();
}
///////////////////////////////////////////////////////////////////
void CncControl::clearControllerErrorControl() {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerErrorControl() ) 
		GET_GUI_CTL(controllerErrorInfo)->DeleteAllItems();
}
///////////////////////////////////////////////////////////////////
void CncControl::appendNumKeyValueToControllerPinInfo(const char* desc, int pin, int type, int mode, int value) {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerErrorControl() ) {
		DcmItemList rows;

		DataControlModel::addPinReportRow(rows, desc, pin, type, mode, value);
		GET_GUI_CTL(controllerPinReport)->Freeze();
		for (DcmItemList::iterator it = rows.begin(); it != rows.end(); ++it) {
			GET_GUI_CTL(controllerPinReport)->AppendItem(*it);
		}
		GET_GUI_CTL(controllerPinReport)->Thaw();
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::appendNumKeyValueToControllerErrorInfo(int num, int code, const char* key, const char* value) {
///////////////////////////////////////////////////////////////////
	wxASSERT(guiCtlSetup);
	
	if ( hasControllerErrorControl() ) {
		DcmItemList rows;
		
		DataControlModel::addNumKeyValueRow(rows, num, code, key, value);
		guiCtlSetup->controllerErrorInfo->Freeze();
		for (DcmItemList::iterator it = rows.begin(); it != rows.end(); ++it) {
			GET_GUI_CTL(controllerErrorInfo)->AppendItem(*it);
		}
		GET_GUI_CTL(controllerErrorInfo)->Thaw();
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::updatePreview3D(bool force) {
///////////////////////////////////////////////////////////////////
	if ( IS_GUI_CTL_NOT_VALID(motionMonitor) )
		return;
		
	if ( force == true ) {
		GET_GUI_CTL(motionMonitor)->display();
		return;
	}
	
	// Online drawing coordinates
	if ( cncConfig->isOnlineUpdateDrawPane() ) {
		static wxDateTime tsLastUpdate = wxDateTime::UNow();
		
		if ( (wxDateTime::UNow() - tsLastUpdate).GetMilliseconds() >= cncConfig->getUpdateInterval() ) {
			GET_GUI_CTL(motionMonitor)->display();
			tsLastUpdate = wxDateTime::UNow();
		}
	}
}
///////////////////////////////////////////////////////////////////
void CncControl::sendIdleMessage() {
///////////////////////////////////////////////////////////////////
	if ( getSerial() == NULL )
		return;
		
	if ( getSerial()->isCommandActive() == true ) {
		return;
	}
	
	//clog <<  wxDateTime::UNow().FormatTime() << " - idle,  delay:" << getStepDelay() << endl;
	getSerial()->processIdle();
}
