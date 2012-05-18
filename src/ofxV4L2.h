/**
 *
 * ofxV4L2 - a V4L2 implementation for Openframeworks
 *
 * Copyright (c) 2012 Menno van der Woude
 *
 * Date created: 12-05-2012
 * Written by: Menno van der Woude <mennowo@gmail.com>
 * Version: 1.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * DESCRIPTION
 *
 * This class implements basic V4L2 functionality in C++, specifically Openframeworks.
 * The methods have been named to resemble the ofGrabber class that is embedded in
 * Openframeworks.
 * The methods that use the V4L2 API are based on the example that is contained
 * in the API documentation: http://v4l2spec.bytesex.org/spec/capture-example.html
 *
 * CHANGELOG
 *
 * Version 1.0
 *
 * The functionality is quite basic. Only grayscale is captured at this time.
 * Capturing is done using the YUYV pixel format. Settings of a device can be
 * changed one at a time with the settings() method. The default framerate is
 * 30 fps, which can be changed by calling setDesiredFramerate().
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

// grabbing modes
#define IO_METHOD_READ 		0
#define IO_METHOD_MMAP 		1
#define IO_METHOD_USERPTR 	2

// setting defines (can be used as id value in call to 'settings()'
// this list is just for ease of use inside an OF app
#define ofxV4L2_BRIGHTNESS 			V4L2_CID_BRIGHTNESS
#define ofxV4L2_CONTRAST			V4L2_CID_CONTRAST
#define ofxV4L2_SATURATION			V4L2_CID_SATURATION
#define ofxV4L2_HUE					V4L2_CID_HUE
#define ofxV4L2_AUTO_WHITE_BALANCE	V4L2_CID_AUTO_WHITE_BALANCE
#define ofxV4L2_DO_WHITE_BALANCE	V4L2_CID_DO_WHITE_BALANCE
#define ofxV4L2_RED_BALANCE			V4L2_CID_RED_BALANCE
#define ofxV4L2_BLUE_BALANCE		V4L2_CID_BLUE_BALANCE
#define ofxV4L2_GAMMA				V4L2_CID_GAMMA
#define ofxV4L2_WHITENESS			V4L2_CID_WHITENESS
#define ofxV4L2_EXPOSURE			V4L2_CID_EXPOSURE
#define ofxV4L2_AUTOGAIN			V4L2_CID_AUTOGAIN
#define ofxV4L2_GAIN				V4L2_CID_GAIN
#define ofxV4L2_HFLIP				V4L2_CID_HFLIP
#define ofxV4L2_VFLIP				V4L2_CID_VFLIP
#define ofxV4L2_HUE_AUTO 			V4L2_CID_HUE_AUTO
#define ofxV4L2_WHITE_BALANCE_TEMPERATURE V4L2_CID_WHITE_BALANCE_TEMPERATURE
#define ofxV4L2_SHARPNESS 			V4L2_CID_SHARPNESS
#define ofxV4L2_BACKLIGHT_COMPENSATION V4L2_CID_BACKLIGHT_COMPENSATION
#define ofxV4L2_CHROMA_AGC 			V4L2_CID_CHROMA_AGC
#define ofxV4L2_COLOR_KILLER 		V4L2_CID_COLOR_KILLER
#define ofxV4L2_COLORFX 			V4L2_CID_COLORFX
#define ofxV4L2_AUTOBRIGHTNESS 		V4L2_CID_AUTOBRIGHTNESS
#define ofxV4L2_BAND_STOP_FILTER 	V4L2_CID_BAND_STOP_FILTER
#define ofxV4L2_ROTATE 				V4L2_CID_ROTATE
#define ofxV4L2_BG_COLOR 			V4L2_CID_BG_COLOR
#define ofxV4L2_CHROMA_GAIN 		V4L2_CID_CHROMA_GAIN

class ofxV4L2
{
    public:

		// constructor (just used to initiate variable v4l2framerate to 0
		ofxV4L2();

        void grabFrame(void);
		bool isNewFrame();
        unsigned char * getPixels(void);

		// allows one to set properties of capture device
		// for each setting, a seperate function call is needed
		// list available options: see the list of defines, these are the appropriate id values
		// example: cam1.settings(ofxV4L2_GAIN, 100);
		// returns 0 if succesfull (xioctl return value)
		// this is quite a rudimentary approach to settings; an all-encompassing gui would be better
		bool settings(int id, int val);

		// setDesiredFramerate should be called before initGrabber
		bool setDesiredFramerate(int fr);
        void initGrabber(const char * devname, int iomethod, int cw, int ch);
        // three below are called inside initGrabber()
        void open_device(const char * devname);
        void init_device(void);
		void start_capturing(void);

		// methods called inside the destructor
        void stop_capturing (void);
        void uninit_device(void);
        void close_device (void);

		// methods called inside other methods
        void errno_exit (const char * s);
        int xioctl(int fd, int request, void * arg);
        void process_image(const void * p, int length);
        void init_userp (unsigned int buffer_size);
        void init_mmap (void);
        void init_read(unsigned int buffer_size);

		// destructor
        ~ofxV4L2();

    private:

		// used to store frames
        struct buffer
        {
            void *                  start;
            size_t                  length;
        };

        unsigned char * image;		// used to store captured frame for use in an app
        int camWidth, camHeight;	// must be set before calling init_device()
        char * dev_name;			// device name
        int io;						// input method
        int fd;						// file descriptor (used to address the device)
        struct buffer * buffers;	// pointer to buffers (no idea what this exactly means, neither how it is used)
        int n_buffers;				// ??
		int v4l2framerate;			// desired framerate
        bool newframe;				// used to check if a new frame is there

};
