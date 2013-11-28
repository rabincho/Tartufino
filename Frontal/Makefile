all:
	g++ main.c periodic.c keyboard.c MotorsServiceClient.c encoder.c LocalCapture.cpp OCVCapture.cpp -o main -lopencv_core -lopencv_highgui -lopencv_imgproc -lv4l2 -pthread -lrt

clean:
	rm -rf *o *d main
	rm -rf frames/f*
	rm -rf frames/c*
	rm -rf exp_encoder/f*
