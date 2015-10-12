// tutorial02.c
// A pedagogical video player that will stream through every video frame as fast as it can.
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
//
// The code is modified so that it can be compiled to a shared library and run on Android
//
// The code play the video stream on your screen
//
// Feipeng Liu (http://www.roman10.net/)
// Aug 2013


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/avcodec.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define LOG_TAG "android-ffmpeg"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

ANativeWindow* 		window;
char 				*videoFileName;
AVFormatContext 	*formatCtx = NULL;
int 				videoStream;
AVCodecContext  	*codecCtx = NULL;
AVFrame         	*decodedFrame = NULL;
AVFrame         	*frameRGBA = NULL;
jobject				bitmap;
void*				buffer;
struct SwsContext   *sws_ctx = NULL;
int 				width;
int 				height;
int					stop;

const char *filter_descr = "movie=/storage/emulated/0/android-ffmpeg-tutorial02/test.png[wm];[in][wm]overlay=5:5[out]";

jint naInit(JNIEnv *pEnv, jobject pObj, jstring pFileName) {
	AVCodec         *pCodec = NULL;
	int 			i;
	AVDictionary    *optionsDict = NULL;

	videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);
	LOGI("video file name is %s", videoFileName);
	// Register all formats and codecs
	av_register_all();
	// Open video file
	if(avformat_open_input(&formatCtx, videoFileName, NULL, NULL)!=0){
		return -1; // Couldn't open file
	}
	// Retrieve stream information
	if(avformat_find_stream_info(formatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	// Dump information about file onto standard error
	av_dump_format(formatCtx, 0, videoFileName, 0);
	// Find the first video stream
	videoStream=-1;
	for(i=0; i<formatCtx->nb_streams; i++) {
		if(formatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}

	if(videoStream==-1)
		return -1; // Didn't find a video stream
	// Get a pointer to the codec context for the video stream
	codecCtx=formatCtx->streams[videoStream]->codec;
	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(codecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(codecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec
	// Allocate video frame
	decodedFrame=av_frame_alloc();
	// Allocate an AVFrame structure
	frameRGBA=av_frame_alloc();
	if(frameRGBA==NULL)
		return -1;
	return 0;
}

jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
	int i;
	//get Bitmap class and createBitmap method ID
	jclass javaBitmapClass = (jclass)(*pEnv)->FindClass(pEnv, "android/graphics/Bitmap");
	jmethodID mid = (*pEnv)->GetStaticMethodID(pEnv, javaBitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	//create Bitmap.Config
	//reference: https://forums.oracle.com/thread/1548728
	const wchar_t* configName = L"ARGB_8888";
	int len = wcslen(configName);
	jstring jConfigName;
	if (sizeof(wchar_t) != sizeof(jchar)) {
		//wchar_t is defined as different length than jchar(2 bytes)
		jchar* str = (jchar*)malloc((len+1)*sizeof(jchar));
		for (i = 0; i < len; ++i) {
			str[i] = (jchar)configName[i];
		}
		str[len] = 0;
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)str, len);
	} else {
		//wchar_t is defined same length as jchar(2 bytes)
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)configName, len);
	}
	jclass bitmapConfigClass = (*pEnv)->FindClass(pEnv, "android/graphics/Bitmap$Config");
	jobject javaBitmapConfig = (*pEnv)->CallStaticObjectMethod(pEnv, bitmapConfigClass,
			(*pEnv)->GetStaticMethodID(pEnv, bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"), jConfigName);
	//create the bitmap
	return (*pEnv)->CallStaticObjectMethod(pEnv, javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}

jintArray naGetVideoRes(JNIEnv *pEnv, jobject pObj) {
    jintArray lRes;
	if (NULL == codecCtx) {
		return NULL;
	}
	lRes = (*pEnv)->NewIntArray(pEnv, 2);
	if (lRes == NULL) {
		LOGI(1, "cannot allocate memory for video size");
		return NULL;
	}
	jint lVideoRes[2];
	lVideoRes[0] = codecCtx->width;
	lVideoRes[1] = codecCtx->height;
	(*pEnv)->SetIntArrayRegion(pEnv, lRes, 0, 2, lVideoRes);
	return lRes;
}

void naSetSurface(JNIEnv *pEnv, jobject pObj, jobject pSurface) {
	if (0 != pSurface) {
		// get the native window reference
		window = ANativeWindow_fromSurface(pEnv, pSurface);
		// set format and size of window buffer
		ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
	} else {
		// release the native window
		ANativeWindow_release(window);
	}
}

jint naSetup(JNIEnv *pEnv, jobject pObj, int pWidth, int pHeight) {
	width = pWidth;
	height = pHeight;
	//create a bitmap as the buffer for frameRGBA
	bitmap = createBitmap(pEnv, pWidth, pHeight);
	if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0)
		return -1;
	//get the scaling context
	sws_ctx = sws_getContext (
	        codecCtx->width,
	        codecCtx->height,
	        codecCtx->pix_fmt,
	        pWidth,
	        pHeight,
	        AV_PIX_FMT_RGBA,
	        SWS_BILINEAR,
	        NULL,
	        NULL,
	        NULL
	);
	// Assign appropriate parts of bitmap to image planes in pFrameRGBA
	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)frameRGBA, buffer, AV_PIX_FMT_RGBA,
			pWidth, pHeight);
	return 0;
}

void finish(JNIEnv *pEnv) {
	//unlock the bitmap
	AndroidBitmap_unlockPixels(pEnv, bitmap);
	av_free(buffer);
	// Free the RGB image
	av_free(frameRGBA);
	// Free the YUV frame
	av_free(decodedFrame);
	// Close the codec
	avcodec_close(codecCtx);
	// Close the video file
	avformat_close_input(&formatCtx);
}

void decodeAndRender(JNIEnv *pEnv) {
	ANativeWindow_Buffer 	windowBuffer;
	AVPacket        		packet;
	int 					i=0;
	int            			frameFinished;
	int 					lineCnt;
	while(av_read_frame(formatCtx, &packet)>=0 && !stop) {
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {
			// Decode video frame
			avcodec_decode_video2(codecCtx, decodedFrame, &frameFinished,
			   &packet);
			// Did we get a video frame?
			if(frameFinished) {
				// Convert the image from its native format to RGBA
				sws_scale
				(
					sws_ctx,
					(uint8_t const * const *)decodedFrame->data,
					decodedFrame->linesize,
					0,
					codecCtx->height,
					frameRGBA->data,
					frameRGBA->linesize
				);
				LOGI("@@@@@@@@@@@@@@@@@@@@@%d",frameRGBA->data[0]);
				// lock the window buffer
				if (ANativeWindow_lock(window, &windowBuffer, NULL) < 0) {
					LOGE("cannot lock window");
				} else {
					// draw the frame on buffer
//					memcpy(windowBuffer.bits, buffer,  width * height * 4);
				    //skip stride-width 跳过padding部分内存
				    if(windowBuffer.width >= windowBuffer.stride){
                         memcpy(windowBuffer.bits, buffer,  width * height * 4);
                    }else{
                         //skip stride-width 跳过padding部分内存
                         int tmpI = 0;
                         while(tmpI < height){
                             memcpy(windowBuffer.bits + windowBuffer.stride * tmpI * 4, buffer + width * tmpI * 4, width * 4);
                             tmpI++;
                         }
                    }

					// unlock the window buffer and post it to display
					ANativeWindow_unlockAndPost(window);
					// count number of frames
					++i;
				}
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
	LOGI("total No. of frames decoded and rendered %d", i);
	finish(pEnv);
}

/**
 * start the video playback
 */
void naPlay(JNIEnv *pEnv, jobject pObj) {
	//create a new thread for video decode and render
	pthread_t decodeThread;
	stop = 0;
	pthread_create(&decodeThread, NULL, decodeAndRender, NULL);
}

/**
 * stop the video playback
 */
void naStop(JNIEnv *pEnv, jobject pObj) {
	stop = 1;
}

/**
  * cut video
  */
int naCutVideo(JNIEnv *pEnv, jobject pObj,jstring inputFile,jstring outFile,jstring mp4File,jint startTime,jint length) {
//    ANativeWindow* 		window;
    char 				*inputFileName;
    AVFormatContext 	*inputFormatCtx = NULL;
    int 				videoStreamIndex;
    AVCodecContext  	*inputCodecContext = NULL;
    AVFrame         	*inputFrame = NULL;
    AVFrame         	*inputRGBFrame = NULL;
    jobject				inputBitmap;
    void*				inputBuffer;
    struct SwsContext   *sws_ctx = NULL;
//
    AVCodec         *pCodec = NULL;
    int 			i;
    AVDictionary    *optionsDict = NULL;

    AVPacket        		packet;
    int 					j=0;
    int            			frameFinished;
    int 					lineCnt;
    FILE *dst_file;
    const char *dst_filename = NULL;
    char *out_filename = NULL;
    char *mp4_filename = NULL;

    //for filter
    AVFilterGraph *filter_graph;
    char args[512];
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;

    LOGI("START INIT");
    inputFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, inputFile, NULL);
    out_filename = (char *)(*pEnv)->GetStringUTFChars(pEnv, outFile, NULL);
    mp4_filename = (char *)(*pEnv)->GetStringUTFChars(pEnv, mp4File, NULL);
    	// Register all formats and codecs
    av_register_all();

    	// Open video file
    LOGI("input file name= %s",inputFileName);
    LOGI("out file name =%s",out_filename);
    LOGI("mp4 file name = %s",mp4_filename);
    if(avformat_open_input(&inputFormatCtx, inputFileName, NULL, NULL)!=0){
        LOGE("can't open input file");
    	return -1; // Couldn't open file
    }

    LOGI("open input sec,formatCCtx duration = %d",inputFormatCtx->duration);
    	// Retrieve stream information
    if(avformat_find_stream_info(inputFormatCtx, NULL)<0)
    	return -1; // Couldn't find stream information
    	// Dump information about file onto standard error
    av_dump_format(inputFormatCtx, 0, inputFileName, 0);
    	// Find the first video stream
    videoStreamIndex=-1;
    for(i=0; i<inputFormatCtx->nb_streams; i++) {
    		if(inputFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
    			videoStreamIndex=i;
    			break;
    		}
    }

    if(videoStreamIndex==-1)
    	return -1; // Didn't find a video stream
    // Get a pointer to the codec context for the video stream
    inputCodecContext=inputFormatCtx->streams[videoStreamIndex]->codec;
    	// Find the decoder for the video stream
    pCodec=avcodec_find_decoder(inputCodecContext->codec_id);
    LOGI("codec name = %s",pCodec->name);
    if(pCodec==NULL) {
    	fprintf(stderr, "Unsupported codec!\n");
    	return -1; // Codec not found
    }
    // Open codec
    if(avcodec_open2(inputCodecContext, pCodec, &optionsDict)<0){
        LOGE("counldn't open with codec");
    	return -1; // Could not open codec
    }

    //init filter for water card
    avfilter_register_all();

    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("ffbuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
    AVBufferSinkParams *buffersink_params;

    filter_graph = avfilter_graph_alloc();

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args,sizeof(args),"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                inputCodecContext->width, inputCodecContext->height, inputCodecContext->pix_fmt,
                inputCodecContext->time_base.num, inputCodecContext->time_base.den,
                inputCodecContext->sample_aspect_ratio.num, inputCodecContext->sample_aspect_ratio.den);

    if(buffersrc_ctx == NULL){
        LOGE("buffersrc_ctx == null");
    }

     if(buffersrc == NULL){
            LOGE("buffersrc== null");
        }

         if(filter_graph == NULL){
                LOGE("filter_graph == null");
            }
    if (avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                                   args, NULL, filter_graph) < 0) {
        LOGE("Cannot create buffer source\n");
    }

    /* buffer video sink: to terminate the filter chain. */
    buffersink_params = av_buffersink_params_alloc();
    buffersink_params->pixel_fmts = pix_fmts;
    if (avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                                   NULL, buffersink_params, filter_graph) < 0) {
            LOGE("Cannot create buffer sink\n");
     }
    av_free(buffersink_params);
    // end init filter

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    if (avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, &outputs, NULL) < 0){
        LOGE("error parse ptr");
    }

    if (avfilter_graph_config(filter_graph, NULL) < 0){
        LOGE("error graph config");
    }

    LOGI("start alloc frame");
    // Allocate video frame
    inputFrame=av_frame_alloc();

    // Allocate an AVFrame structure
    inputRGBFrame=av_frame_alloc();
    if(inputRGBFrame==NULL)
    	return -1;

    inputBitmap = createBitmap(pEnv, inputCodecContext->width, inputCodecContext->height);
	if (AndroidBitmap_lockPixels(pEnv, inputBitmap, &inputBuffer) < 0)
		return -1;
    //get the scaling context
    sws_ctx = sws_getContext (
    	        inputCodecContext->width,
    	        inputCodecContext->height,
    	        inputCodecContext->pix_fmt,
    	        inputCodecContext->width,
                inputCodecContext->height,
    	        PIX_FMT_YUV420P,
    	        SWS_BILINEAR,
    	        NULL,
    	        NULL,
    	        NULL
    );
    // Assign appropriate parts of bitmap to image planes in pFrameRGBA
    	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
    	// of AVPicture
    avpicture_fill((AVPicture *)inputRGBFrame, inputBuffer, PIX_FMT_YUV420P,
    		 inputCodecContext->width, inputCodecContext->height);


    if(!sws_ctx){
        LOGE("get sws context fail");
        return -1;
    }

    dst_filename = (char *)(*pEnv)->GetStringUTFChars(pEnv, outFile, NULL);
    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        LOGE("Could not open destination file %s\n", dst_filename);
        return;
    }
    while(av_read_frame(inputFormatCtx, &packet)>=0) {
        avcodec_get_frame_defaults(inputFrame);

        AVFilterBufferRef *picref;
    	// Is this a packet from the video stream?
    	if(packet.stream_index==videoStreamIndex) {
    			// Decode video frame
    	    avcodec_decode_video2(inputCodecContext, inputFrame, &frameFinished,
    			   &packet);
    			// Did we get a video frame?
    		if(frameFinished) {

//    			// Convert the image from its native format to RGBA
//    			// 去除多余长度
//    			LOGI("ragFrame length = %d",sizeof(inputRGBFrame->data));
//    			int ret = sws_scale
//    			(
//    					sws_ctx,
//    					(uint8_t const * const *)inputFrame->data,
//    					inputFrame->linesize,
//    					0,
//    					inputCodecContext->height,
//    					inputRGBFrame->data,
//    					inputRGBFrame->linesize
//    			);
//    			LOGI("frame.linesize = %d width = %d  length = %d&& rabFrameLinesize = %d width= %d data length = %d",inputFrame->linesize,inputFrame->width,sizeof(inputFrame->data),inputRGBFrame->linesize,inputRGBFrame->width,sizeof(inputRGBFrame->data));
//    			if(ret<0){
//    				LOGE("swsscale fail");
//    				return;
//    			}

                // add the water card ..
    			inputFrame->pts = av_frame_get_best_effort_timestamp(inputFrame);

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame(buffersrc_ctx, inputFrame) < 0) {
                    LOGE( "Error while feeding the filtergraph %d\n",av_buffersrc_add_frame(buffersrc_ctx, inputRGBFrame));
                }

                /* pull filtered pictures from the filtergraph */
                while (1) {
                    int ret = av_buffersink_get_buffer_ref(buffersink_ctx, &picref, 0);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        LOGE("error get buffer ref");

                    if (picref) {

                        int y_size=picref->video->w*picref->video->h;
                        fwrite(picref->data[0],1,y_size,dst_file);     //Y
                        fwrite(picref->data[1],1,y_size/4,dst_file);   //U
                        fwrite(picref->data[2],1,y_size/4,dst_file);   //V

                        avfilter_unref_bufferp(&picref);
                    }
                }
    		}
    	}
    	// Free the packet that was allocated by av_read_frame
    	av_free_packet(&packet);
    }
    LOGI("total No. of frames decoded and rendered %d", j);
    fclose(dst_file);

    encodeVideo(out_filename,mp4_filename);
}

/**
  * encode video
  */
int encodeVideo(char *inputFile,char *mp4File) {
    AVFormatContext  *mp4FormatContext;
    AVOutputFormat   *mp4OutputFormat;
    AVStream    *video_st;

    AVCodecContext *mp4CodecContext;
    AVCodec *mp4Codec;

    uint8_t *picture_buf;
    AVFrame *picture;
    int size;

    int i;

    FILE *yuvFile = fopen(inputFile,"rb");

    LOGI("start format = %s",mp4File);
//    mp4OutputFormat = av_guess_format(NULL, mp4File, NULL);
//    LOGI("264format = %s",mp4OutputFormat->name);
//    mp4FormatContext = avformat_alloc_context();
//    mp4FormatContext->oformat = mp4OutputFormat;
//    LOGI("file name %s",mp4FormatContext->filename);

    // 不使用guess 强制使用 mpeg4, NOTE: guess方式不是根据文件后缀获得
    avformat_alloc_output_context2(&mp4FormatContext,NULL,"m4v",mp4File);
    mp4OutputFormat = mp4FormatContext->oformat;

    if(!mp4OutputFormat){
        LOGE("guess format error");
        return -1;
    }


    if(avio_open(&mp4FormatContext->pb,mp4File,AVIO_FLAG_READ_WRITE)<0){
        LOGE("can't open outFile--%s",mp4File);
        return -1;
    }

    video_st = NULL;
    LOGI("video codec = %d",mp4OutputFormat->video_codec);
    if (mp4OutputFormat->video_codec != CODEC_ID_NONE)
        {
            video_st = avformat_new_stream(mp4FormatContext, 0);
            if(video_st == NULL){
                LOGE("new stream fail");
                return -1;
            }

            mp4CodecContext = video_st->codec;
            mp4CodecContext->codec_id = mp4OutputFormat->video_codec;
            mp4CodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
            mp4CodecContext->bit_rate = 400000;
            mp4CodecContext->width = 640;
            mp4CodecContext->height = 352;
            mp4CodecContext->time_base.num = 1;
            mp4CodecContext->time_base.den = 25;
            mp4CodecContext->gop_size = 12;
            mp4CodecContext->pix_fmt = PIX_FMT_YUV420P;
            if (mp4CodecContext->codec_id == CODEC_ID_MPEG2VIDEO)
            {
                mp4CodecContext->max_b_frames = 2;
            }
            if (mp4CodecContext->codec_id == CODEC_ID_MPEG1VIDEO)
            {
                mp4CodecContext->mb_decision = 2;
            }
            if (!strcmp(mp4FormatContext->oformat->name, "mp4") || !strcmp(mp4FormatContext->oformat->name, "mov") || !strcmp(mp4FormatContext->oformat->name, "3gp"))
            {
                mp4CodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
        }
//    if(avformat_open_input(&mp4FormatContext, mp4File, NULL, NULL)!=0){
//
//    	return -1; // Couldn't open file
//    }
    av_dump_format(mp4FormatContext, 0, mp4File, 1);
    LOGI("find encoder codec_id = %d And h264id = %d",mp4CodecContext->codec_id,CODEC_ID_MPEG4);
//    mp4Codec = avcodec_find_encoder(mp4CodecContext->codec_id);
    mp4Codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);

    if(!mp4Codec){
        LOGE("没有编码器");
        return -1;
    }

    if(avcodec_open2(mp4CodecContext,mp4Codec,NULL) < 0){
        LOGE("打开编码器失败");
        return -1;
    }

    picture = av_frame_alloc();
    size = avpicture_get_size(mp4CodecContext->pix_fmt, mp4CodecContext->width, mp4CodecContext->height);
    picture_buf = (uint8_t *)av_malloc(size);
    avpicture_fill((AVPicture *)picture, picture_buf, mp4CodecContext->pix_fmt, mp4CodecContext->width, mp4CodecContext->height);

     avformat_write_header(mp4FormatContext,NULL);

     AVPacket pkt;
     int y_size = mp4CodecContext->width * mp4CodecContext->height;
     LOGI("ysize = %d",y_size);
    av_new_packet(&pkt,y_size*3);


    for (i=0; i<20; i++)
    {
        LOGI("i = %d",i);
//            if (video_st)
//            {
//                video_pts = (double)(video_st->pts.val * video_st->time_base.num / video_st->time_base.den);
//            }
//            else
//            {
//                video_pts = 0.0;
//            }
        if (!video_st/* || video_pts >= 5.0*/)
        {
            break;
        }

        if (fread(picture_buf, 1, y_size*3/2, yuvFile) < 0)
        {
            break;
        }

        picture->data[0] = picture_buf;  // 亮度
        picture->data[1] = picture_buf+ y_size;  // 色度
        picture->data[2] = picture_buf+ y_size*5/4; // 色度

        picture->pts=i;

        int got_picture=0;
         //Encode 编码
        int ret = avcodec_encode_video2(mp4CodecContext, &pkt,picture, &got_picture);
        if(ret < 0){
             LOGE("Failed to encode! 编码错误！%d\n",ret);
             return -1;
        }
        LOGI("247145885 result = %d",ret);
        if (got_picture==1){
            LOGE("Succeed to encode 1 frame! 编码成功1帧！\n");
            pkt.stream_index = video_st->index;
            ret = av_write_frame(mp4FormatContext, &pkt);
            if(ret<0){
                LOGE("write frame fail!");
                return -1;
            }
            av_free_packet(&pkt);
        }
    }

    //Flush Encoder
    LOGI("start flush encoder");
    int ret = flush_encoder(mp4FormatContext,0);
    if (ret < 0) {
         printf("Flushing encoder failed\n");
         return -1;
    }
    LOGI("start write encode tailler");
        //Write file trailer 写文件尾
    av_write_trailer(mp4FormatContext);
    LOGI("free bojects");

    //Clean 清理
    if (video_st){
        avcodec_close(video_st->codec);
        av_free(picture);
        av_free(picture_buf);
    }
    avio_close(mp4FormatContext->pb);
    avformat_free_context(mp4FormatContext);

    fclose(yuvFile);
    LOGI("complete encode");
    return 0;

            // 如果是rgb序列，可能需要如下代码
    //      SwsContext* img_convert_ctx;
    //      img_convert_ctx = sws_getContext(c->width, c->height, PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    //      sws_scale(img_convert_ctx, pictureRGB->data, pictureRGB->linesize, 0, c->height, picture->data, picture->linesize);

//            if (mp4OutputFormat->oformat->flags & AVFMT_RAWPICTURE)
//            {
//                AVPacket pkt;
//                av_init_packet(&pkt);
//                pkt.flags |= PKT_FLAG_KEY;
//                pkt.stream_index = video_st->index;
//                pkt.data = (uint8_t*)picture;
//                pkt.size = sizeof(AVPicture);
//                ret = av_write_frame(oc, &pkt);
//            }
//            else
//            {
//                int out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);
//                if (out_size > 0)
//                {
//                    AVPacket pkt;
//                    av_init_packet(&pkt);
//                    pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, video_st->time_base);
//                    if (c->coded_frame->key_frame)
//                    {
//                        pkt.flags |= PKT_FLAG_KEY;
//                    }
//                    pkt.stream_index = video_st->index;
//                    pkt.data = video_outbuf;
//                    pkt.size = out_size;
//                    ret = av_write_frame(mp4OutputFormat, &pkt);
//                }
//            }
//        }

//        if (video_st)
//        {
//            avcodec_close(video_st->codec);
//    //      av_free(picture->data[0]);
//            av_free(picture);
//            av_free(video_outbuf);
//    //      av_free(picture_buf);
//        }
//        av_write_trailer(mp4OutputFormat);
//        for (int i=0; i<oc->nb_streams; i++)
//        {
//            av_freep(&mp4OutputFormat->streams[i]->codec);
//            av_freep(&mp4OutputFormat->streams[i]);
//        }
//        if (!(fmt->flags & AVFMT_NOFILE))
//        {
//            url_fclose(mp4OutputFormat->pb);
//        }
//        av_free(mp4OutputFormat);
}

int flush_encoder(AVFormatContext *fmt_ctx,unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
        CODEC_CAP_DELAY)){
        LOGE("capabilities error")
        return 0;
    }
    LOGI("start while");
    while (1) {
        LOGI("Flushing stream #%u encoder\n", stream_index);
        //ret = encode_write_frame(NULL, stream_index, &got_frame);
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2 (fmt_ctx->streams[stream_index]->codec, &enc_pkt,
            NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame){
            ret=0;
            break;
        }
        LOGI("Succeed to encode 1 frame! 编码成功1帧！\n");
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}


jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}
	JNINativeMethod nm[8];
	nm[0].name = "naInit";
	nm[0].signature = "(Ljava/lang/String;)I";
	nm[0].fnPtr = (void*)naInit;

	nm[1].name = "naSetSurface";
	nm[1].signature = "(Landroid/view/Surface;)V";
	nm[1].fnPtr = (void*)naSetSurface;

	nm[2].name = "naGetVideoRes";
	nm[2].signature = "()[I";
	nm[2].fnPtr = (void*)naGetVideoRes;

	nm[3].name = "naSetup";
	nm[3].signature = "(II)I";
	nm[3].fnPtr = (void*)naSetup;

	nm[4].name = "naPlay";
	nm[4].signature = "()V";
	nm[4].fnPtr = (void*)naPlay;

	nm[5].name = "naStop";
	nm[5].signature = "()V";
	nm[5].fnPtr = (void*)naStop;

	nm[6].name = "naCutVideo";
    nm[6].signature = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)I";
    nm[6].fnPtr = (void*)naCutVideo;

	jclass cls = (*env)->FindClass(env, "com/example/FFmpeg/Tutorial2Activity");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, 7);
	return JNI_VERSION_1_6;
}

