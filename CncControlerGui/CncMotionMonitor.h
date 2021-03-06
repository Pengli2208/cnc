#ifndef CNC_MOTION_MONITOR_H
#define CNC_MOTION_MONITOR_H

#include <wx/glcanvas.h>
#include <wx/timer.h>

#include "CncPosition.h"
#include "CncCommon.h"
#include "3D/VerticeData.h"
#include "3D/GLContextPathBase.h"

wxDECLARE_EVENT(wxEVT_MOTION_MONITOR_TIMER, wxTimerEvent);

class GL3DOptions;
class CncVectiesListCtrl;

/////////////////////////////////////////////////////////////
class CncMotionMonitor : public wxGLCanvas {

	public:

		///////////////////////////////////////////////////
		struct Flags {
			bool positionMarker 	= true;
			bool drawFlyPath		= true;
			bool autoScaling		= true;
			
			wxColour flyColour		= *wxYELLOW;
			wxColour workColour		= *wxWHITE;
			
			// fill more flags here if neccessary
		};
		
		// constructor
		CncMotionMonitor(wxWindow *parent, int *attribList = NULL);
		virtual ~CncMotionMonitor();
		
		// is used from global key down hook, that's the reason why it is public
		void onKeyDown(wxKeyEvent& event);
		
		// interface
		void enable(bool state);
		void clear();
		void display();
		void appendVertice(const GLI::VerticeLongData& vd);
		void centerViewport();
		void resetRotation();
		
		void setModelType(const GLContextBase::ModelType mt);
		
		void viewTop() 		{ view(GLContextBase::ViewMode::V2D_TOP); }
		void viewBottom() 	{ view(GLContextBase::ViewMode::V2D_BOTTOM); }
		void viewLeft() 	{ view(GLContextBase::ViewMode::V2D_LEFT); }
		void viewRight()	{ view(GLContextBase::ViewMode::V2D_RIGHT); }
		void viewFront() 	{ view(GLContextBase::ViewMode::V2D_FRONT); }
		void viewRear() 	{ view(GLContextBase::ViewMode::V2D_REAR); }

		void viewIso1() 	{ view(GLContextBase::ViewMode::V3D_ISO1); }
		void viewIso2() 	{ view(GLContextBase::ViewMode::V3D_ISO2); }
		void viewIso3() 	{ view(GLContextBase::ViewMode::V3D_ISO3); }
		void viewIso4() 	{ view(GLContextBase::ViewMode::V3D_ISO4); }
		
		void setAngleX(int a) { monitor->getModelRotation().setAngleX(a); display(); }
		void setAngleY(int a) { monitor->getModelRotation().setAngleY(a); display(); }
		void setAngleZ(int a) { monitor->getModelRotation().setAngleZ(a); display(); }
		
		int getAngleX() { return monitor->getModelRotation().angleX(); }
		int getAngleY() { return monitor->getModelRotation().angleY(); }
		int getAngleZ() { return monitor->getModelRotation().angleZ(); }
		
		int getCameraEyeAngle() { return monitor->getCameraPosition().getCurXYPlaneEyeAngle(); }
		
		unsigned int calculateScaleDisplay(unsigned int height);
		
		long fillVectiesListCtr(long curCount, CncVectiesListCtrl* listCtrl);
		
		// usage:
		// getFlags().positionMarker 	= false;
		// getFlags().smoothing			= true;
		CncMotionMonitor::Flags& getFlags() { return flags; }
		
		void pushProcessMode();
		void popProcessMode();
		
		void showOptionDialog();
		
		void reconstruct();
		
		void tracePathData(std::ostream& s);
		
		// camera handling
		enum CameraMode{ CM_OFF, CM_CLOCKWISE, CM_COUNTER_CLOCKWISE};
		void rotateCamera(int angle);
		int getCameraRotationSpeed() { return cameraRotationSpeed; }
		void setCameraRotationSpeed(int speed);
		void cameraRotationTimerHandler(CncMotionMonitor::CameraMode cm);
		
		// zoom handling
		float getZoom() { return zoom; }
		void setZoom(float z) { zoom = z; monitor->setZoomFactor(zoom); }
		
		// smoothing
		bool isSmoothingEnabled() { return monitor->isSmoothingEnabled(); }
		void enableSmoothing(bool state=true) { monitor->enableSmoothing(state); }
		
		// draw type
		GLContextCncPathBase::DrawType getDrawType() { return monitor->getDrawType(); }
		void setDrawType(GLContextCncPathBase::DrawType t) { monitor->setDrawType(t); }
		
		// bound box
		bool isBoundBoxEnabled() { return monitor->isBoundBoxEnabled(); }
		void enableBoundBox(bool enable) { monitor->enableBoundBox(enable); }
		const wxColour& getBoundBoxColour() { return monitor->getBoundBoxColour(); }
		void setBoundBoxColour(const wxColour& c) { monitor->setBoundBoxColour(c); }
		
		// client id
		long getCurrentClientId() { return currentClientID; }
		void setCurrentClientId(long id) { currentClientID = id; monitor->setCurrentClientId(id); }
		void resetCurrentClientId() { setCurrentClientId(-1L); }
		
	protected:
	
		CncMotionMonitor::Flags		flags;
		
		GLContextCncPathBase* 		monitor;
		GLContextCncPathBase* 		testCube;
		GL3DOptions*				optionDlg;
		
		wxTimer cameraRotationTimer;
		int cameraRotationStepWidth;
		int cameraRotationSpeed;
		
		bool isShown;
		
		float zoom;
		
		bool currentClientID;
		
		void onPaint(wxPaintEvent& event);
		void onMouse(wxMouseEvent& event);
		void onSize(wxSizeEvent& event);
		void onEraseBackground(wxEraseEvent& event);
		void onCameraRotationTimer(wxTimerEvent& event);
		
		void onPaintRotatePaneX3D(wxPaintEvent& event);
		void onPaintRotatePaneY3D(wxPaintEvent& event);
		void onPaintRotatePaneZ3D(wxPaintEvent& event);
		void onPaintScalePane3D(wxPaintEvent& event);
		
		void view(GLContextBase::ViewMode fm);
		
	private:
		
		inline void appendVertice(long id, float x, float y, float z, GLI::GLCncPathVertices::CncMode cm);
		inline void onPaint();
		
		inline void onPaintRotatePane3D(wxPanel* panel, int angle);
		
		wxDECLARE_NO_COPY_CLASS(CncMotionMonitor);
		wxDECLARE_EVENT_TABLE();
};

#endif