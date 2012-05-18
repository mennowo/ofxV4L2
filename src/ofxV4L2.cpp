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

#include "ofxV4L2.h"

ofxV4L2::ofxV4L2()
{
	v4l2framerate = 0;
}

bool ofxV4L2::settings(int id, int val)
{
	struct v4l2_control argptr;
	argptr.id = id;
	argptr.value = val;
	return xioctl(fd, VIDIOC_S_CTRL, &argptr);
}

unsigned char * ofxV4L2::getPixels(void)
{
	return image;
}

bool ofxV4L2::setDesiredFramerate(int fr)
{
	if(v4l2framerate != 0)
	{
		fprintf(stdout, "Framerate cannot be set after initialisation. Please call 'setDesiredFramerate()' before 'initGrabber()'\n");
		return -1;
	}
	else
	{
		// this only sets the desired rate: init_device() tries to actually set this and gives feedback through stdout
		v4l2framerate = fr;
		return 0;
	}
}

bool ofxV4L2::isNewFrame()
{
	return newframe;
}

void ofxV4L2::initGrabber(const char * devname, int iomethod, int cw, int ch)
{
	// set input/output method
	io = iomethod;
	// set resolution used for capture
	camWidth = cw;
	camHeight = ch;
	image = new unsigned char[camWidth * camHeight];
	dev_name = devname;

	// check if framerate was set externally
	if(v4l2framerate == 0)
	{
		fprintf(stdout, "Framerate for device %s not set. Using default value of 30 fps.\n", dev_name);
		v4l2framerate = 30;
	}

	open_device(dev_name);
    init_device();
    start_capturing();

}

// error output
void ofxV4L2::errno_exit(const char * s)
{
	fprintf (stderr, "%s error %d, %s\n", s, errno, strerror (errno));
	exit (EXIT_FAILURE);
}

// x input output control
int ofxV4L2::xioctl (int fd, int request, void * arg)
{
    int r;
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}

void ofxV4L2::process_image(const void * p, int length)
{
	int row, col;
	unsigned char * y = p;
	for (row=0; row<camHeight; row++)
	{
		for (col=0; col<camWidth; col++)
		{
			 image[col + row * camWidth] = *(y + 2*(col + (row*camWidth)));
		}
	}
}

void ofxV4L2::grabFrame(void)
{
    struct v4l2_buffer buf;
    unsigned int i;

	// what is this?
	fd_set fds;
	struct timeval tv;
	int r;

	// and this?
	FD_ZERO (&fds);
	FD_SET (fd, &fds);

	/* Timeout. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	// and this?
	// what does this do? outcome: app exits cause r == 0
	r = select (fd + 1, &fds, NULL, NULL, &tv);
	r = 1;

	if (-1 == r)
	{
		if (EINTR == errno)
			return 0;
		errno_exit ("select");
	}

	if (0 == r)
	{
		fprintf (stderr, "select timeout\n");
		exit (EXIT_FAILURE);
	}

    switch (io)
    {
        case IO_METHOD_READ:
            if (-1 == read (fd, buffers[0].start, buffers[0].length))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;
                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */
                    default:
                        errno_exit ("read");
                }
            }

            process_image (buffers[0].start, buffers[0].length);
            break;

        case IO_METHOD_MMAP:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
						newframe = false;
                        return 0;
                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */
                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            assert (buf.index < n_buffers);

            process_image(buffers[buf.index].start, buffers[0].length);
            newframe = true;

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */
                        /* fall through */
                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start
                    && buf.length == buffers[i].length)
                    break;

            assert (i < n_buffers);

            process_image ((void *) buf.m.userptr, buf.length);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");

            break;
    }
}

void ofxV4L2::stop_capturing (void)
{
    enum v4l2_buf_type type;

    switch (io) {
    case IO_METHOD_READ:
        /* Nothing to do. */
        break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
                errno_exit ("VIDIOC_STREAMOFF");

        break;
    }
}

void ofxV4L2::start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io)
    {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
            {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = i;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                    errno_exit ("VIDIOC_STREAMON");

            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
            {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_USERPTR;
                buf.index       = i;
                buf.m.userptr   = (unsigned long) buffers[i].start;
                buf.length      = buffers[i].length;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;
    }
}

void ofxV4L2::uninit_device(void)
{
    unsigned int i;

    switch (io)
    {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    errno_exit ("munmap");
                break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free (buffers[i].start);
            break;
    }

    free (buffers);
}

void ofxV4L2::init_read(unsigned int buffer_size)
{
    buffers = calloc (1, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);

    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}

void ofxV4L2::init_mmap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s does not support memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2)
    {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
                 dev_name);
        exit (EXIT_FAILURE);
    }

    buffers = calloc (req.count, sizeof (*buffers));

    if (!buffers)
    {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}

void ofxV4L2::init_userp(unsigned int buffer_size)
{
    struct v4l2_requestbuffers req;
    unsigned int page_size;

    page_size = getpagesize ();
    buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s does not support user pointer i/o\n", dev_name);
                exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    buffers = calloc (4, sizeof (*buffers));

    if (!buffers) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
            buffers[n_buffers].length = buffer_size;
            buffers[n_buffers].start = memalign (/* boundary */ page_size,
                                                 buffer_size);

            if (!buffers[n_buffers].start) {
                    fprintf (stderr, "Out of memory\n");
                    exit (EXIT_FAILURE);
            }
    }
}

void ofxV4L2::init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
        if (EINVAL == errno)
        {
            fprintf (stderr, "%s is no V4L2 device\n", dev_name);
            exit (EXIT_FAILURE);
        }
        else
        {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }

    struct v4l2_streamparm streamparm;
//    struct v4l2_fract *tpf = &streamparm.parm.capture.timeperframe;
    //streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int set, ret;// = xioctl(fd, VIDIOC_G_PARM, &streamparm);




	cap.capabilities |= V4L2_CAP_TIMEPERFRAME;
	streamparm.parm.capture.capability = cap.capabilities;

	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparm.parm.capture.timeperframe.numerator = 1;
	streamparm.parm.capture.timeperframe.denominator = v4l2framerate;
	set = xioctl(fd,VIDIOC_S_PARM,&streamparm);
	ret = xioctl(fd,VIDIOC_G_PARM,&streamparm);

	if(set == 0 && ret == 0)
		fprintf(stdout, "Framerate for device %s set at: %d fps\n", dev_name, streamparm.parm.capture.timeperframe.denominator);
	else if(set < 0 && ret == 0)
		fprintf(stdout, "Framerate for device %s could not be set. Framerate is now: %d fps\n", dev_name, streamparm.parm.capture.timeperframe.denominator);
	else
		fprintf(stdout, "Framerate for device %s could not read.", dev_name);

//    tpf->numerator = ap->time_base.num;
//    tpf->denominator = ap->time_base.den;





    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf (stderr, "%s is no video capture device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    switch (io)
    {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE))
            {
                fprintf (stderr, "%s does not support read i/o\n", dev_name);
                exit (EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING))
            {
                fprintf (stderr, "%s does not support streaming i/o\n", dev_name);
                exit (EXIT_FAILURE);
            }
            break;
    }


    /* Select video input, video standard and tune here. */


    CLEAR (cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop))
        {
            switch (errno)
            {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    }
    else
    {
        /* Errors ignored. */
    }


    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = camWidth;
    fmt.fmt.pix.height      = camHeight;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
            errno_exit ("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (io)
    {
        case IO_METHOD_READ:
            init_read (fmt.fmt.pix.sizeimage);
            break;

        case IO_METHOD_MMAP:
            init_mmap ();
            break;

        case IO_METHOD_USERPTR:
            init_userp (fmt.fmt.pix.sizeimage);
            break;
    }
}

void ofxV4L2::close_device(void)
{
    if (-1 == close (fd))
        errno_exit ("close");
    fd = -1;
}

void ofxV4L2::open_device(const char * devname)
{
	dev_name = devname;
    struct stat st;
    // check if the device exists
    if (-1 == stat (dev_name, &st))
    {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
    // check if the device is a real device
    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);
    }
    // open the device
    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    fprintf(stdout, "Opened device: %s\n", dev_name);
    //check for errors
    if (-1 == fd)
    {
        fprintf (stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}

ofxV4L2::~ofxV4L2()
{
	stop_capturing();
	uninit_device();
	close_device();
}
