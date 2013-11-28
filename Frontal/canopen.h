#include <linux/types.h>

void set_init_flag(int v);
void sendMsg(__u32 ID, __u8 DATA[], int len);
int canOpen(void);
int register_pdo(int id, int PDOn);

int get_1b_signed_val(int id, int PDOn, int pos);
unsigned int get_1b_unsigned_val(int id, int PDOn, int pos);
int get_2b_signed_val(int id, int PDOn, int pos);
unsigned int get_2b_unsigned_val(int id, int PDOn, int pos);
int get_4b_signed_val(int id, int PDOn, int pos);
unsigned int get_4b_unsigned_val(int id, int PDOn, int pos);
void canopen_synch(void);
void canClose();

int activateSonarServiceClient();
int activateMotorsServiceClient();