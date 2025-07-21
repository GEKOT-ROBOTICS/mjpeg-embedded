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

void mjpeg_svc(void *pvParameters) {
	const char *F_TAG = pcTaskGetName(NULL);

	ESP_LOGI(F_TAG, "Entered on core %d", xPortGetCoreID());

	mjpeg_svc_handle_t ctx = (mjpeg_svc_handle_t)pvParameters;

	mjpeg_handle_t mjpeg_handle = ctx->mjpeg_handle;
	QueueHandle_t out_buffer = ctx->out_buffer;
	QueueHandle_t in_buffer = ctx->in_buffer;

	frame_buffer_t frame_buffer;

	ESP_LOGI(F_TAG, "Writing header information for .avi file");

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
