#include <iostream>
#include "3D/GLContextCncPath.h"

#ifdef __DARWIN__
    #include <OpenGL/glu.h>
	#include <OpenGL/glut.h>
	#include <OpenGL/freeglut.h>
#else
    #include <GL/glu.h>
	#include <GL/glut.h>
	#include <GL/freeglut.h>
#endif

/////////////////////////////////////////////////////////////////
OpenGLContextCncPath::OpenGLContextCncPath(wxGLCanvas* canvas) 
: OpenGLContextCncPathBase(canvas)
{
/////////////////////////////////////////////////////////////////
	// do something here on demand
}
/////////////////////////////////////////////////////////////////
OpenGLContextCncPath::~OpenGLContextCncPath() {
/////////////////////////////////////////////////////////////////
	// do something here on demand
}
/////////////////////////////////////////////////////////////////
GLViewPort* OpenGLContextCncPath::createViewPort() {
/////////////////////////////////////////////////////////////////
	// determine the destort type
	return new GLViewPort(GLViewPort::VPT_Undistored);
}
/////////////////////////////////////////////////////////////////
void OpenGLContextCncPath::initContext() {
/////////////////////////////////////////////////////////////////
	// do context specific initalization here
}
/////////////////////////////////////////////////////////////////
void OpenGLContextCncPath::determineProjection(int w, int h) {
/////////////////////////////////////////////////////////////////
	OpenGLContextBase::determineProjection(w, h);
}
/////////////////////////////////////////////////////////////////
void OpenGLContextCncPath::determineModel() {
/////////////////////////////////////////////////////////////////
	// draw the scene
	 OpenGLContextCncPathBase::determineModel();
}
