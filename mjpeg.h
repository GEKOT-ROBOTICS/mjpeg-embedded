#ifndef MJPEG_H
#define MJPEG_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "sd.h"

#include "esp_err.h"

#include "riff.h"

// TODO: For the pixel height, pixel width, and aspect ratio, the info is available somewhere in the esp32-camera component, we might be able to extract it from there

#define FRAME_SIZE      FRAMESIZE_VGA
#define FPS             20
#define BIT_COUNT       24

#if (FRAME_SIZE == FRAMESIZE_CIF)
	#define HEIGHT		296
	#define WIDTH		400
	#define ASPECT_RATIO	ASPECT_4_3

#elif (FRAME_SIZE == FRAMESIZE_QVGA)
	#define HEIGHT		240
	#define WIDTH		320
	#define ASPECT_RATIO	ASPECT_4_3

#elif (FRAME_SIZE == FRAMESIZE_VGA)
        #define HEIGHT          480
        #define WIDTH           640
        #define ASPECT_RATIO    ASPECT_4_3

#elif (FRAME_SIZE == FRAMESIZE_HD)
	#define HEIGHT		720
	#define WIDTH		1280
	#define ASPECT_RATIO	ASPECT_16_9
#endif

#define MJPEG_SVC_TASK                 mjpeg_svc
#define MJPEG_SVC_TASK_NAME            "MJPEG-SVC-TASK"
#define MJPEG_SVC_STACK_SIZE           4096
#define MJPEG_SVC_TASK_PRIORITY        tskIDLE_PRIORITY + 1
#define MJPEG_SVC_TASK_CORE            1
#define MJPEG_SVC_TASK_MALLOC          MALLOC_CAP_SPIRAM

struct mjpeg_context {
	sd_handle_t out_file_handle; // This is the real file that the avi will be stored in
	sd_handle_t idx_file_handle; // This is a temporary file. It stores the index table for seeking to particular frames that is appended to the end of the avi file after we are done
	uint8_t fps;		// We really can't get fps larger than 256. Change if we can
	uint16_t height;
	uint16_t width;
	size_t riff_size;
	size_t hdrl_size;
	size_t strl_size;
	size_t movi_size;
	size_t total_frames;
	long riff_size_pos;
	long hdrl_size_pos;
	long strl_size_pos;
	long movi_size_pos;
	long avih_total_frames_pos;
	long strh_length_pos;
	AVIH avih;
	STRH strh;
	BMPH bmph;
	VPRP vprp;
};

typedef struct mjpeg_context	mjpeg_context_t;
typedef struct mjpeg_context*	mjpeg_handle_t;

struct mjpeg_svc_context {
	uint8_t	state;
	mjpeg_handle_t mjpeg_handle;
	QueueHandle_t out_buffer;
	QueueHandle_t in_buffer;
};

typedef struct mjpeg_svc_context	mjpeg_svc_context_t;
typedef struct mjpeg_svc_context*	mjpeg_svc_handle_t;

void mjpeg_svc(void *pvParameters);

#endif /* MJPEG_H */
