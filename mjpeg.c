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

#include "esp_log.h"

esp_err_t _write_riff_header(mjpeg_handle_t mjpeg_handle) {
	const char F_TAG[] = "write-riff-header";
	esp_err_t err = ESP_OK;

	uint32_t buffer[16] = {0x00};

	// We do not know what the size of the riff will end up being until after we are done writing everything.
	// Thus, we should save ourselves space for us to update the riff size later

	// The RIFF keyword and the size of the file are not included in the "riff size"
	// Therefore, technically, we can consider the riff size as: total file size - 8 bytes

	// Technically, for all the sizes, we could calculate them at compile time, but it is easier for it to be dynamic as it is because we are doing init
	buffer[0] = FOURCC_RIFF;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->out_file_handle->payload.data		= buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp RIFF size to file: %s", esp_err_to_name(err));
		return err;
	}

	// Write AVI keyword
	buffer[0] = FOURCC_AVI;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->out_file_handle->payload.data		= buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp HDRL size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(uint32_t);

	// Write HDRL keyword
	buffer[0] = FOURCC_HDRL;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->hdrl_size += sizeof(avih);

	// Write LIST keyword
	buffer[0] = FOURCC_LIST;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->out_file_handle->payload.data		= buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp STRL size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->hdrl_size += sizeof(uint32_t);
	
	// Write STRL keyword
	buffer[0] = FOURCC_STRL;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->strl_size += sizeof(strh);

	// Write BMPH keyword and size of bmph
	buffer[0] = FOURCC_BMPH;
	buffer[1] = sizeof(ctx->bmph);
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC) + sizeof(size_t);
	ctx->out_file_handle->payload.data		= (char *)buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write BMPH keyword and size to file: %s", esp_err_to_name(err));
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
	ctx->out_file_handle->payload.data		= buffer;
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
	ctx->out_file_handle->payload.data		= buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write temp MOVI size to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->riff_size += sizeof(uint32_t);

	// Write MOVI keyword
	buffer[0] = FOURCC_MOVI;
	ctx->out_file_handle->payload.current_data_len	= sizeof(FOURCC);
	ctx->out_file_handle->payload.data		= buffer;
	err = write_file(ctx->out_file_handle);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write MOVI keyword to file: %s", esp_err_to_name(err));
		return err;
	}
	ctx->movi_size += sizeof(FOURCC);

	return err;	
}

void mjpeg_svc(void *pvParameters) {
	const char *F_TAG = pcTaskGetName(NULL);
	esp_err_t err = ESP_OK;

	ESP_LOGI(F_TAG, "Entered on core %d", xPortGetCoreID());

	mjpeg_svc_handle_t ctx = (mjpeg_svc_handle_t)pvParameters;

	mjpeg_handle_t mjpeg_handle = ctx->mjpeg_handle;
	QueueHandle_t out_buffer = ctx->out_buffer;
	QueueHandle_t in_buffer = ctx->in_buffer;

	frame_buffer_t frame_buffer;

	// Write header
	err = write_riff_header(ctx);
	if (err != ESP_OK) {
		ESP_LOGE(F_TAG, "Failed to write the riff header for the AVI file: %s", esp_err_to_name(err));
		vTaskDelete(NULL);
	}
	ESP_LOGI(F_TAG, "Header information written");

	while (true) {
		
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

/*
int main(int argc, char const *argv[])
{
	int argi, fps = DEFAULT_FPS, width, height;
	const char *outPath = NULL, *sndPath = NULL;
	fpos_t riffPos, hdrlPos, strlPos, moviPos, sndDataPos, sndFmtPos;
	size_t riffSize, hdrlSize, strlSize, moviSize, read, sndFmtSize;
	AVIH avih;
	STRH strh;
	BMPH bmph;
	VPRP vprp;
	int frame = 0;
	FILE *out, *idx, *snd = NULL, *in;
	mp3header_t mp3 = 0;
	double videoFrameLength, audio = 0, video = 0;

	for(argi = 1; argi < argc && *argv[argi] == '-'; argi++) {
		if(!strcmp(argv[argi], "-h")) {
			help(argv[0]);
			return 0;
		} else if(!strcmp(argv[argi], "-o") && argi + 1 < argc) {
			outPath = argv[++argi];
		} else if(!strcmp(argv[argi], "-s") && argi + 1 < argc) {
			sndPath = argv[++argi];
		} else if(!strcmp(argv[argi], "-f") && argi + 1 < argc) {
			fps = atoi(argv[++argi]);
			if(fps == 0) {
				fprintf(stderr, "Error: Invalid FPS value `%s'.\n", argv[argi]);
				return 255;
			}
		}
	}

	if(argi >= argc) {
		help(argv[0]);
		return 255;
	}

	if(outPath && !(out = fopen(outPath, "w+b"))) {
		fprintf(stderr, "Error: Cannot open output `%s'.\n", outPath);
		return 2;
	} else if(!out) {
		out = stdout;
	}

	videoFrameLength = 1.0 / fps;

	if(!(idx = tmpfile())) {
		fprintf(stderr, "Error: Cannot create temporary index for `%s'.\n", outPath ?: "(stdout)");
		return 3;
	}

	// We do not know what the size of the riff will end up being until after we are done writing everything.
	// Thus, we should save ourselves space for us to update the riff size later

	// The RIFF keyword and the size of the file are not included in the "riff size"
	// Therefore, technically, we can consider the riff size as: total file size - 8 bytes
	fwritecc(FOURCC_RIFF, out);

	// Store the position here
	fgetpos(out, &riffPos);
	fwritechunk(FOURCC_RIFF, 0, out);
	riffSize = fwritecc(FOURCC_AVI, out);

		fgetpos(out, &hdrlPos);
		riffSize += fwritechunk(FOURCC_LIST, 0, out);
		hdrlSize = fwritecc(FOURCC_HDRL, out);

		hdrlSize += fwritechunk(FOURCC_AVIH, sizeof(avih), out);
		memset(&avih, 0, sizeof(avih));
		avih.microSecPerFrame = 1000000 / fps;
		avih.maxBytesPerSec = 45000;
		avih.flags = AVIF_HASINDEX | AVIF_ISINTERLEAVED | AVIF_TRUSTCKTYPE;
		avih.totalFrames = argc - argi;
		avih.streams = snd ? 2 : 1;
		avih.width = width;
		avih.height = height;
		avih.suggestedBufferSize = 1024*1024;
		hdrlSize += fwrite(&avih, 1, sizeof(avih), out);

		fprintf(stderr, "AVI `%s' %dx%d %d frames\n", outPath, avih.width, avih.height, avih.totalFrames);

			fgetpos(out, &strlPos);
			hdrlSize += fwritechunk(FOURCC_LIST, 0, out);
			strlSize = fwritecc(FOURCC_STRL, out);

				strlSize += fwritechunk(FOURCC_STRH, sizeof(strh), out);
				memset(&strh, 0, sizeof(strh));
				strh.type = FOURCC_VIDS;
				strh.handler = CC("MJPG");
				strh.scale   = 1;
				strh.rate    = fps;
				strh.quality = (uint32_t)-1;
				strh.length  = avih.totalFrames;
				strh.suggestedBufferSize = avih.suggestedBufferSize;
				strh.frame.right  = avih.width;
				strh.frame.bottom = avih.height;
				strlSize += fwrite(&strh, 1, sizeof(strh), out);

				strlSize += fwritechunk(FOURCC_STRF, sizeof(bmph), out);
				memset(&bmph, 0, sizeof(bmph));
				bmph.size     = sizeof(bmph);
				bmph.width    = avih.width;
				bmph.height   = avih.height;
				bmph.planes   = 1;
				bmph.bitCount = 24;
				bmph.imgSize  = bmph.width * bmph.height * bmph.bitCount / 8;
				bmph.compression = CC("MJPG");
				strlSize += fwrite(&bmph, 1, sizeof(bmph), out);

				strlSize += fwritechunk(FOURCC_VPRP, sizeof(vprp), out);
				memset(&vprp, 0, sizeof(vprp));
				vprp.verticalRefreshRate = fps;
				vprp.hTotalInT           = avih.width;
				vprp.vTotalInLines       = avih.height;
				vprp.frameAspectRatio    = ASPECT_3_2;
				vprp.frameWidthInPixels  = avih.width;
				vprp.frameHeightInLines  = avih.height;
				vprp.fieldsPerFrame      = 1;
				vprp.field.compressedBMHeight = avih.height;
				vprp.field.compressedBMWidth  = avih.width;
				vprp.field.validBMHeight      = avih.height;
				vprp.field.validBMWidth       = avih.width;
				strlSize += fwrite(&vprp, 1, sizeof(vprp), out);

			fupdate(out, &strlPos, strlSize);
			hdrlSize += strlSize;

		fupdate(out, &hdrlPos, hdrlSize);
		riffSize += hdrlSize;

		fgetpos(out, &moviPos);
		riffSize += fwritechunk(FOURCC_LIST, 0, out);
		moviSize = fwritecc(FOURCC_MOVI, out);

		while(1) {
			if(frame + argi >= argc) break;

			in = fopen(argv[frame + argi], "rb");
			if(!in) {
				moviSize += fwritechunk(CC("00dc"), 0, out);
			} else {
				IDX1 idx1 = { CC("00dc"), 0, moviSize, 0 };
				uint32_t buf[1024];
				fseek(in, 0, SEEK_END);
				idx1.size = ftell(in);
				fwrite(&idx1, 1, sizeof(idx1), idx);
				moviSize += fwritechunk(CC("00dc"), idx1.size, out);
				fseek(in, 0, SEEK_SET);
				while(1) {
					read = fread(buf, 1, sizeof(buf), in);
					if(read > 0) {
						// NOTE: Little trick here, ensure we write 2 byte padded data
						moviSize += fwrite(buf, 1, read + (read % 2), out);
					}
					if(read < sizeof(buf)) break;
				}
			}
			video += videoFrameLength;
			frame ++;
			fclose(in);
		}

		fupdate(out, &moviPos, moviSize);
		riffSize += moviSize;

		// rewrite index
		if(idx) {
			uint32_t buf[1024];
			moviSize += fwritechunk(FOURCC_IDX1, ftell(idx), out);
			fseek(idx, 0, SEEK_SET);
			while(1) {
				read = fread(buf, 1, sizeof(buf), idx);
				if(read > 0) {
					// NOTE: Little trick here, ensure we write 2 byte padded data
					riffSize += fwrite(buf, 1, read + (read % 2), out);
				}
				if(read < sizeof(buf)) break;
			}
		}

	fupdate(out, &riffPos, riffSize);

	if(out && out != stdout) fclose(out);
	if(idx) fclose(idx);
	if(snd) fclose(snd);

	return 0;
}
*/
