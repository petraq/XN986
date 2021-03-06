/**
 *
 * SONiX SDK Example Code
 * Category: Video Encode
 * File: snx_m2m_capture_2stream.c
 * Usage: 
 *		 1. The result video file would be saved in /tmp directory
 *		 2. Execute snx_m2m_one_stream (/usr/bin/snx_m2m_one_stream)
 *       3. The video encoding would start
 *       4. After the frame_num is reached, the encoding would stop
 *       5. Check the file recorded in the SD card by using VLC
 *
 * NOTE:
 *       Recording all streams to SD card would cause the target framerate can
 *       not be reached because of the bandwidth leakage of SD card.
 */
#include "snx_video_codec.h"

/*-----------------------------------------------------------------------------
 * Example Code Configuration
 *----------------------------------------------------------------------------*/

/*----------------- Stream 1 M2M stream --------------------------------------*/
#define V1_FORMAT			V4L2_PIX_FMT_H264	// V4L2_PIX_FMT_H264 or V4L2_PIX_FMT_MJPEG or V4L2_PIX_FMT_SNX420
												// Video Encode Format

#define V1_WIDTH			640					// Video Encoded Frame Width
#define V1_HEIGHT			480					// Video Encoded Frame Height
#define V1_FRAME_RATE		30					// Video Encoded Frame Rate
#define V1_IMG_DEV_NAME		"/dev/video0"		// ISP Device Node
#define V1_CAP_DEV_NAME		"/dev/video1"		// Video Codec Device Node for M2M
#define V1_FRAME_NUM		300					// Video Encoded Frame Number
#define V1_SCALE			1					// 1: 1, 2: 1/2, 4: 1/4 scaling down
#define V1_FILENAME			"/tmp/video1.h264"	// Video Encoded File Path

#define V1_BIT_RATE			600000				// Bitrate control for H264 ONLY
#define V1_DS_DEV_NAME		"1_h"				// Data stamp Device name for MROI setting

/*----------------- Stream 2 (1's capture stream) ----------------------------*/
#define V2_FORMAT			V4L2_PIX_FMT_H264	// V4L2_PIX_FMT_H264 or V4L2_PIX_FMT_MJPEG or V4L2_PIX_FMT_SNX420
												// Video Encode Format

#define V2_WIDTH			640					// Video Encoded Frame Width
#define V2_HEIGHT			480					// Video Encoded Frame Height
#define V2_FRAME_RATE		30					// Video Encoded Frame Rate
#define V2_IMG_DEV_NAME		"/dev/video0"		// ISP Device Node
#define V2_CAP_DEV_NAME		"/dev/video1"		// Video Codec Device Node for M2M
#define V2_FRAME_NUM		300					// Video Encoded Frame Number
#define V2_SCALE			2					// 1: 1, 2: 1/2, 4: 1/4 scaling down
#define V2_FILENAME			"/tmp/video2.h264"	// Video Encoded File Path

#define V2_BIT_RATE			200000				// Bitrate control for H264 ONLY
#define V2_DS_DEV_NAME		"1_hs"				// Data stamp Device name for MROI setting

/*-----------------------------------------------------------------------------
 * GLOBAL Variables
 *----------------------------------------------------------------------------*/

/* For thread control use */
pthread_cond_t      m2m1_cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     m2m1_mutex = PTHREAD_MUTEX_INITIALIZER;
int					m2m1_ref=0;

/*-----------------------------------------------------------------------------
 * Example Code Flow
 *----------------------------------------------------------------------------*/
 

/*
	The entrance of this file.
	One M2M streams and one capture streams would be created.
	The configuration can set on the setting definitions above. 
	After all stream confs are done, the thread of each stream would be created.
	The bitstreams of two stream will be record on the SD card.
*/
int main(int argc, char **argv)
{
	int ret = 0;
	/* Create 2 stream config */
	stream_conf_t *stream1 = NULL;
	stream_conf_t *stream2 = NULL;
	
	/*--------------------------------------------------------
		stream 1 setup
	---------------------------------------------------------*/
	stream1 = malloc(sizeof(stream_conf_t));
	memset(stream1, 0, sizeof(stream_conf_t));
	struct snx_m2m *v1_m2m = &stream1->m2m;
	
	v1_m2m->isp_fps = 30;
	v1_m2m->m2m_buffers = 2;
	v1_m2m->ds_font_num = DS_BUFFER_SIZE;
	v1_m2m->scale = V1_SCALE;
	v1_m2m->codec_fps = V1_FRAME_RATE;
	v1_m2m->width = V1_WIDTH;
	v1_m2m->height = V1_HEIGHT;
	v1_m2m->gop = v1_m2m->isp_fps;
	v1_m2m->codec_fmt = V1_FORMAT;
	v1_m2m->out_mem = V4L2_MEMORY_USERPTR;
	v1_m2m->m2m = 1;								/* M2M stream */
	v1_m2m->isp_mem = V4L2_MEMORY_MMAP;
    v1_m2m->isp_fmt = V4L2_PIX_FMT_SNX420;
	v1_m2m->bit_rate = V1_BIT_RATE;

	strcpy(v1_m2m->ds_dev_name, V1_DS_DEV_NAME);
	
	strcpy(v1_m2m->isp_dev,V1_IMG_DEV_NAME);
	strcpy(v1_m2m->codec_dev,V1_CAP_DEV_NAME);
	
	if(strlen(V1_FILENAME))
		stream1->fd = open(V1_FILENAME, O_RDWR | O_NONBLOCK | O_CREAT);
	stream1->frame_num = V1_FRAME_NUM;
	
	/* 
		Point to the m2m thread mutex and cond 
		For multi-thread control use (one m2m thread with multi cap thread
	*/
	stream1->cond = &m2m1_cond;
	stream1->mutex = &m2m1_mutex;
	stream1->ref = &m2m1_ref;
	
	/*--------------------------------------------------------
		stream 2 setup
	---------------------------------------------------------*/
	stream2 = malloc(sizeof(stream_conf_t));
	memset(stream2, 0, sizeof(stream_conf_t));
	struct snx_m2m *v2_m2m = &stream2->m2m;
	
	v2_m2m->isp_fps = 30;
	v2_m2m->m2m_buffers = 2;
	v2_m2m->ds_font_num = DS_BUFFER_SIZE;
	v2_m2m->scale = V2_SCALE;
	v2_m2m->codec_fps = V2_FRAME_RATE;
	v2_m2m->width = V2_WIDTH;
	v2_m2m->height = V2_HEIGHT;
	v2_m2m->gop = v2_m2m->isp_fps;
	v2_m2m->codec_fmt = V2_FORMAT;
	v2_m2m->out_mem = V4L2_MEMORY_USERPTR;
	v2_m2m->m2m = 0;								/* stream 2 is a capture stream from stream1 m2m */
	v2_m2m->isp_mem = V4L2_MEMORY_MMAP;
	v2_m2m->bit_rate = V2_BIT_RATE;
	/*
	v2_m2m->isp_fmt = V4L2_PIX_FMT_SNX420;
	strcpy(v2_m2m->isp_dev,V2_IMG_DEV_NAME);
	*/

	strcpy(v2_m2m->ds_dev_name, V2_DS_DEV_NAME);

	strcpy(v2_m2m->codec_dev,V2_CAP_DEV_NAME);
	
	if(strlen(V2_FILENAME))
		stream2->fd = open(V2_FILENAME, O_RDWR | O_NONBLOCK | O_CREAT);
	stream2->frame_num = V2_FRAME_NUM;
	
	/* stream 2 is a capture stream from stream1 m2m */
	stream2->cond = &m2m1_cond;
	stream2->mutex = &m2m1_mutex;
	stream2->ref = &m2m1_ref;
	
	/*--------------------------------------------------------
		Start 
	---------------------------------------------------------*/
	ret = pthread_create(&stream1->stream_thread, NULL, (void *)snx_m2m_cap_rc_flow, stream1);
	ret += pthread_create(&stream2->stream_thread, NULL, (void *)snx_m2m_cap_rc_flow, stream2);
	if(ret != 0) {
		perror_exit(1, "exit thread creation failed");   
	}
	
	pthread_detach(stream2->stream_thread);
	pthread_detach(stream1->stream_thread);

	while (1) {
		if((stream1->frame_num == 0) && (stream2->frame_num == 0))
			break;
		sleep(2);
		// dynamic modify fps/bps/play/pause
		snx_dynamic_modify(stream1);
		snx_dynamic_modify(stream2);
	}
	
	
	/*--------------------------------------------------------
		Record End 
	---------------------------------------------------------*/
	/* destroy the thread mutex and cond */
	pthread_cond_destroy(&m2m1_cond);
	pthread_mutex_destroy(&m2m1_mutex);
	
	close(stream1->fd);
	close(stream2->fd);
	/* Free the stream configs */
	free(stream1);
	free(stream2);
    return ret;
}
