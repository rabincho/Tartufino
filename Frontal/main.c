#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <pthread.h>

#include "periodic.h"
#include "keyboard.h"
#include "main.h"
#include "encoder.h"
#include "LocalCapture.h"
#include "MotorsServiceClient.h"

#define V 0.3 /* Initial speed for the robot (m/s) */
#define step_speed 0.02 /* Step to increase/decrease the speed */
#define r 0.0475 /* Radius of the wheels */
#define L 0.275 /* Base wheel of the robot*/

/* Variables to identify the socket */
static int sock_can;
static const char* can_interface = "can0";

/* Variables to identify the threads */
pthread_t capture_th, process_th, receive_th;

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
	//rfilter.can_id   = 0x584;
	//rfilter.can_mask = CAN_SFF_MASK;

	//setsockopt(sock_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
	
	pthread_create(&receive_th, NULL, receive_info, NULL);  
	
	return;
}

void sendCommand(char cmd)
{
	uint8_t frame[8];
	frame[0] = 0x91;
	frame[1] = 0x92;
	frame[3] = 0x00;
	frame[4] = 0x00;
	frame[5] = 0x00;
	frame[6] = 0x00;
	frame[7] = 0x00;

	if (cmd == 'c') 
		frame[2] = 0x66;
	else if (cmd == 's')
		frame[2] = 0x55;
	else if (cmd == 'p')
		frame[2] = 0x99;
	else if (cmd == 'e')
		frame[2] = 0x33;

	struct can_frame Tmsg;
	Tmsg.can_id = 0x604;
	Tmsg.can_dlc = 8;
	int nbytes, i;
	for (i = 0; i < Tmsg.can_dlc; i++) Tmsg.data[i] = frame[i];
	
	if ((nbytes = write(sock_can, &Tmsg, sizeof(Tmsg))) != sizeof(Tmsg)) {
		printf("Error sending message through CANbus!!!\n");
	}
}

void *receive_info(void *args)
{
	struct can_frame m;
	int nbytes;

	while (1) {

		/* Read next message */
		if ((nbytes = read(sock_can, &m, sizeof(m))) < 0) {
			printf("Error sending message through CANbus!!!\n");
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (m.data[0] == 0x93) {
			int width = m.data[1] + (m.data[2] << 8) + (m.data[3] << 16);
			int height = m.data[4] + (m.data[5] << 8) + (m.data[6] << 16);
			int fps = m.data[7];

			/* Information about the remote capture parameters */
			printf("Remote Camera: Enabled (%dx%d - %d fps)\n", width, 
				height, fps);

		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}

	pthread_exit(NULL);
}

int main()
{
	float speedL = V;
	float speedR = V;
	char c = '\0';

	printf("************************\n");
	printf("   Starting Tartufino   \n");
	printf("************************\n\n");

	/* Enable the communication with the remote camera */
	enableCommunication();

	/* Start capturing the frames from the remote camera */
	sendCommand('c');

	/* Start capturing the frames from the local camera */
	pthread_create(&capture_th, NULL, capture_frames, NULL);
	
	/* Encoder service settings */
	pthread_t encoder_th;
	int encoder_active = 0;
	int index_encoder_file = 0;
	struct encoder_th_params enc_params;

	/* Image processing service settings */
	int processing_active = 0;
	int index_video_file = 0;
	struct video_th_params proc_params;

	sleep(8);
	
	/* Enable the Motors */
	/* TODO: rethink the interface */
	MotorsServiceClient();

	/* Enable the Telecommand Piloting */
	enterInputMode();

	while (1) {
		/* Read the pressed key */
		c = readKey();
		//printf("read: %d\n", c);

		/* Start piloting the robot */
		if (c == 115) {

			/* Start processing the frames from the remote camera */
			sendCommand('s');

			/* Start processing the frames from the local camera */ 
			if (!processing_active) {
				processing_active = 1;
				index_video_file++;
				proc_params.file_index = index_video_file;
				pthread_create(&process_th, NULL, process_frames, &proc_params);
			}
			
			/* Start obtaining the readings from the encoder */
			if (!encoder_active) {
				encoder_active = 1;
				index_encoder_file++;
				enc_params.file_index = index_encoder_file;
				enc_params.period_ms = 10;
				enc_params.can_interface = can_interface;			
				pthread_create(&encoder_th, NULL, encoder, &enc_params);
			}
			
			/* Set the base speed */
			speedL = V;
			speedR = V;
			setMotorRightSpeed(speedR, NULL);
			setMotorLeftSpeed(speedL, NULL);

			//printf("Start\n");
		}

		/* Finish piloting the robot */
		if (c == 101) {
			
			/* Stop capturing & processing the frames from the remote camera */
			sendCommand('e');
			printf("Remote Camera: Disabled\n");

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

			/* Stop obtaining the readings from the encoder */
			if (encoder_active) {
				encoder_active = 0;
				pthread_cancel(encoder_th);
				pthread_join(encoder_th, NULL);
			}
			 
			/* Set the speed to zero */
			setMotorLeftSpeed(0, NULL);
			setMotorRightSpeed(0, NULL);
			
			printf("Motors:        Disabled\n");
			//printf("Stop\n");
			
			break;
		}

		/* Turn to the left */
		if (c == 68) {
			
			/* Increase the right speed and decrease the left one*/
			speedL -= step_speed;
			speedR += step_speed;
			setMotorLeftSpeed(speedL, NULL);
			setMotorRightSpeed(speedR, NULL);

			//printf("Turn left\n");
		}

		/* Turn to the right */
		if (c == 67) {
			
			/* Increase the left speed and decrease the right one*/
			speedL += step_speed;
			speedR -= step_speed;
			setMotorLeftSpeed(speedL, NULL);
			setMotorRightSpeed(speedR, NULL);

			//printf("Turn right\n");
		}

		/* Go forward */
		if (c == 65) {
			/* Set the base speed to go straightforward in the desired direction */
			speedL = V;
			speedR = V;
			setMotorLeftSpeed(speedL, NULL);
			setMotorRightSpeed(speedR, NULL);

			//printf("Go forward\n");
		}

		/* Pause the robot */
		if (c == 66) {
			
			/* Stop processing the frames from the remote camera */  
			sendCommand('p');

			/* Stop processing the frames from the local camera */ 
			if (processing_active) {
				processing_active = 0;
				pthread_cancel(process_th);
				pthread_join(process_th, NULL);
				pauseProcessing();
			}
			
			/* Stop obtaining the readings from the encoder */
			if (encoder_active) {
				encoder_active = 0;
				pthread_cancel(encoder_th);
				pthread_join(encoder_th, NULL);
			}
			
			/* Stops the robot in the current position*/
			setMotorLeftSpeed(0, NULL);
			setMotorRightSpeed(0, NULL);

			//printf("Pause\n");
		}
	}
	
	pthread_cancel(receive_th);
	pthread_join(receive_th, NULL);
	
	/* Disable the Telecommand Piloting */
	leaveInputMode();
	
	close(sock_can);
	
	sleep(2);

	printf("\n************************\n");
	printf("   Tartufino Finished   \n");
	printf("************************\n\n");

	return 0;
}
