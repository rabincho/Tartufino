#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <sys/time.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include <pthread.h>

#include "periodic.h"
#include "OCVCapture.h"

#define SCALE 0.5

using namespace cv;
using namespace std;

/* Variables for the names of the saved frames */
static int num_frames = 0;

/* Variables to identify the socket */
static int sock_can;
static const char* can_interface = "can0";

/* Variables for the log file */
static FILE *capt_file;
static FILE *proc_file;
uint32_t timestamp_ms;

static pthread_t capture_th, process_th;

/* Mutex and condition variable to control the access to the new frame */
pthread_mutex_t lock_new_frame;
pthread_cond_t cond_new_frame;
int new_frame = 0;
int grabbed_frames = 0;

/* Variables to identify the camera */
OCVCapture camera;

/* Structure for the arguments of the threads */
struct video_th_params {
	int file_index;
};

/* Structures for the captured frames */
Mat gray = Mat::zeros(240, 320, CV_8U);
Mat edge = Mat::zeros(240*SCALE, 320*SCALE, CV_8U);

static void stopCapture()
{
	fclose(capt_file);

	pthread_mutex_destroy(&lock_new_frame);
	pthread_cond_destroy(&cond_new_frame);
	
	camera.closeCamera();
	
	close(sock_can);

	cout << "Capture:  Disabled" << endl;
}

static void pauseProcessing()
{
	pthread_mutex_unlock(&lock_new_frame);
	
	fclose(proc_file);
}

static void sendParameters()
{
    int width = camera.getWidth();
    int height = camera.getHeight();
    int fps = camera.getFrameRate();
    
    uint8_t frame[8];

	frame[0] = 0x93;
	
	frame[1] = (uint8_t) (width & 0x000000FF);
	width = width >> 8;
	frame[2] = (uint8_t) (width & 0x000000FF);
	width = width >> 8;
	frame[3] = (uint8_t) (width & 0x000000FF);

	frame[4] = (uint8_t) (height & 0x000000FF);
	height = height >> 8;
	frame[5] = (uint8_t) (height & 0x000000FF);
	height = height >> 8;
	frame[6] = (uint8_t) (height & 0x000000FF);

	frame[7] = (uint8_t) (fps & 0x000000FF);

	struct can_frame Tmsg;
	Tmsg.can_id = 0x603;
	Tmsg.can_dlc = 8;
	int nbytes, i;
	
	for (i = 0; i < Tmsg.can_dlc; i++) 
		Tmsg.data[i] = frame[i];
	
	if ((nbytes = write(sock_can, &Tmsg, sizeof(Tmsg))) != sizeof(Tmsg)) {
		cout << "Error sending message through CANbus!!!" << endl;
	}
}

static void *capture_frames(void *args)
{
	/* Avoid to cancel the thread during this period */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	
	/* Set up the capture device */
	camera.configureCapture((char*)"/dev/video0", 320, 240, 30, (char*)"YUYV");

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

	/* Information about the capture parameters */
	sendParameters();
	
	/* Information about the remote capture parameters */
	cout << "Capture:  Enabled (" << camera.getWidth() << "x" << 
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

static void *process_frames(void *args)
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
		//resize(gray, edge, edge.size(), 0, 0, INTER_AREA);
		//Canny(edge, edge, 0, 30, 3);
		//imwrite(full_name_edge, edge);
		fprintf(proc_file, "%s %d\n", name_edge, timestamp_ms);

		new_frame = 0;
		pthread_mutex_unlock(&lock_new_frame);

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		usleep(5000);
	}

	pthread_exit(NULL);
}	

static void enableCommunication()
{
	/* Open CAN socket */
	struct sockaddr_can addr;
	struct ifreq ifr;
	
	if ((sock_can = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("socket");
		return;
	}
	
	strcpy(ifr.ifr_name, can_interface);
	ioctl(sock_can, SIOCGIFINDEX, &ifr);
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	
	if (bind(sock_can, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		return;
	}

	/* Filter the can messages */
	//struct can_filter rfilter;
	//rfilter.can_id   = 0x583;
	//rfilter.can_mask = CAN_SFF_MASK;

	//setsockopt(sock_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

	return;
}

int main(int argc, char** argv)
{
	struct can_frame m;
	int nbytes;
	
	cout << "************************" << endl;
	cout << "   Starting Tartufino   " << endl;
	cout << "************************" << endl << endl;

	enableCommunication();
	
	/* Image processing service settings */
	int processing_active = 0;
	int index_video_file = 0;
	struct video_th_params proc_params;

  	while (1) {

		/* Read next message */
		if ((nbytes = read(sock_can, &m, sizeof(m))) < 0) {
			cout << "Error sending message through CANbus" << endl;
		}
		
		if ((m.data[0] == 0x91) && (m.data[1] == 0x92)) {

			/* The pilot sent the setup command */
			if (m.data[2] == 0x66) {
				pthread_create(&capture_th, NULL, capture_frames, NULL);
			}

			/* The pilot sent the start command (s)*/
			else if (m.data[2] == 0x55) {

				/* Start processing the frames from the camera */ 
				if (!processing_active) {
					processing_active = 1;
					index_video_file++;
					proc_params.file_index = index_video_file;

					pthread_create(&process_th, NULL, process_frames, &proc_params);
				}
			}
		
			/* Pause the program */
			else if (m.data[2] == 0x99) {

				/* Stop processing the frames from the camera */ 
				if (processing_active) {
					processing_active = 0;
					pthread_cancel(process_th);
					pthread_join(process_th, NULL);
					pauseProcessing();
				}
			}
		
			/* Stop the program */
			else if (m.data[2] == 0x33) {

				/* Stop capturing & processing the frames from the local camera */ 
				if (processing_active) //{
					//processing_active = 0;
					pthread_cancel(process_th);
				pthread_cancel(capture_th);
				
				if (processing_active)
					pthread_join(process_th, NULL);
				pthread_join(capture_th, NULL);

				if (processing_active)
					pauseProcessing();
				stopCapture();
				//}
				processing_active = 0;
			
				break;
			}
		}
	}

	usleep(20000);

	cout << endl << "************************" << endl;
	cout << "   Tartufino Finished   " << endl;
	cout << "************************" << endl << endl;

	return 0;
}
