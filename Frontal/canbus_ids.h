/*
 * This file defines the can bus ids of the services on the BlueBot
 * A service can be involved in two types of communications:
 * - send a message (ack, share info...) 0x580 + Id of the service
 * - been asked to do something 0x600 + Id of the service
 */

#define CAN_SENDTO 0x600
#define CAN_SENDFROM 0x580

/* Ids of the two Motors can't be changed just by editing this file
 * In order to change them it is needed to connect to the drivers through serial and change them
 * with the CME2 tool
 */

#define CAN_ID_MotorLeft 0x01
#define CAN_ID_MotorRight 0x02

/*Ids of servives running on BlueBot*/

#define CAN_ID_Motors 0x00
#define CAN_ID_HighController 0x03
#define CAN_ID_RemoteCamera 0x04
#define CAN_ID_SonarSensors 0x06

