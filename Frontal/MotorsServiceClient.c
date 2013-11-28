#include <net/if.h>
#include <sys/ioctl.h>
#include <string.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "MotorsServiceClient.h"
//#include "canopen.h"
#include "socket_ids.h"
#include "canbus_ids.h"

#define CPR 64000 /* counts per revolution of the encoder */
#define C_WHEEL 0.298 /* meters - Diameter 0.095 meters */

/* Variables to identify the socket */
static int sock_can;
static const char* can_interface = "can0";

/**Status varibales**/
static struct Motors* _motorsClient;
static struct MotorsAck* _lastReq;
static struct MotorsAck noRequest;

static int status_updated;

/**Private Methods**/
/*Implementation at the bottom of the file*/
float from_encoder_to_rpm(int encoder);
float from_rpm_to_mps(float rpm);
float from_mps_to_rpm(float mps);
int from_rpm_to_encoder(float rpm);

/**Public methods**/

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
	struct can_filter rfilter;
	rfilter.can_id   = 0x583;
	rfilter.can_mask = CAN_SFF_MASK;

	setsockopt(sock_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
	
	return;
}

int MotorsServiceClient(){
	_motorsClient = (struct Motors*) malloc( sizeof(struct Motors) );
	_motorsClient->statusLeft = 0;
	_motorsClient->statusRight = 0;
	_motorsClient->encoderLeft = 0;
	_motorsClient->encoderRight = 0;

	status_updated = 0;
  
	noRequest.idSender=0;
	noRequest.ack=1;
	_lastReq=&noRequest;
  
	//activateMotorsServiceClient();
	
	enableCommunication();

	printf("Motors:        Enabled\n");
  
	return 0;
}

struct Motors readMotors(){
	status_updated = 0;
	return *_motorsClient;
}

void sendMsg(__u32 ID, __u8 DATA[], int len)
{
  /* Procedure to send a CAN message */
  struct can_frame Tmsg;
  int nbytes;
  int i, errno;

  Tmsg.can_id = ID;
  Tmsg.can_dlc = len;

  for(i=0;i<len;i++) Tmsg.data[i] = DATA[i];

#ifdef VERB
    printf("--> 0x%03x  %d   ",Tmsg.can_id,Tmsg.can_dlc);
    for(i=0;i<Tmsg.can_dlc;i++) printf("0x%02x  ",Tmsg.data[i]);
    printf("\n");    
#endif

    if ((nbytes = write(sock_can, &Tmsg, sizeof(Tmsg))) != sizeof(Tmsg)) {
      errno = -1;
      printf("Error sending message through CANbus!!!\n");
    }
}

int setMotorLeftSpeed(float speed_mps, struct MotorsAck* ack){
	/*if request pending not accept any new one*/
	if( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	int encoder = from_rpm_to_encoder(from_mps_to_rpm(speed_mps));
	//printf("left encoder: %d\n", encoder);

	uint8_t frame[8];
	frame[0] = 0x22;
	frame[1] = 0x41;
	frame[2] = 0x23;
	frame[3] = 0x00;
	frame[4] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[5] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[6] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[7] = (uint8_t) (encoder & 0x000000FF);

	if(ack!=NULL){
		//Driver motor doesn't include requester in the reply
		ack->idSender=0; 
		ack->type = ACK_SET_SPEED;
		ack->ack=0;
		_lastReq=ack;
	}

	sendMsg(CAN_SENDTO+CAN_ID_MotorLeft, frame, 8);

	return 0;
}

int setMotorRightSpeed(float speed_mps, struct MotorsAck* ack) {
	/*if request pending not accept any new one*/
	if( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	int encoder = from_rpm_to_encoder(from_mps_to_rpm(speed_mps));
	//printf("right encoder: %d\n", encoder);

	uint8_t frame[8];
	frame[0] = 0x22;
	frame[1] = 0x41;
	frame[2] = 0x23;
	frame[3] = 0x00;
	frame[4] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[5] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[6] = (uint8_t) (encoder & 0x000000FF);
	encoder = encoder >> 8;
	frame[7] = (uint8_t) (encoder & 0x000000FF);

	if(ack!=NULL){
		//Driver motor doesn't include requester in the reply
		ack->idSender=0; 
		ack->type = ACK_SET_SPEED;
		ack->ack=0;
		_lastReq=ack;
	}
  
	sendMsg(CAN_SENDTO+CAN_ID_MotorRight, frame, 8);

	return 0;
}

int enableMotors(uint8_t idSender, struct MotorsAck* ack) {
	/*if request pending not accept any new one*/
	if( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	uint8_t frame[8];
	frame[0] = idSender;
	frame[1] = 0x00; //write
	frame[2] = 0x03; //both motors enabled

	if(ack!=NULL){
		ack->idSender=idSender;
		ack->type = ACK_STATUS;
		ack->ack=0;
		_lastReq=ack;
	}

	sendMsg(CAN_SENDTO+CAN_ID_Motors, frame, 3);

	return 0;
}

int disableMotors(uint8_t idSender, struct MotorsAck* ack){
	/*if request pending not accept any new one*/
	if( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	uint8_t frame[8];
	frame[0] = idSender;
	frame[1] = 0x00; //write
	frame[2] = 0x00; //both motors disabled

	if(ack!=NULL){
		ack->idSender=idSender;
		ack->type = ACK_STATUS;
		ack->ack=0;
		_lastReq=ack;
	} 

	sendMsg(CAN_SENDTO+CAN_ID_Motors, frame, 3);

	return 0;
}

int updatestatusMotors(uint8_t idSender, struct MotorsAck* ack){
	/*if request pending not accept any new one*/
	if ((_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	uint8_t frame[8];
	frame[0] = idSender;
	frame[1] = 0x01; //read

	if(ack!=NULL){
		ack->idSender=idSender;
		ack->type = ACK_STATUS;
		ack->ack=0;
		_lastReq=ack;
	}

	sendMsg(CAN_SENDTO+CAN_ID_Motors, frame, 2);

	return 0;
}

int updatespeedMotorLeft(struct MotorsAck* ack){
	/*if request pending not accept any new one*/
	if( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	uint8_t frame[8];
	frame[0] = 0x42;
	frame[1] = 0x69;
	frame[2] = 0x60;
	frame[3] = 0x00;
	frame[4] = 0x00;
	frame[5] = 0x00;
	frame[6] = 0x00;
	frame[7] = 0x00;

	if(ack!=NULL){
		//Driver motor doesn't include requester in the reply
		ack->idSender=0; 
		ack->type = ACK_GET_SPEED;
		ack->ack=0;
		_lastReq=ack;
	}

	sendMsg(CAN_SENDTO+CAN_ID_MotorLeft, frame, 8);

	return 0;
}

int updatespeedMotorRight(struct MotorsAck* ack){
	/*if request pending not accept any new one*/
	if ( (_lastReq->ack==0) & (ack!=NULL) )
		return -1;

	uint8_t frame[8];
	frame[0] = 0x42;
	frame[1] = 0x69;
	frame[2] = 0x60;
	frame[3] = 0x00;
	frame[4] = 0x00;
	frame[5] = 0x00;
	frame[6] = 0x00;
	frame[7] = 0x00;

	if(ack!=NULL){
		//Driver motor doesn't include requester in the reply
		ack->idSender=0; 
		ack->type = ACK_GET_SPEED;
		ack->ack=0;
		_lastReq=ack;
	}

	sendMsg(CAN_SENDTO+CAN_ID_MotorRight, frame, 8);

	return 0;
}

int isMotorsUpdated(){
	return status_updated;
}

int cancelMotorsPendingRequest(){
	_lastReq=&noRequest;
	return 0;
	/*TODO adding possibility of handling more requests*/
}

int MotorsServiceClienthandle(uint8_t* frame, int lenght, int sender){
  
	/*FLEX status response*/
	if ( (lenght==3) && (sender==CAN_ID_Motors) ) {
		if( (frame[0]==_lastReq->idSender) && (frame[1]==1) ){
			uint8_t temp = frame[2];
			_motorsClient->statusLeft = ((temp==3) || (temp==2));
			_motorsClient->statusRight = ((temp==3) || (temp==1));
			status_updated = 1;
			_lastReq->ack=1;
		}
		else{
			if(frame[1]==0)
				_lastReq->ack=0;
			else
				return -1;
		}
		return 0;
	}

	/*Left Motor Driver response*/
	if( (lenght==8) && (sender==CAN_ID_MotorLeft) ) {
		/*Set speed*/
		if(_lastReq->type==ACK_SET_SPEED){
			if( (frame[0]==0x60) && (frame[1]==0x41) && (
				frame[2]==0x23) && (frame[3]==0x00) ) {
				/* TODO check if the speed (present in the response) 
				 * is the actual speed required
				 */
				_lastReq->ack=1;
			}
			else{
				_lastReq->ack=0;
			}
			return 0;
		}
		/*Get speed*/
		if(_lastReq->type==ACK_GET_SPEED) {
			if( (frame[0]==0x43) && (frame[1]==0x69) && 
				(frame[2]==0x60) && (frame[3]==0x00) ) {
				int temp = frame[7];
				temp = temp << 8;
				temp = temp + frame[6];
				temp = temp << 8;
				temp = temp + frame[5];
				temp = temp << 8;
				temp = temp + frame[4];
				_motorsClient->encoderLeft = temp;
				_motorsClient->rpmLeft = 
					from_encoder_to_rpm(_motorsClient->encoderLeft);
				_motorsClient->mpsLeft = 
					from_rpm_to_mps(_motorsClient->rpmLeft);
				status_updated = 1;
				_lastReq->ack=1;
			}
			else {
				_lastReq->ack=0;
			}
			return 0;
		}
	}

	/*Right Motor Driver response*/
	if( (lenght==8) && (sender==CAN_ID_MotorRight) ){
		/*Set speed*/
		if(_lastReq->type==ACK_SET_SPEED){
			if( (frame[0]==0x60) && (frame[1]==0x41) && 
				(frame[2]==0x23) && (frame[3]==0x00) ){
				/* TODO check if the speed (present in the response) 
				 * is the actual speed required
				 */
				_lastReq->ack=1;
			}
			else {
				_lastReq->ack=0;
			}
			return 0;
		}
		
		/*Get speed*/
		if(_lastReq->type==ACK_GET_SPEED){
			if( (frame[0]==0x43) && (frame[1]==0x69) && 
				(frame[2]==0x60) && (frame[3]==0x00) ){
				int temp = frame[7];
				temp = temp << 8;
				temp = temp + frame[6];
				temp = temp << 8;
				temp = temp + frame[5];
				temp = temp << 8;
				temp = temp + frame[4];
				_motorsClient->encoderRight = temp;
				_motorsClient->rpmLeft = 
					from_encoder_to_rpm(_motorsClient->encoderRight);
				_motorsClient->mpsLeft = 
					from_rpm_to_mps(_motorsClient->rpmRight);
				status_updated = 1;
				_lastReq->ack=1;
			} 
			else {
				_lastReq->ack=0;
			}
			return 0;
		}
	}
	return -1;
}

/**Private methods implementation**/
float from_encoder_to_rpm(int encoder)
{
	/* unit of encoder speed (0.1 counts/s) */
	return (float) (encoder / (CPR * 10)) * 60; 
}

float from_rpm_to_mps(float rpm)
{
	return (float) (rpm / 60) * C_WHEEL;
}

float from_mps_to_rpm(float mps)
{
	return (float) (mps * 60) / C_WHEEL;
}

int from_rpm_to_encoder(float rpm)
{
	/* unit of encoder speed (0.1 counts/s) */
	float encoder = (rpm / 60.0) * CPR * 10.0; 
	return (int) encoder; 
}
