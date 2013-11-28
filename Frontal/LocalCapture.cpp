#include "OCVCapture.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "periodic.h"
#include "LocalCapture.h"

#include <linux/can.h>
#include <linux/can/raw.h>

using namespace cv;
using namespace std;

/* Variables for the names of the saved frames */
static int num_frames = 0;

/* Variables for the log file */
static FILE *proc_file;
static FILE *capt_file;
uint32_t timestamp_ms;

/* Mutex and condition variable to control the access to the new frame */
pthread_mutex_t lock_new_frame;
pthread_cond_t cond_new_frame;
int new_frame = 0;
int grabbed_frames = 0;

/* Variables to identify the camera */
OCVCapture camera;

/* Structures for the captured frames */
Mat gray = Mat::zeros(480, 640, CV_8U);
Mat edge = Mat::zeros(480, 640, CV_8U);

void stopCapture()
{
	fclose(capt_file);

	pthread_mutex_destroy(&lock_new_frame);
	pthread_cond_destroy(&cond_new_frame);
	
	camera.closeCamera();

	printf("Local Camera:  Disabled\n");
}

void pauseProcessing()
{
	pthread_mutex_unlock(&lock_new_frame);
	
	fclose(proc_file);
}

void *capture_frames(void *args)
{
	/* Avoid to cancel the thread during this period */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	
	/* Set up the capture device */
	camera.configureCapture((char*)"/dev/video0", 640, 480, 5, (char*)"YUYV");

	/* Open the capture device */
	camera.openCamera();

	/* Open file descriptor */
	capt_file = fopen("frames/capture.csv", "w");

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	
	/* Verify if the device is active */
	if (!camera.isOpen()) {
		cerr << "ERROR: Failed to open the local camera" << endl;
		pthread_exit(NULL);
	}

	/* The first several frames tend to come out black */
	for (int i = 0; i < 20; ++i) {
		camera.grabFrame(timestamp_ms);
		usleep(1000);
	}

	/* Information about the local capture parameters */
	cout << "Local Camera:  Enabled (" << camera.getWidth() << "x" << 
		camera.getHeight() << " - " << camera.getFrameRate() << " fps)" << endl;

	/* Initialize the mutex for the new captured frame */
	pthread_mutex_init(&lock_new_frame, NULL);
	pthread_cond_init(&cond_new_frame, NULL);
		
	/* Capture the frames in a gray-scale format */
	while (1) {
		
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		pthread_mutex_lock(&lock_new_frame);

		/* Grab the frame from the device */
		camera.grabFrame(timestamp_ms);
		grabbed_frames++;
		
		fprintf(capt_file, "%d %d\n", grabbed_frames, timestamp_ms);

		/* Convert the frame to gray-scale */
		camera.gray(gray);

		/* Notify to other threads that the new frame is ready */
		new_frame = 1;
		pthread_cond_signal(&cond_new_frame);
		pthread_mutex_unlock(&lock_new_frame);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		
		/* TODO: Define the priority to the keyboard and the capture*/
		usleep(5000);
	}

	pthread_exit(NULL);
}

void *process_frames(void *args)
{
	/* Get index of the file */
	struct video_th_params *params = (struct video_th_params *) args;
	int file_index = params->file_index;

	/* Open file descriptor */
	char file_name[50];
	sprintf(file_name, "frames/file%u.csv", file_index);
	
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	proc_file = fopen(file_name, "w");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		
	char name_edge[50];
	char full_name_edge[50];
	
	/* Process the gray-scale frames */
	while (1) {

		/* Define the name of the new frame */
		num_frames++;
		sprintf(name_edge, "frame%u.jpg", num_frames);
		sprintf(full_name_edge, "frames/frame%u.jpg", num_frames);

		pthread_mutex_lock(&lock_new_frame);

		/* Block until a new frame is available */
		while (!new_frame)
			pthread_cond_wait(&cond_new_frame, &lock_new_frame);
				
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		
		/* Apply the edge filter and save the resulting frame */
		//Canny(gray, edge, 0, 30, 3);
		//imwrite(full_name_edge, edge);
		fprintf(proc_file, "%s %d\n", name_edge, timestamp_ms);

		new_frame = 0;
		pthread_mutex_unlock(&lock_new_frame);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		usleep(5000);
	}

	pthread_exit(NULL);
}	
