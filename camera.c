/*
 * Copyright 2017 Brian Starkey <stark3y@gmail.com>
 *
 * Portions based on the Raspberry Pi Userland examples:
 * Copyright (c) 2013, Broadcom Europe Ltd
 * Copyright (c) 2013, James Hughes
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "cameracontrol.h"
#include "camera.h"

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

static void port_cleanup(MMAL_PORT_T *port)
{
	if (!port)
		return;

	if (port->is_enabled)
		mmal_port_disable(port);
}

static void component_cleanup(MMAL_COMPONENT_T *component)
{
	int i;

	if (!component)
		return;

	for (i = 0; i < component->input_num; i++)
		port_cleanup(component->input[i]);

	for (i = 0; i < component->output_num; i++)
		port_cleanup(component->output[i]);

	port_cleanup(component->control);

	mmal_component_destroy(component);
}

struct buffer_pool {
	MMAL_PORT_T *port;
	MMAL_POOL_T *free_pool;
	MMAL_QUEUE_T *ready_queue;
};

static void buffer_pool_cleanup(struct buffer_pool *pool)
{
	if (!pool)
		return;

	if (pool->ready_queue)
		mmal_queue_destroy(pool->ready_queue);

	if (pool->free_pool)
		mmal_pool_destroy(pool->free_pool);

	free(pool);
}

static struct buffer_pool *buffer_pool_create(MMAL_PORT_T *port)
{
	struct buffer_pool *pool = calloc(1, sizeof(*pool));
	if (!pool)
		return NULL;

	port->buffer_num = 3;
	port->buffer_size = port->buffer_size_recommended;
	pool->free_pool = mmal_port_pool_create(port, port->buffer_num, port->buffer_size);
	if (!pool->free_pool) {
		fprintf(stderr, "Couldn't create buffer pool\n");
		goto fail;
	}

	pool->ready_queue = mmal_queue_create();
	if (!pool->ready_queue) {
		fprintf(stderr, "Couldn't create ready queue\n");
		goto fail;
	}

	return pool;

fail:
	buffer_pool_cleanup(pool);
	return NULL;
}

struct camera {
	MMAL_COMPONENT_T *component;
	MMAL_PORT_T *port;
	struct buffer_pool *pool;

	uint32_t width, height;
	unsigned int fps;
};

struct camera_buffer *camera_dequeue_buffer(struct camera *camera)
{
	struct camera_buffer *buf = malloc(sizeof(*buf));
	if (!buf)
		return buf;

	buf->hnd = mmal_queue_timedwait(camera->pool->ready_queue, 1000);
	if (!buf->hnd) {
		fprintf(stderr, "Couldn't dequeue buffer\n");
		free(buf);
		return NULL;
	}

	buf->egl_buf = ((MMAL_BUFFER_HEADER_T *)buf->hnd)->data;

	return buf;
}

void camera_queue_buffer(struct camera *camera, struct camera_buffer *buf)
{
	MMAL_BUFFER_HEADER_T *free_buf = NULL;

	mmal_buffer_header_release(buf->hnd);
	free(buf);

	while ((free_buf = mmal_queue_get(camera->pool->free_pool->queue))) {
		MMAL_STATUS_T ret = mmal_port_send_buffer(camera->port, free_buf);
		if (ret != MMAL_SUCCESS)
			fprintf(stderr, "Couldn't queue free buffer: %d\n", ret);
	}
}

static void camera_frame_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
	struct camera *camera = (struct camera *)port->userdata;

	mmal_queue_put(camera->pool->ready_queue, buf);
}

static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf) {
	/* Don't do anything with control buffers... */
	mmal_buffer_header_release(buf);
}

void camera_exit(struct camera *camera)
{
	camera_disable(camera);
	free(camera);
}

int camera_enable(struct camera *camera)
{
	RASPICAM_CAMERA_PARAMETERS default_parameters = { 0 };
	MMAL_BUFFER_HEADER_T *buf;
	MMAL_ES_FORMAT_T *format;
	MMAL_STATUS_T ret;
	MMAL_PORT_T *ports[2];
	int i, iret;

	ret = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera->component);
	if (ret != MMAL_SUCCESS)
	{
		fprintf(stderr, "Failed to create component\n");
		goto fail;
	}

	if (camera->component->output_num != 3) {
		fprintf(stderr, "Unexpected number of output ports: %d\n", camera->component->output_num);
		goto fail;
	}

	ret = mmal_port_enable(camera->component->control, camera_control_callback);
	if (ret != MMAL_SUCCESS) {
		fprintf(stderr, "Enabling control port failed: %d\n", ret);
		goto fail;
	}

	MMAL_PARAMETER_CAMERA_CONFIG_T camera_config = {
		.hdr = { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(camera_config) },
		.max_stills_w = 0,
		.max_stills_h = 0,
		.stills_yuv422 = 0,
		.stills_yuv422 = 0,
		.one_shot_stills = 1,
		.max_preview_video_w = camera->width,
		.max_preview_video_h = camera->height,
		.num_preview_video_frames = 3,
		.stills_capture_circular_buffer_height = 0,
		.fast_preview_resume = 0,
		.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
	};

	ret = mmal_port_parameter_set(camera->component->control, &camera_config.hdr);
	if (ret != MMAL_SUCCESS) {
		fprintf(stderr, "Configuring camera parameters failed: %d\n", ret);
		goto fail;
	}

	raspicamcontrol_set_defaults(&default_parameters);
	iret = raspicamcontrol_set_all_parameters(camera->component, &default_parameters);
	if (iret != 0) {
		fprintf(stderr, "Setting raspicam defaults failed: %d\n", iret);
		goto fail;
	}

	camera->port = camera->component->output[MMAL_CAMERA_PREVIEW_PORT];
	camera->port->userdata = (void *)camera;

	ports[0] = camera->component->output[MMAL_CAMERA_VIDEO_PORT];
	ports[1] = camera->component->output[MMAL_CAMERA_CAPTURE_PORT];

	format = camera->port->format;
	format->encoding = MMAL_ENCODING_OPAQUE;
	format->encoding_variant = MMAL_ENCODING_I420;
	format->es->video.width = camera->width;
	format->es->video.height = camera->height;
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = camera->width;
	format->es->video.crop.height = camera->height;
	format->es->video.frame_rate.num = camera->fps;
	format->es->video.frame_rate.den = 1;
	ret = mmal_port_format_commit(camera->port);
	if (ret != MMAL_SUCCESS)
	{
		fprintf(stderr, "Couldn't set preview port format: %d\n", ret);
		goto fail;
	}

	for (i = 0; i < 2; i++) {
		mmal_format_full_copy(ports[i]->format, camera->port->format);
		ret = mmal_port_format_commit(ports[i]);
		if (ret != MMAL_SUCCESS)
		{
			fprintf(stderr, "Couldn't copy format to port %d: %d\n", i, ret);
			goto fail;
		}
	}

	ret = mmal_port_parameter_set_boolean(camera->port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
	if (ret != MMAL_SUCCESS)
	{
		fprintf(stderr, "Couldn't set zero-copy on preview port: %d\n", ret);
		goto fail;
	}

	ret = mmal_component_enable(camera->component);
	if (ret != MMAL_SUCCESS )
	{
		fprintf(stderr, "Couldn't enable component: %d\n", ret);
		goto fail;
	}

	camera->pool = buffer_pool_create(camera->port);
	if (!camera->pool) {
		fprintf(stderr, "Couldn't create buffer pool\n");
		goto fail;
	}

	ret = mmal_port_enable(camera->port, camera_frame_callback);
	if (ret != MMAL_SUCCESS)
	{
		fprintf(stderr, "Couldn't enable camera port: %d\n", ret);
		goto fail;
	}

	i = 0;
	while ((buf = mmal_queue_get(camera->pool->free_pool->queue))) {
		ret = mmal_port_send_buffer(camera->port, buf);
		if (ret != MMAL_SUCCESS) {
			fprintf(stderr, "Couldn't queue free buffer: %d\n", ret);
			goto fail;
		}
		i++;
	}
	if (i != camera->port->buffer_num)
		fprintf(stderr, "Queued an unexpected number of buffers (%d)\n", i);

	return 0;

fail:
	return -1;
}

int camera_disable(struct camera *camera)
{
	if (camera->port->is_enabled)
		mmal_port_disable(camera->port);
	buffer_pool_cleanup(camera->pool);
	component_cleanup(camera->component);

	return 0;
}

struct camera *camera_init(uint32_t width, uint32_t height, unsigned int fps)
{
	int ret;
	struct camera *camera = calloc(1, sizeof(*camera));

	if (!camera)
		return NULL;

	camera->width = width;
	camera->height = height;
	camera->fps = fps;

	ret = camera_enable(camera);
	if (!ret) {
		return camera;
	}

	camera_exit(camera);
	return NULL;
}
