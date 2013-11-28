all:
	g++ RemoteCapture.cpp OCVCapture.cpp periodic.c -o main -lopencv_core -lopencv_highgui -lopencv_imgproc -lv4l2 -pthread -lrt

clean:
	rm -rf *o *d main
	rm -rf frames/f*
	rm -rf frames/c*
