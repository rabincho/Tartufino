#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include "can.h" 
#include "canbus_ids.h" 
#include <sys/mman.h>

#include "MotorsServiceClient.h"

//#define VERB

#define PRIO 98              /* Priority of the receiving thread  */

__u8 * PDO0[128] = {NULL};   /* Memory reserved for TX PDO1 */
__u8 * PDO1[128] = {NULL};   /* Memory reserved for TX PDO2 */
__u8 * PDO2[128] = {NULL};   /* Memory reserved for TX PDO3 */

__u8 lastSDOack[8];          /* When init_flag is set this array */
                             /* contains the las received message */
static int init_flag = 0;
static int dev_cnt = 0;      /* CAN devices counter */
static volatile int endrcv = 0;
static pthread_t  rt_rcv;
static int s;                 /* can raw socket  */

//static int sonar_service_client = 0;
static int motors_service_client = 0;

int activateMotorsServiceClient(){
  motors_service_client = 1;
  
  return 0;
}

/**CANbus Part**/
void set_init_flag(int v)
{
  init_flag = v;
}

__u8 * get_PDO(int PDOn,int id)
{
  switch(PDOn){
  case 1:
    return(PDO0[id]);
    break;
  case 2:
    return(PDO1[id]);
    break;
  case 3:
    return(PDO2[id]);
    break;
  }
  return(NULL);
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

    if ((nbytes = write(s, &Tmsg, sizeof(Tmsg))) != sizeof(Tmsg)) {
      errno = -1;
      printf("Error sending message through CANbus!!!\n");
    }
}

void *rcv(void *args)
{
  /* Receiving thread */

  struct can_frame m;
  int nbytes;
  int i, errno;

  int id, PDOn;
  __u8 * data;
  struct sched_param param;

  param.sched_priority = PRIO;
  if(sched_setscheduler(0, SCHED_FIFO, &param)==-1){
    perror("sched_setscheduler failed");
    exit(-1);
  }

  mlockall(MCL_CURRENT | MCL_FUTURE);

  while (!endrcv) {       /* receiving loop */
    if ((nbytes = read(s, &m, sizeof(m))) < 0) {
      errno = -1;
    }

#ifdef VERB
    printf("<-- 0x%03x  %d   ",m.can_id,m.can_dlc);
    for(i=0;i<m.can_dlc;i++) printf("0x%02x  ",m.data[i]);
    printf("\n");    
#endif

int id_message_sendfrom = m.can_id - CAN_SENDFROM;
//int id_message_sendto = m.can_id - CAN_SENDTO;

if( ( (id_message_sendfrom == CAN_ID_Motors) || 
	(id_message_sendfrom == CAN_ID_MotorLeft) || 
	(id_message_sendfrom == CAN_ID_MotorRight) ) && 
	(motors_service_client)) {
#ifdef DEBUG_L3
    printf("CANbus message received for MotorsServiceClient");
#endif
    MotorsServiceClienthandle(m.data, m.can_dlc, id_message_sendfrom);
}

    /* Store PDO messages  */
    if(m.can_id==0x7FF) m.can_id=0x3FF;

    if((m.can_id>=0x180) && (m.can_id<=0x3FF)){    
      PDOn = (m.can_id >> 8) & 0x0F;
      id   = (m.can_id & 0xFF) - 0x80;

      data = get_PDO(PDOn,id);
      if (data!=NULL) memcpy(data,m.data,m.can_dlc);
    }
    if(init_flag){
      memcpy(lastSDOack,m.data,m.can_dlc);
    }
  }
  return 0;
}

int canOpen()
{
  /* CAN initialization */
  struct sockaddr_can addr;
  struct ifreq ifr;

#ifdef VERB
#endif

  if(!dev_cnt){  /* This task is performed only one time */

    /* open socket */
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
      return -1;
    }

    strcpy(ifr.ifr_name, "can0");
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("bind");
      return 1;
    }

	/* Start receiving task */
    pthread_create(&rt_rcv, NULL, rcv, NULL);  
  }
  dev_cnt++;
  printf("Init CAN end\n");
  return 0;
}

int register_pdo(int id, int PDOn)
{
  /* Allows to allocate the memory for the registered PDO's */
  int i;
  __u8 * data;

  data = (__u8*) malloc(8*sizeof(__u8));
  for(i=0;i<8;i++) data[i]=0;
  switch(PDOn){
  case 1:
    PDO0[id] = data;
    break;
  case 2:
    PDO1[id] = data;
    break;
  case 3:
    PDO2[id] = data;
    break;
  }
  return(0);
}

void canopen_synch()
{
  sendMsg(0x80,NULL,0);
}

void canClose()
{
  if(--dev_cnt == 0) {
    endrcv = 1;
    close(s);
#ifdef VERB
#endif

  }
}

/* The following 8 functions allows to read N (1, 2 or 4) */
/* signed or unsigned bytes from a saved PDO message    */ 

int get_1b_signed_val(int id, int PDOn, int pos)
{
  __u8 * data;
  char counter;
  counter = 0;

  data = get_PDO(PDOn,id);
  counter = data[pos];
  return ((int) counter);
}

unsigned int get_1b_unsigned_val(int id, int PDOn, int pos)
{
  __u8 * data;
  unsigned char counter;
  counter = 0;

  data = get_PDO(PDOn,id);
  counter = data[pos];
  return ((unsigned int) counter);

}

int get_2b_signed_val(int id, int PDOn, int pos)
{
  __u8 * data;
  short int counter;
  int i;
  counter = 0;

  data = get_PDO(PDOn,id);
  for(i=pos+1;i>=pos;i--) counter = (counter << 8) + data[i];
  return ((int) counter);
}

unsigned int get_2b_unsigned_val(int id, int PDOn, int pos)
{
  __u8 * data;
  short unsigned int counter;
  int i;
  counter = 0;

  data = get_PDO(PDOn,id);
  for(i=pos+1;i>=pos;i--) counter = (counter << 8) + data[i];
  return ((unsigned int) counter);

}

int get_4b_signed_val(int id, int PDOn, int pos)
{
  __u8 * data;
  int counter;
  int i;
  counter = 0;

  data = get_PDO(PDOn,id);
  for(i=pos+3;i>=pos;i--) counter = (counter << 8) + data[i];
  return (counter);
}

unsigned int get_4b_unsigned_val(int id, int PDOn, int pos)
{
  __u8 * data;
  unsigned int counter;
  int i;
  counter = 0;

  data = get_PDO(PDOn,id);
  for(i=pos+3;i>=pos;i--) counter = (counter << 8) + data[i];
  return (counter);

}

