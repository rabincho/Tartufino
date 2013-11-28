void *encoder(void *args);

struct encoder_th_params {
	int file_index;
	int period_ms;
	const char* can_interface;
};
