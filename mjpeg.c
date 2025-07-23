/*
 * mjpeg.c - MJPEG creator tool (https://github.com/nanoant/mjpeg)
 *
 * Copyright (c) 2011 Adam Strzelecki
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "riff.h"
#include "mjpeg.h"

#include "types.h"
#include "task_states.h"

#include "esp_log.h"

esp_err_t _write_riff_header(mjpeg_handle_t ctx) {
	const char F_TAG[] = "_write-riff-header";
	esp_err_t err = ESP_OK;

	uint32_t buffer[16] = {0x00};

	// We do not know what the size of the riff will end up being until after we are done writing everything.
	// Thus, we should save ourselves space for us to update the riff size later

	// The RIFF keyword and the size of the file are not included in the "riff size"
	// Therefore, technically, we can consider the riff size as: total file size - 8 bytes

	// Technically, for all the sizes, we could calculate them at compile time, but it is easier for it to be dynamic as it is because we are doing init
	buffer[0] = FOURCC_RIFF;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write RIFF keyword to file: %s", esp_err_to_name(err));
		return err;
	}

	// The current position is where we should be writing to the riff size
	ctx->riff_size_pos = ctx->out_file_handle->pos;

	// Write the placeholder for the riff size
	buffer[0] = 0;
	ctx->out_file_handle->payload.current_data_len	= sizeof(uint32_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp RIFF size to file: %s", esp_err_to_name(err));
		return err;
	}

	// Write AVI keyword
	buffer[0] = FOURCC_AVI;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write AVI keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	// Add AVI keyword to riff size
	ctx->riff_size += sizeof(FOURCC);

	// Write LIST keyword
	buffer[0] = FOURCC_LIST;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write LIST keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(FOURCC);

	// The current position is where we should be writing to the hdrl size
	ctx->hdrl_size_pos = ctx->out_file_handle->pos;

	// Write the placeholder for the HDRL size
	buffer[0] = 0;
	ctx->out_file_handle->payload.current_data_len	= sizeof(uint32_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp HDRL size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(uint32_t);

	// Write HDRL keyword
	buffer[0] = FOURCC_HDLR;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write HDRL keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(FOURCC);

	// Write AVIH keyword and size of avih
	buffer[0] = FOURCC_AVIH;
	buffer[1] = sizeof(ctx->avih);
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(size_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write AVIH keyword and size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(FOURCC) + sizeof(size_t);

	// Set the byte offset of the totalFrames field to overwrite later
	ctx->avih_total_frames_pos = ctx->out_file_handle->pos + offsetof(AVIH, totalFrames);

	// Write AVIH struct
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->avih);
	ctx->out_file_handle->payload.data		= (char *)&ctx->avih;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write AVIH struct to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(ctx->avih);

	// Write LIST keyword
	buffer[0] = FOURCC_LIST;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write LIST keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(FOURCC);

	// The current position is where we should be writing to the strl size
	ctx->strl_size_pos = ctx->out_file_handle->pos;
	
	// Write the placeholder for the STRL size
	buffer[0] = 0;
	ctx->out_file_handle->payload.current_data_len	= sizeof(uint32_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp STRL size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(uint32_t);
	
	// Write STRL keyword
	buffer[0] = FOURCC_STRL;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write STRL keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(FOURCC);

	// Write STRH keyword and size of strh
	buffer[0] = FOURCC_STRH;
	buffer[1] = sizeof(ctx->strh);
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(size_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write STRH keyword and size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(FOURCC) + sizeof(size_t);

	// Set the byte offset of the length field to overwrite later
	ctx->strh_length_pos = ctx->out_file_handle->pos + offsetof(STRH, length);
	
	// Write STRH struct
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->strh);
	ctx->out_file_handle->payload.data		= (char *)&ctx->strh;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write STRH struct to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(ctx->strh);

	// Write BMPH keyword and size of bmph
	buffer[0] = FOURCC_STRF;
	buffer[1] = sizeof(ctx->bmph);
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(size_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write STRF keyword and size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(FOURCC) + sizeof(size_t);

	// Write BMPH struct
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->bmph);
	ctx->out_file_handle->payload.data		= (char *)&ctx->bmph;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write BMPH struct to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(BMPH);

	// Write VPRP keyword and size of bmph
	buffer[0] = FOURCC_VPRP;
	buffer[1] = sizeof(ctx->vprp);
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(size_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write VPRP keyword and size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(FOURCC) + sizeof(size_t);

	// Write VPRP struct
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->vprp);
	ctx->out_file_handle->payload.data		= (char *)&ctx->vprp;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write VPRP struct to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->strl_size += sizeof(VPRP);

	// We now know the size of strl, so update that value
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->strl_size);
	ctx->out_file_handle->payload.data		= (char *)&ctx->strl_size;
	ctx->out_file_handle->payload.pos		= ctx->strl_size_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the strl size: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += ctx->strl_size;

	// We now know the size of hdrl, so update that value
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->hdrl_size);
	ctx->out_file_handle->payload.data		= (char *)&ctx->hdrl_size;
	ctx->out_file_handle->payload.pos		= ctx->hdrl_size_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the strl size: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += ctx->hdrl_size;
	
	// Write LIST keyword
	buffer[0] = FOURCC_LIST;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write LIST keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(FOURCC);

	// The current position is where we should be writing to the movi size
	ctx->movi_size_pos = ctx->out_file_handle->pos;

	// Write the placeholder for the MOVI size
	buffer[0] = 0;
	ctx->out_file_handle->payload.current_data_len	= sizeof(uint32_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp MOVI size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(uint32_t);

	// Write MOVI keyword
	buffer[0] = FOURCC_MOVI;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write MOVI keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->movi_size += sizeof(FOURCC);

	return err;	
}


esp_err_t _write_jpeg_frame(mjpeg_handle_t ctx, frame_buffer_t frame_buffer) {
	const char F_TAG[] = "_write-jpeg-frame";
	esp_err_t err = ESP_OK;

	uint8_t byte_alignment_buffer = 0x00;
	uint32_t buffer[2] = {0x00};
	
	ESP_LOGI(F_TAG, "Received frame buffer: %zu", ctx->total_frames++);
	IDX1 idx1 = {
		.id	= FOURCC_00DC,
		.flags	= 0,
		.offset	= ctx->movi_size,
		.size	= frame_buffer.buffer_len
	};

	// Write IDX1 struct
	ctx->idx_file_handle->payload.current_data_len	= sizeof(idx1);
	ctx->idx_file_handle->payload.data		= (char *)&idx1;
	err = write_file(ctx->idx_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write IDX1 struct to file: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(F_TAG, "Saved index information to index file");

	// Write 00dc header and idx1 size to file
	buffer[0] = FOURCC_00DC;
	buffer[1] = frame_buffer.buffer_len;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(uint32_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write 00dc header and size to file: %s", esp_err_to_name(err));
		return err;
	}					
	ctx->movi_size += sizeof(FOURCC) + sizeof(uint32_t);

	// Write actual JPEG image
	// Data must be byte aligned, so if we happent o write an odd amount of data, we must pad to make it even
	ctx->out_file_handle->payload.current_data_len	= frame_buffer.buffer_len;
	ctx->out_file_handle->payload.data		= (char *)frame_buffer.buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write 00dc header and size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->movi_size += frame_buffer.buffer_len;

	if (frame_buffer.buffer_len % 2 != 0) {
		ctx->out_file_handle->payload.current_data_len	= sizeof(byte_alignment_buffer);
		ctx->out_file_handle->payload.data		= (char *)&byte_alignment_buffer;
		err = write_file(ctx->out_file_handle);
		if (err != ESP_OK) {
			ESP_LOGE(F_TAG, "Failed to write byte alignment buffer to file: %s", esp_err_to_name(err));
			return err;
		}
		ctx->movi_size += sizeof(byte_alignment_buffer);
	}
	return err;
}


esp_err_t _write_final_riff_updates(mjpeg_handle_t ctx) {
	const char F_TAG[] = "_write-final-riff-updates";
	esp_err_t err = ESP_OK;

	uint8_t byte_alignment_buffer = 0x00;

	// We now know the size of movi, so update that value
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->movi_size);
	ctx->out_file_handle->payload.data		= (char *)&ctx->movi_size;
	ctx->out_file_handle->payload.pos		= ctx->movi_size_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the movi size: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += ctx->movi_size;

	// We now have to write the indicies in the temporary file into the actual file
	ctx->idx_file_handle->payload.pos		= 0;
	err = seek_file(ctx->idx_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to set file pointer to the beginning of the index file");
		return err;
	}

	// Read and write each IDX1 saved to the temp file
	for (size_t i = 0; i < ctx->total_frames; i++) {
		IDX1 idx1 = {0x00};
		ctx->idx_file_handle->payload.max_data_len	= sizeof(idx1);
		ctx->idx_file_handle->payload.data		= (char *)&idx1;
		err = read_file(ctx->idx_file_handle);
		if (err != ESP_OK) {
			ESP_LOGE(F_TAG, "Failed to read IDX1 from temp file: %s", esp_err_to_name(err));
			return err;
		}
		if (ctx->idx_file_handle->payload.max_data_len != ctx->idx_file_handle->payload.current_data_len) {
			ESP_LOGE(F_TAG, "We did not retrieve the expected amount of data! Expected: %lu, got: %lu", ctx->idx_file_handle->payload.max_data_len, ctx->idx_file_handle->payload.current_data_len);
			return ESP_ERR_INVALID_SIZE;
		}

		ctx->out_file_handle->payload.current_data_len	= sizeof(idx1);
		ctx->out_file_handle->payload.data		= (char *)&idx1;
		err = write_file(ctx->out_file_handle);
		if (err != ESP_OK) {
			ESP_LOGE(F_TAG, "Failed to write IDX1 to file: %s", esp_err_to_name(err));
			return err;
		}
		ctx->riff_size += sizeof(idx1);

		if (sizeof(idx1) % 2 != 0) {
			ctx->out_file_handle->payload.current_data_len	= sizeof(byte_alignment_buffer);
			ctx->out_file_handle->payload.data		= (char *)&byte_alignment_buffer;
			err = write_file(ctx->out_file_handle);
			if (err != ESP_OK) {
				ESP_LOGE(F_TAG, "Failed to write byte alignment buffer to file: %s", esp_err_to_name(err));
				return err;
			}
			ctx->riff_size += sizeof(byte_alignment_buffer);
		}
	}

	// We now know the size of riff, so update that value
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->riff_size);
	ctx->out_file_handle->payload.data		= (char *)&ctx->riff_size;
	ctx->out_file_handle->payload.pos		= ctx->riff_size_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the riff size: %s", esp_err_to_name(err));
		return err;
	}

	// We know the number of frames we recorded, so update that value
	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->total_frames);
	ctx->out_file_handle->payload.data		= (char *)&ctx->total_frames;
	ctx->out_file_handle->payload.pos		= ctx->avih_total_frames_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the avih total frames: %s", esp_err_to_name(err));
		return err;
	}

	ctx->out_file_handle->payload.current_data_len	= sizeof(ctx->total_frames);
	ctx->out_file_handle->payload.data		= (char *)&ctx->total_frames;
	ctx->out_file_handle->payload.pos		= ctx->strh_length_pos;
	err = update_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to update the strh length: %s", esp_err_to_name(err));
		return err;
	}

	return err;
}


void mjpeg_svc(void *pvParameters) {
	const char *F_TAG = pcTaskGetName(NULL);
	esp_err_t err = ESP_OK;

	ESP_LOGI(F_TAG, "Entered on core %d", xPortGetCoreID());

	mjpeg_svc_handle_t ctx = (mjpeg_svc_handle_t)pvParameters;

	frame_buffer_t frame_buffer;

	// Write header
	err = _write_riff_header(ctx->mjpeg_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write the riff header for the AVI file: %s", esp_err_to_name(err));
		vTaskDelete(NULL);
	}
	ESP_LOGI(F_TAG, "Header information written");

	TickType_t xPeriod = pdMS_TO_TICKS(1000);
	while (true) {
		switch (ctx->state) {
			case TASK_IDLE:
				xPeriod = pdMS_TO_TICKS(1000);
				break;
			case TASK_INIT:
				xPeriod = pdMS_TO_TICKS(100);
				ctx->state = TASK_RUN;
				break;
			case TASK_RUN:
			xPeriod = pdMS_TO_TICKS(5);
				if (xQueueReceive(ctx->in_buffer, &frame_buffer, 0) == pdPASS) {
					err = _write_jpeg_frame(ctx->mjpeg_handle, frame_buffer);
					if (err != ESP_OK) {
						ESP_LOGE(F_TAG, "Failed to write jpeg frame: %s", esp_err_to_name(err));
						// Even if we fail, we should attempt to send the frame buffer back so that we don't clog up the queue
					}

					if (xQueueSend(ctx->out_buffer, &frame_buffer, 0) != pdPASS) {
						ESP_LOGE(F_TAG, "Failed to send frame buffer to free it up");
					}
				}
				break;
			case TASK_EXIT:
				xPeriod = pdMS_TO_TICKS(1000);
				ctx->state = TASK_IDLE;

				err = _write_final_riff_updates(ctx->mjpeg_handle);
				if (err != ESP_OK) {
					ESP_LOGE(F_TAG, "Failed to update final items of AVI file: %s", esp_err_to_name(err));
				}
				break;
			default:
				ESP_LOGE(F_TAG, "Invalid state. Returning to TASK_IDLE");
				ctx->state = TASK_IDLE;
		}
		vTaskDelay(xPeriod);
	}
}
