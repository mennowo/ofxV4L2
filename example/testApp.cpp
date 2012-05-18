#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup()
{

	// some global settings
	camWidth = 640;
	camHeight = 480;

	// this must be called before init (otherwise fprintf will tell you so)
	// note that high framerates will only function properly if the usb has enough bandwidth
	// for example, a ps3 eye cam at 60 fps will only function when it has full USB 2.0 bandwidth available
	v4l2cam1.setDesiredFramerate(60);

	// use this to set appropriate device and capture method
	v4l2cam1.initGrabber("/dev/video0", IO_METHOD_MMAP, camWidth, camHeight);

	// some initial settings
	set_gain = 2.0;
	set_autogain = true;

	// rudimentary settings implementation: each settings needs a seperate call to the settings method
	v4l2cam1.settings(ofxV4L2_AUTOGAIN, set_autogain);
	v4l2cam1.settings(ofxV4L2_GAIN, set_gain);

	// we use a texture because the ofxV4L2 class has no draw method (yet)
	// we use GL_LUMINANCE because the ofxV4L2 class supports only grayscale (for now)
    camtex.allocate(camWidth, camHeight, GL_LUMINANCE);
}

//--------------------------------------------------------------
void testApp::update()
{
	v4l2cam1.grabFrame();
	if(v4l2cam1.isNewFrame())
	{
		camtex.loadData(v4l2cam1.getPixels(), camWidth, camHeight, GL_LUMINANCE);
	}
}

//--------------------------------------------------------------
void testApp::draw()
{
	ofBackground(150, 150, 150);
	char text[256];
	sprintf(text, "fps: %f", ofGetFrameRate());
	ofSetHexColor(0xffffff);
	ofDrawBitmapString(text, 20, 20);
	camtex.draw(20, 40);
	ofSetHexColor(0x333333);
	sprintf(text, "use 'a' to toggle autogain (value: %d)\nuse 'g' to raise gain and 'G' to lower gain (value: %3f)", set_autogain, set_gain);
	ofDrawBitmapString(text, 20, 60 + camHeight);
}

//--------------------------------------------------------------
void testApp::keyPressed(int key)
{
	if(key == 'a')
		v4l2cam1.settings(ofxV4L2_AUTOGAIN, set_autogain = !set_autogain);
	if(key == 'g')
		v4l2cam1.settings(ofxV4L2_GAIN, set_gain += 0.05);
	if(key == 'G')
		v4l2cam1.settings(ofxV4L2_GAIN, set_gain -= 0.05);
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){

}
