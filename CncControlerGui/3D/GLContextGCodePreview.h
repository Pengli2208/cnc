#ifndef OPENGL_CONTEXT_GCODE_PREVIEW_H
#define OPENGL_CONTEXT_GCODE_PREVIEW_H

#include "3D/GLContextPathBase.h"

/////////////////////////////////////////////////////////////////
class GLContextGCodePreview : public GLContextCncPathBase {
	
	public:
		GLContextGCodePreview(wxGLCanvas* canvas);
		virtual ~GLContextGCodePreview();
		
		virtual const char* getContextName() { return "GLContextGCodePreview"; };
		
	protected:
		virtual void initContext();
		virtual void determineProjection(int w, int h);
		virtual void determineModel();
		virtual GLViewPort* createViewPort();
};

#endif