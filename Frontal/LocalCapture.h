void *capture_frames(void *args);
void *process_frames(void *args);

void pauseProcessing();
void stopCapture();

struct video_th_params {
	int file_index;
};
