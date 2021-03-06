#ifndef GCODE_PATH_HANDLER_BASE_H
#define GCODE_PATH_HANDLER_BASE_H

#include "PathHandlerBase.h"
#include "CncControl.h"
#include "CncPosition.h"
#include "GCodeBlock.h"

class GCodePathHandlerBase : public PathHandlerBase {
	
	public:
		enum CutterCompensation { CC_OFF, CC_STATIC_LEFT, CC_STATIC_RIGHT, CC_DYNAMIC_LEFT, CC_DYNAMIC_RIGHT };
		
		GCodePathHandlerBase();
		virtual ~GCodePathHandlerBase();
		
		GCodePathHandlerBase::CutterCompensation getCutterCompensationMode() { return cutterCompMode; }
		void setCutterCompensationMode(GCodePathHandlerBase::CutterCompensation ccm) { cutterCompMode = ccm; }
		
		int getToolLengthOffsetId() { return toolLengthOffsetId; }
		void setToolLengthOffsetId(int tloi) { toolLengthOffsetId = tloi; }
		
		virtual void prepareWork(); 
		virtual void finishWork();
		
		bool processRapidLinearMove(GCodeBlock& gcb);
		bool processLinearMove(GCodeBlock& gcb);
		bool processArcMove(GCodeBlock& gcb, bool sweep);
		bool processDwell(GCodeBlock& gcb);
		bool moveToOrigin(GCodeBlock& gcb);
		
		virtual void switchToolState(bool state) = 0;
		void swichtToolOn()  { switchToolState(true); }
		void swichtToolOff() { switchToolState(false); }
		
	protected:
		
		virtual bool processDwellIntern(int64_t microseconds) { return true; }
		virtual bool processLinearMove(bool alreadyRendered) = 0;
		virtual bool changeCurrentFeedSpeedXYZ(CncSpeed s, double value = 0.0) = 0;
		virtual void prepareWorkImpl() = 0;
		virtual void finishWorkImpl() = 0;
		
	private:
		
		CutterCompensation cutterCompMode;
		int toolLengthOffsetId;
		
		void updateCurrentPosition(GCodeBlock& gcb);
};

#endif