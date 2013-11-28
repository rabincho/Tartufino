/*
 * OCVCapture - Code for capturing video frames using
 * v4l2 into OpenCV Mat wrappers
 * 
 * Written in 2012 by Martin Fox
 * 
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 * 
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 * 
 * This class can be used to capture OpenCV
 * images from a Camera using video4linux.
 * It has only been tested on one camera so far,
 * the MS Lifecam VX-7000.
 * 
 * The VideoCapture component in OpenCV calls on
 * the video4linux driver to deliver uncompressed
 * RGB images. The driver apparently asks the camera
 * for a JPEG image and then decompresses it. On an ARM
 * device such as a Beagleboard this is done in software
 * using a meager floating-point unit and the result is
 * so slow the board can't keep up, leading to error messages
 * and scrambled images (in part due to race conditions in
 * the OpenCV code.) This module asks the camera for YCbCr
 * (aka YUV) images which avoids the JPEG path.
 * 
 * Modified in 2013 by Bernardo Villalba Frias
 * 
 */
#ifndef OCVCAPTURE_H
#define OCVCAPTURE_H

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <vector>
#include <string>

class OCVCapture
{
  public:

    /*
     * Instantiate a Capture object.
     * The default size is 320 x 240.
     */
    OCVCapture();

    /*
     * The destructor automatically closes the camera.
     */
    virtual ~OCVCapture();

    /*
     * You can turn on verbose mode to which will cause the capture
     * object to spew debug messages to cout.
     */
    void setVerbose(bool verboseOn);
    bool verbose() const;

    /*
     * When the capture object is closed you can set the size. At the
     * time the capture object is opened the hardware will be queried
     * for a supported size which may differ from the requested size,
     * so do not assume the images you retrieve from the capture object
     * are the requested size.
     */
    void configureCapture(char* id, uint32_t width, uint32_t height, 
      uint32_t fps, char* pixfmt);

    /*
     * Before capturing images you must open the capture object.
     * The open call returns true if it was successful.
     * Open to read from the camera.
     */
    bool openCamera();

    /*
     * Returns true if the capture object is open.
     */
    bool isOpen() const;

    /*
     * After the capture object is opened you can query for the final
     * size that it negotiated with the camera.
     */
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getFrameRate() const;

    /*
     * Close the capture object. This releases the video device and
     * frees up driver-related memory. You can change the desired image
     * size while the object is closed.
     */
    void closeCamera();

    /*
     * The capturing process is divided into two parts. In the first
     * step you call 'grabFrame' to actually grab the (RAW) image. Then
     * later you call 'gray' to convert the grabbed image to the
     * desired color space depending on the pixel format chosen.
     */
    bool grabFrame(uint32_t& time);
    bool gray(cv::Mat& gray);
    bool rgb(cv::Mat& rgb);
    bool yuv2rgb(cv::Mat& rgb);
    bool yuv2gray(cv::Mat& gray);
    bool yuv2yuv(cv::Mat& yuv);
    bool mjpeg2rgb(cv::Mat& rgb);
    bool mjpeg2gray(cv::Mat& gray);

  private:
    int retry_ioctl(int request, void* argument);
    bool firstGrabSetup();
    void reportError(const char* error);
    void reportError(const char* error, int64_t value);

    void resizeMat(cv::Mat& mat, int matType);

  private:
    const char* m_device_id;

    /*
     * What the client wants...
     */
    uint32_t m_desired_width;
    uint32_t m_desired_height;
    uint32_t m_desired_frame_rate;
    uint32_t m_desired_pix_fmt;
    bool m_verbose;

    /*
     * Internal bookkeeping for the camera
     */
    int m_camera_handle;
    bool m_first_grab;

    /*
     * The size we negotitated with the driver.
     */
    uint32_t m_final_width;
    uint32_t m_final_height;
    uint32_t m_final_frame_rate;

    /*
     * These are the memory mapped image buffers
     * provided by the camera driver.
     */
    std::vector<void*>	m_mapped_buffer_ptrs;
    std::vector<size_t>	m_mapped_buffer_lens;

    /*
     * A copy of the mostly recently grabbed raw data.
     */
    std::vector<uint8_t> dataBuffer;
    size_t m_raw_image_size;
    uint8_t* m_raw_image_data;
    uint32_t m_raw_bytes_per_line;
};
#endif
