/* 服务端程序 server.c */ 
//WB
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <setjmp.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include "convert.h"

#include "../avc-src-0.14/avc/common/T264.h"

#define SERVER_PORT 8888 
 
T264_t* m_t264;
T264_param_t m_param;
char* m_pSrc;
char* m_pDst;
int* m_lDstSize;
char* m_pPoolData;


#define USB_VIDEO "/dev/video0"
int cam_fd;
struct video_mmap cam_mm;/*视频内存映射*/
/*包含摄像头的基本信息，例如设备名称、
支持的最大最小分辨率、信号源信息等*/
struct video_capability cam_cap;
/*亮度、对比度等和voide_mmap中的分辨率*/
struct video_picture cam_pic;
struct video_mbuf cam_mbuf;/*摄像头存储缓冲区的帧信息*/
struct video_window win;/* 设备采集窗口参数*/
char *cam_data = NULL;
int nframe;

void read_video(char *pixels,int w, int h)
{
  int ret;
  int frame=0; 
  cam_mm.width = w;
  cam_mm.height = h;
 /* 对于单帧采集只需设置 frame=0*/
  cam_mm.frame = 0;
  cam_mm.format=VIDEO_PALETTE_YUV420P; //change by 091215
/*  若调用成功，则激活设备真正开始一帧图像的截取，是非阻塞的*/
  ret = ioctl(cam_fd,VIDIOCMCAPTURE,&cam_mm);
  if( ret<0 ) {
    printf("ERROR: VIDIOCMCAPTURE\n");
  }
/*  函数判断该帧图像是否截取完毕，成功返回表示截取完毕*/
  ret = ioctl(cam_fd,VIDIOCSYNC,&frame);
  if( ret<0 ) {
    printf("ERROR: VIDIOCSYNC\n");
  }
}

void config_vid_pic()
{
  char cfpath[100];
  FILE *cf;
  int ret;
  if (ioctl(cam_fd, VIDIOCGPICT, &cam_pic) < 0) {
    printf("ERROR:VIDIOCGPICT\n");
  }
  /*图像采集格式,网眼V2000只支持VIDEO_PALETTE_YUV420P*/
  cam_pic.palette = VIDEO_PALETTE_YUV420P; //change by 091215

#if 0
    cam_pic.brightness  = 30464;
    cam_pic.hue = 36000;
    cam_pic.colour = 0;
    cam_pic.contrast = 43312;
    cam_pic.whiteness = 10312;
    cam_pic.depth =         24;
#endif
    cam_pic.brightness  =	30464;
    cam_pic.hue =		111;
    cam_pic.colour =		555;
    cam_pic.contrast =		43312;
    cam_pic.whiteness =		111;
    /*VIDEO_PALETTE_YUV420,bpp=12bit*/
    cam_pic.depth =         12; //bpp == bytes per pixel,change by 091215
    /*设置摄像头缓冲中voideo_picture信息*/
    ret = ioctl( cam_fd, VIDIOCSPICT,&cam_pic );      

    if( ret<0 ) {
      close(cam_fd);
      printf("ERROR: VIDIOCSPICT,Can't set video_picture format\n");
    }
    return;
}


void init_video(int w,int h) /* bpp == bytes per pixel*/
{
  int ret;
  
  /*设备的打开*/
  cam_fd = open( USB_VIDEO, O_RDWR );
  if( cam_fd<0 )
    printf("Can't open video device\n");
	/* 使用IOCTL命令VIDIOCGCAP，获取摄像头的基本信息，如最大，最小分辨率*/
  
  ret = ioctl( cam_fd,VIDIOCGCAP,&cam_cap );       /* 摄像头的基本信息*/
  if( ret<0 ) {
    printf("Can't get device information: VIDIOCGCAP\n");
  }
  printf("Device name:%s\nWidth:%d ~ %d\nHeight:%d ~ %d\n",cam_cap.name, cam_cap.maxwidth, cam_cap.minwidth, cam_cap.maxheight, cam_cap.minheight);
  if( ioctl(cam_fd,VIDIOCGWIN,&win)<0 ) {
    printf("ERROR:VIDIOCGWIN\n");
  }
  win.x = 0;  //windows中的原点坐标
  win.y = 0;  //windows中的原点坐标
  win.width=w; //capture area 宽度
  win.height=h; //capture area 高度
  
  /*使用IOCTL命令VIDIOCSWIN，设置摄像头的基本信息*/
  if (ioctl(cam_fd, VIDIOCSWIN, &win) < 0) {
    printf("ERROR:VIDIOCSWIN\n");
  }
  
  /*设置摄像头voideo_picture信息*/
  config_vid_pic();  
  
   /*使用IOCTL命令VIDIOCGCAP，获取获得摄像头存储缓冲区的帧信息*/
  ret = ioctl(cam_fd,VIDIOCGMBUF,&cam_mbuf);
  if( ret<0 ) {
    printf("ERROR:VIDIOCGMBUF,Can't get video_mbuf\n");
  }
  printf("Frames:%d\n",cam_mbuf.frames);
  nframe = cam_mbuf.frames;
  
  /*接着把摄像头对应的设备文件映射到内存区*/
  cam_data = (char*)mmap(0, cam_mbuf.size, PROT_READ|PROT_WRITE,MAP_SHARED,cam_fd,0); 
  if( cam_data == MAP_FAILED ) {
    printf("ERROR:mmap\n");
  }
  printf("Buffer size:%d\nOffset:%d\n",cam_mbuf.size,cam_mbuf.offsets[0]);
  InitLookupTable();
  
}


void init_param(T264_param_t* param, const char* file)
{
	int total_no;
	FILE* fd; 
	char line[255];
	int32_t b;
	if (!(fd = fopen(file,"r")))
	{
		printf("Couldn't open parameter file %s.\n", file);
		exit(-1);
	}

	memset(param, 0, sizeof(*param));
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	if (b != 4)
	{
		printf("wrong param file version, expect v4.0\n");
		exit(-1);
	}
	fgets(line, 254, fd); sscanf(line,"%d", &param->width);
	fgets(line, 254, fd); sscanf(line,"%d", &param->height);
	fgets(line, 254, fd); sscanf(line,"%d", &param->search_x);
	fgets(line, 254, fd); sscanf(line,"%d", &param->search_y);
	fgets(line, 254, fd); sscanf(line,"%d", &total_no);
	fgets(line, 254, fd); sscanf(line,"%d", &param->iframe);
	fgets(line, 254, fd); sscanf(line,"%d", &param->idrframe);
	fgets(line, 254, fd); sscanf(line,"%d", &param->b_num);
	fgets(line, 254, fd); sscanf(line,"%d", &param->ref_num);
	fgets(line, 254, fd); sscanf(line,"%d", &param->enable_rc);
	fgets(line, 254, fd); sscanf(line,"%d", &param->bitrate);
	fgets(line, 254, fd); sscanf(line,"%f", &param->framerate);
	fgets(line, 254, fd); sscanf(line,"%d", &param->qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->min_qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->max_qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->enable_stat);
	fgets(line, 254, fd); sscanf(line,"%d", &param->disable_filter);
	fgets(line, 254, fd); sscanf(line,"%d", &param->aspect_ratio);
	fgets(line, 254, fd); sscanf(line,"%d", &param->video_format);
	fgets(line, 254, fd); sscanf(line,"%d", &param->luma_coeff_cost);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRA16x16) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRA4x4) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRAININTER) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_HALFPEL) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_QUARTPEL) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SUBBLOCK) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FULLSEARCH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_DIAMONDSEACH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FORCEBLOCKSIZE) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FASTINTERPOLATE) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SAD) * b;
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_EXTRASUBPELSEARCH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SCENEDETECT) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x16P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x16P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x4P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_4x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_4x4P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x16B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x8B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x16B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x8B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &param->cpu);
	fgets(line, 254, fd); sscanf(line, "%d", &param->cabac);

	// 	fgets(line, 254, fd); sscanf(line,"%s", src_path);
	// 	fgets(line, 254, fd); sscanf(line,"%s", out_path);
	// 	fgets(line, 254, fd); sscanf(line,"%s", rec_path);
	// 	param->rec_name = rec_path;

	fclose(fd);
}


void init_encoder()
{
	//编码准备
	const char* paramfile = "fastspeed.txt";
	/*获取fastspeed.txt文件信息，打开编码库*/
	init_param(&m_param, paramfile);
	m_param.direct_flag = 1;
	/*t264编码的打开*/
	m_t264 = T264_open(&m_param);
	m_lDstSize  = m_param.height * m_param.width + (m_param.height * m_param.width >> 1);
  	/*分配t264解码后数据存放的内存*/
	m_pDst = (uint8_t*)T264_malloc(m_lDstSize, CACHE_SIZE);
	
	/*分配内存用于存放一帧数据长度的数据*/
	m_pPoolData = malloc(m_param.width*m_param.height*3/2); 
}
	
void udps_respon(int sockfd,int w,int h) 
{ 
	struct sockaddr_in addrsrc;
	struct sockaddr_in addrdst; 
	int addrlen,n; 
	
	int32_t iActualLen;
	int row_stride = w*3*h/2;

	bzero(&addrdst,sizeof(struct sockaddr_in)); 
	addrdst.sin_family=AF_INET; 
	/*客户端PC机IP地址*/
	addrdst.sin_addr.s_addr=inet_addr("172.18.23.26");
	addrdst.sin_port=htons(SERVER_PORT);

	while(1) 
	{ 
		/*数据的采集*/
		read_video(NULL,w,h);
		/*对采集到的数据通过H264编码*/
		iActualLen = T264_encode(m_t264, cam_data, m_pDst, row_stride);
		printf("encoded:%d, %d bytes.\n",row_stride,iActualLen); 
		/*frame_num：存放帧号*/
		memcpy(m_pPoolData,&m_t264->frame_num,1);
		/*m_pDst解码后的数据*/
		memcpy(m_pPoolData+1, m_pDst, iActualLen);
		iActualLen++;
		/*使用UDP协议发送编码后的数据到客服端*/
		sendto(sockfd,m_pPoolData,iActualLen,0,(struct sockaddr*)&addrdst,sizeof(struct sockaddr_in)); 	
	} 
} 


void free_dev()
{
  printf("free device\n");
  close(cam_fd);
}

/*主函数入口*/
int main(void) 
{ 	
	int sockfd; 
	struct sockaddr_in addr;

	printf("start 2.0...\n"); 

/*创建套接字,是UDP*/
	sockfd=socket(AF_INET,SOCK_DGRAM,0); 

	if(sockfd<0) 
	{
		printf("0-");
		printf("Socket Error\n"); 
		exit(1); 
	} 

	bzero(&addr,sizeof(struct sockaddr_in)); 
	addr.sin_family=AF_INET; 
	addr.sin_addr.s_addr=htonl(INADDR_ANY); 
	addr.sin_port=htons(SERVER_PORT); 
	/*套接字绑定*/
	if(bind(sockfd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))<0 ) 
	{ 
		printf(stderr,"Bind Error:%s\n",strerror(errno)); 
		exit(1); 
	} 

	/*该函数完成编码前的准备*/
	init_encoder();

	atexit( &free_dev );
	/*采集数据前的初始化函数*/
	init_video(m_param.width,m_param.height);

	/*使用UDP协议发送采集到的数据*/
	udps_respon(sockfd,m_param.width,m_param.height); 
	
	close(sockfd); 

} 
