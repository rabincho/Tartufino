#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "periodic.h"
#include "encoder.h"
//#include "can.h"
#include <linux/can.h>
#include <linux/can/raw.h>

static int sock_can; /* can raw socket  */
static FILE *file; /* file descriptor for the output file */
static int period_ms;
static const char* can_interface;
static pthread_t query_th, save_th;

static void cleanup_handler(void *arg)
{
	pthread_cancel(query_th);
	pthread_join(query_th, NULL);
	
	pthread_cancel(save_th);
	pthread_join(save_th, NULL);
	
	close(sock_can);
	fclose(file);

	printf("Encoders:      Disabled\n");
}

static void *query_encoder(void *args)
{
	int oldstate;

	uint8_t frame[8];
	frame[0] = 0x42;
	frame[1] = 0x40;
	frame[2] = 0x22;
	frame[3] = 0x00;
	frame[4] = 0x00;
	frame[5] = 0x00;
	frame[6] = 0x00;
	frame[7] = 0x00;

	struct can_frame Tmsg;
	Tmsg.can_dlc = 8;
	int i;
	for (i = 0; i < Tmsg.can_dlc; i++) Tmsg.data[i] = frame[i];
	int left_motor = 0x601;
	int right_motor = 0x602;
	int nbytes;

	struct periodic_task *task = start_periodic_timer(1000, 
		1000 * period_ms);
	
    while (1) {
		wait_next_activation(task);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		Tmsg.can_id = left_motor;
		if ((nbytes = write(sock_can, &Tmsg, sizeof(Tmsg))) != 
			sizeof(Tmsg)) {
			printf("Error sending message through CANbus!!!\n");
		}
		Tmsg.can_id = right_motor;
		if ((nbytes = write(sock_can, &Tmsg, sizeof(Tmsg))) != 
			sizeof(Tmsg)) {
			printf("Error sending message through CANbus!!!\n");
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	}
	pthread_exit(NULL);
}

static void *save_encoder(void *args)
{
	int oldstate;

	struct can_frame m;
	int nbytes;
	struct timeval tv;
	uint32_t timestamp_ms;
	int encoder;

	while (1) {
		/* read next message */
		if ((nbytes = read(sock_can, &m, sizeof(m))) < 0) {
			printf("Error sending message through CANbus!!!\n");
		}

		/* check if packet is encoder data */
		if ((m.data[1] != 0x40) || (m.data[2] != 0x22))
			break;

		/* get timestamp of the message */
		ioctl(sock_can, SIOCGSTAMP, &tv);
		timestamp_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

		/* get encoder value */
        encoder = m.data[4] 
		          + (m.data[5] << 8) 
		          + (m.data[6] << 16)
		          + (m.data[7] << 24);

		/* write to file */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		if (m.can_id == 1410)
			fprintf(file, "\t%d %d %d\n", m.can_id, timestamp_ms, encoder);
		else
			fprintf(file, "%d %d %d\n", m.can_id, timestamp_ms, encoder);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	}
	pthread_exit(NULL);
}

void *encoder(void *args)
{
	int oldstate;
	
	/* Get index of the file */
	struct encoder_th_params *params = (struct encoder_th_params *) args;
	int file_index = params->file_index;
	period_ms = params->period_ms;
	can_interface = params->can_interface;
	
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

	/* open CAN socket */
	struct sockaddr_can addr;
	struct ifreq ifr;
	
	if ((sock_can = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("socket");
		pthread_exit(NULL);
	}
	
	strcpy(ifr.ifr_name, can_interface);
	ioctl(sock_can, SIOCGIFINDEX, &ifr);
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	if (bind(sock_can, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		pthread_exit(NULL);
	}
	
	/* Filter the can messages */
	struct can_filter rfilter[2];
	rfilter[0].can_id   = 0x581;
	rfilter[0].can_mask = CAN_SFF_MASK;
	rfilter[1].can_id   = 0x582;
	rfilter[1].can_mask = CAN_SFF_MASK;

	setsockopt(sock_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

	/* Open file descriptor */
	char *file_name = 0;
	
	if (file_index > 10) {
		file_name = (char*) malloc(21);
		sprintf(file_name, "exp_encoder/file%d.csv", file_index);
	}
	else {
		file_name = (char*) malloc(22);
		sprintf(file_name, "exp_encoder/file0%d.csv", file_index);
	}
	
	file = fopen(file_name, "w");

	pthread_create(&query_th, NULL, query_encoder, NULL);
	pthread_create(&save_th, NULL, save_encoder, NULL);

	pthread_cleanup_push(cleanup_handler, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	
	printf("Encoders:      Enabled\n");

	pthread_join(query_th, NULL);
	pthread_join(save_th, NULL);

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}
