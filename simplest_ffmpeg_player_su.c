/**
 * 最简单的基于FFmpeg的视频播放器SU(SDL升级版)
 * Simplest FFmpeg Player (SDL Update)
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 本程序实现了视频文件的解码和显示（支持HEVC，H.264，MPEG2等）。
 * 是最简单的FFmpeg视频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 * 本版本中使用SDL消息机制刷新视频画面。
 * This software is a simplest video player based on FFmpeg.
 * Suitable for beginner of FFmpeg.
 * 
 * Version:1.2
 * 
 * 备注:
 * 标准版在播放视频的时候，画面显示使用延时40ms的方式。这么做有两个后果：
 * （1）SDL弹出的窗口无法移动，一直显示是忙碌状态
 * （2）画面显示并不是严格的40ms一帧，因为还没有考虑解码的时间。
 * SU（SDL Update）版在视频解码的过程中，不再使用延时40ms的方式，而是创建了
 * 一个线程，每隔40ms发送一个自定义的消息，告知主函数进行解码显示。这样做之后：
 * （1）SDL弹出的窗口可以移动了
 * （2）画面显示是严格的40ms一帧
 * Remark:
 * Standard Version use's SDL_Delay() to control video's frame rate, it has 2
 * disadvantages:
 * (1)SDL's Screen can't be moved and always "Busy".
 * (2)Frame rate can't be accurate because it doesn't consider the time consumed 
 * by avcodec_decode_video2()
 * SU（SDL Update）Version solved 2 problems above. It create a thread to send SDL 
 * Event every 40ms to tell the main loop to decode and show video frames.
 */


#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>
#ifdef __cplusplus
};
#endif
#endif

//Refresh
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

int thread_exit=0;
//Thread
int sfp_refresh_thread(void *opaque)
{
	SDL_Event event;
	while (thread_exit==0) {
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		//Wait 40 ms
		SDL_Delay(40);
	}
	return 0;
}


int main(int argc, char* argv[])
{
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame,*pFrameYUV;
	AVPacket *packet;
	struct SwsContext *img_convert_ctx;
	//SDL
	int ret, got_picture;
	int screen_w=0,screen_h=0;
	SDL_Surface *screen; 
	SDL_Overlay *bmp; 
	SDL_Rect rect;
	SDL_Thread *video_tid;
	SDL_Event event;
//valid_h264_yuy2.h264  bigbuckbunny_480x272.h265
	char filepath[]="bigbuckbunny_480x272.h265";
	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	
	if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
	if(videoindex==-1){
		printf("Didn't find a video stream.\n");
		return -1;
	}
	pCodecCtx=pFormatCtx->streams[videoindex]->codec;
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
	{
		printf("Could not open codec.\n");
		return -1;
	}

	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();
	//uint8_t *out_buffer=(uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	//avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
//------------SDL----------------
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	} 

	
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	screen = SDL_SetVideoMode(screen_w, screen_h, 0,0);

	if(!screen) {  
		printf("SDL: could not set video mode - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen); 
	
	rect.x = 0;    
	rect.y = 0;    
	rect.w = screen_w;    
	rect.h = screen_h;  

	packet=(AVPacket *)av_malloc(sizeof(AVPacket));

	printf("---------------File Information------------------\n");
	av_dump_format(pFormatCtx,0,filepath,0);
	printf("-------------------------------------------------\n");
	
	//AV_PIX_FMT_YUV420P   PIX_FMT_YUV420P
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 
	//--------------
	video_tid = SDL_CreateThread(sfp_refresh_thread,NULL);
	//
	SDL_WM_SetCaption("Simple FFmpeg Player (SDL Update)",NULL);

	//Event Loop
	
	for (;;) {
		//Wait
		SDL_WaitEvent(&event);
		if(event.type==SFM_REFRESH_EVENT){
			//------------------------------
			if(av_read_frame(pFormatCtx, packet)>=0){
				if(packet->stream_index==videoindex){
					ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
					if(ret < 0){
						printf("Decode Error.\n");
						return -1;
					}
					if(got_picture){

						SDL_LockYUVOverlay(bmp);
						pFrameYUV->data[0]=bmp->pixels[0];
						pFrameYUV->data[1]=bmp->pixels[2];
						pFrameYUV->data[2]=bmp->pixels[1];     
						pFrameYUV->linesize[0]=bmp->pitches[0];
						pFrameYUV->linesize[1]=bmp->pitches[2];   
						pFrameYUV->linesize[2]=bmp->pitches[1];
						sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

						SDL_UnlockYUVOverlay(bmp); 
						
						SDL_DisplayYUVOverlay(bmp, &rect); 

					}
				}
				av_free_packet(packet);
			}else{
				//Exit Thread
				thread_exit=1;
				break;
			}
		}

	}
	
	SDL_Quit();

	sws_freeContext(img_convert_ctx);

	//--------------
	//av_free(out_buffer);
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}

