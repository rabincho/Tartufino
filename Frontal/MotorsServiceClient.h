#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#define ACK_STATUS 0
#define ACK_SET_SPEED 1
#define ACK_GET_SPEED 2

struct Motors{
  int statusLeft;
  int statusRight;
  int encoderLeft;
  int encoderRight;
  float rpmLeft;
  float rpmRight;
  float mpsLeft;
  float mpsRight;
};

struct MotorsAck{
  int idSender;
  int type;
  int ack;
};

int MotorsServiceClient();
struct Motors readMotors();

int setMotorLeftSpeed(float speed_mps, struct MotorsAck* ack);
int setMotorRightSpeed(float speed_mps, struct MotorsAck* ack);

int enableMotors(uint8_t idSender, struct MotorsAck* ack);

int disableMotors(uint8_t idSender, struct MotorsAck* ack);

int updatestatusMotors(uint8_t idSender, struct MotorsAck* ack);
int updatespeedMotorLeft(struct MotorsAck* ack);
int updatespeedMotorRight(struct MotorsAck* ack);

int isMotorsUpdated();
int cancelMotorsPendingRequest();

int MotorsServiceClienthandle(uint8_t* frame, int lenght, int sender);