#include "include.h"
#include "api_alg.h"
#include "ains4.h"
/*
 * 文件名称: ains3_api.c
 * 功能描述: 本文件为软件AINS3处理模块
    AT(.buf.ains4);
    AT(.rodata.ains4)
    AT(.text.ains4_proc)
    AT(.text.ains4_init)

 ****************************************************************************************
    code + rodata :
    buf           :
    time :  240M 480samples 3.3ms/10ms
 */

#if AINS4_EN

#define UARTDUMP_AINS4_EN       0                         //DUMP数据一键配置，需要使用定制上位机
#define AINS4_INFO_PRINT        0
#define AINS4_RDFT_HW           0                         //是否使用硬件FFT处理,软件fft底层处理若是关闭则无法编译通过
#define FRAME_LEN		        480                       //算法处理帧长
#define PROCESS_OUT_SAMPLES     120                       //每次存取帧长

static struct tog_bug_tag ains4_tbuf AT(.buf.ains4);          //乒乓buf控制
s16 ains4_cache_buf[FRAME_LEN*2]  AT(.buf.ains4); //乒乓buf缓存
u8 ains4_out_buf[PROCESS_OUT_SAMPLES*2]  AT(.buf.ains4);   //输出buf中转缓存
static void *ains4_proc_ptr = ains4_cache_buf;


static ains4_cb_t ains4_cb AT(.buf.ains4);
///全局结构体放bss段，避免被覆盖修改
static ains4_mic_cfg_t ains4_mic_cfg;

#if AINS4_RDFT_HW
typedef struct{
    fft_cfg_t fft_cft;
    ifft_cfg_t ifft_cft;
    s32 ains4_fft_in[512];
    s32 ains4_fft_out[512];
    s32 ains4_ifft_out[512];
}ains4_rdft_t;
ains4_rdft_t ains4_rdft AT(.buf.ains4);
#endif

//AINS4 48k 降噪参数定义
int8_t ains4_nt                  = 0;//AINS4_PAR_NT; // -20 ~ 40
uint8_t ains4_prior_opt_idx      = 0;//AINS4_PAR_OPT;// 0 ~ 19

#if AINS4_INFO_PRINT
AT(.com_text.ains4)
const char ains4_info[] = "AINS4 samples = %d, isrcnt = %d (SR_%d)\n";
#endif

#if UARTDUMP_AINS4_EN
static u8 dump_head_buf[14*2]AT(.buf.ains4);
static u8 UartDump_buf[2][14 + FRAME_LEN*2]AT(.buf.ains4);
static u8 UartDump_buf_temp[14 + FRAME_LEN*2]AT(.buf.ains4);
#endif

AT(.text.ains4_proc)WEAK
void ains4_mic_audio_input(u8 *ptr, u32 samples, int ch_mode, void *params)
{
    if (!ains4_mic_cfg.mute && wireless_get_status()) {
        while(samples > 0) {
            uint rlen = (samples > PROCESS_OUT_SAMPLES)? PROCESS_OUT_SAMPLES : samples;
            if(tog_buf_get(&ains4_tbuf, ains4_out_buf, rlen*2)) {
                tog_buf_rd_toggle(&ains4_tbuf);
            }

            if(tog_buf_put(&ains4_tbuf, ptr, rlen*2)) {
                ains4_proc_ptr = tog_bug_get_w_block(&ains4_tbuf);
                tog_buf_wr_toggle(&ains4_tbuf);

                //触发低优先级线程处理ains4
                ains4_mic_proc_kick_start();
            }

            memcpy(ptr, ains4_out_buf, rlen*2);
            if (ains4_mic_cfg.callback) {
                ains4_mic_cfg.callback((void *)ptr, rlen, ch_mode, params);
            }

            samples -= rlen;
        }
    } else {
        if (ains4_mic_cfg.callback) {
            ains4_mic_cfg.callback((void *)ptr, samples, ch_mode, params);
        }
    }

}
/*
AT(.rodata.ains4)
static u8 test_data[640]={
	0x23, 0x00, 0x3c, 0xa8, 0xdb, 0x51, 0xf9, 0x95, 0xc8, 0x22, 0xe0, 0xbc, 0x65, 0xc0, 0x51, 0x0a, 0x9e, 0x81,
	0x11, 0x07, 0xfd, 0x01, 0x63, 0xdd, 0x89, 0xcd, 0x74,
	0x2a, 0x00, 0x81, 0x87, 0xa4, 0x6f, 0x0c, 0xfa, 0x20, 0xeb, 0x90, 0x24, 0xb7, 0x1d, 0xd8, 0x00, 0x00, 0x2d,
	0x67, 0x2b, 0x17, 0x78, 0x6d, 0xb7, 0x91, 0xc0, 0xdc,
	0xa1, 0x0d, 0x89, 0xd2, 0x2c, 0xcd, 0x89, 0xdd, 0x7c,
	0xc1, 0x00, 0xc9, 0x1b, 0xc3, 0xf4, 0xe2, 0xc0, 0xc7, 0x75, 0x62, 0xab, 0x18, 0xe1, 0xc3, 0xb3, 0x00, 0x01,
	0xa1, 0xc7, 0xc6, 0x0e, 0xb4, 0x4d, 0x91, 0xd5, 0x74,
	0xd3, 0x00, 0xb1, 0x2c, 0x0d, 0xd8, 0xaf, 0xaa, 0xd4, 0x2a, 0xb2, 0x5b, 0xfe, 0x15, 0x74, 0x98, 0xc9, 0x00,
	0xd1, 0xa6, 0xd2, 0xa6, 0x7a, 0x4d, 0x89, 0xc9, 0x84,
	0x23, 0x00, 0x3c, 0xa8, 0xdb, 0x51, 0xf9, 0x95, 0xc8, 0x22, 0xe0, 0xbc, 0x65, 0xc0, 0x51, 0x0a, 0x9e, 0x81,
	0x11, 0x07, 0xfd, 0x01, 0x63, 0xdd, 0x89, 0xcd, 0x74,
	0x2a, 0x00, 0x81, 0x87, 0xa4, 0x6f, 0x0c, 0xfa, 0x20, 0xeb, 0x90, 0x24, 0xb7, 0x1d, 0xd8, 0x00, 0x00, 0x2d,
	0x67, 0x2b, 0x17, 0x78, 0x6d, 0xb7, 0x91, 0xc0, 0xdc,
	0xa1, 0x0d, 0x89, 0xd2, 0x2c, 0xcd, 0x89, 0xdd, 0x7c,
	0xc1, 0x00, 0xc9, 0x1b, 0xc3, 0xf4, 0xe2, 0xc0, 0xc7, 0x75, 0x62, 0xab, 0x18, 0xe1, 0xc3, 0xb3, 0x00, 0x01,
	0xa1, 0xc7, 0xc6, 0x0e, 0xb4, 0x4d, 0x91, 0xd5, 0x74,
	0xd3, 0x00, 0xb1, 0x2c, 0x0d, 0xd8, 0xaf, 0xaa, 0xd4, 0x2a, 0xb2, 0x5b, 0xfe, 0x15, 0x74, 0x98, 0xc9, 0x00,
	0xd1, 0xa6, 0xd2, 0xa6, 0x7a, 0x4d, 0x89, 0xc9, 0x84,
	0x80, 0x00, 0xb1, 0x84, 0x25, 0x8f, 0xd7, 0xbe, 0x16, 0x0e, 0x97, 0x20, 0x55, 0x2c, 0x8b, 0xcd, 0x34, 0x80,
	0x55, 0xe8, 0x3b, 0x0c, 0x8f, 0x5d, 0x21, 0xd1, 0x8c,
	0x66, 0x00, 0x85, 0x6d, 0x32, 0x03, 0xed, 0x99, 0xd7, 0x30, 0x1b, 0x49, 0x89, 0x08, 0xf2, 0xb3, 0x8a, 0x0f,
	0x80, 0xb5, 0xfb, 0x3d, 0xf6, 0x4d,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb, 0x8c, 0x39, 0xf0, 0x3d, 0x91, 0xb9, 0x8c,
	0x11, 0x00, 0xba, 0x9d, 0xc5, 0xdb, 0x11, 0xb1, 0x87, 0x57, 0x17, 0xec, 0x44, 0x63, 0xe8, 0xd0, 0x80, 0x03,
	0x83, 0x75, 0x82, 0x87, 0x02, 0xcd, 0x89, 0xdd, 0x78,
	0xb9, 0x00, 0x7b, 0xee, 0xe3, 0x70, 0x1b, 0xdb, 0x1e, 0x5f, 0x34, 0x39, 0x74, 0x25, 0xf6, 0x2a, 0x46, 0x9e,

	0x80, 0x00, 0xb1, 0x84, 0x25, 0x8f, 0xd7, 0xbe, 0x16, 0x0e, 0x97, 0x20, 0x55, 0x2c, 0x8b, 0xcd, 0x34, 0x80,
	0x55, 0xe8, 0x3b, 0x0c, 0x8f, 0x5d, 0x21, 0xd1, 0x8c,
	0x66, 0x00, 0x85, 0x6d, 0x32, 0x03, 0xed, 0x99, 0xd7, 0x30, 0x1b, 0x49, 0x89, 0x08, 0xf2, 0xb3, 0x8a, 0x0f,
	0x80, 0xb5, 0xfb, 0x3d, 0xf6, 0x4d,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb, 0x8c, 0x39, 0xf0, 0x3d, 0x91, 0xb9, 0x8c,
	0x11, 0x00, 0xba, 0x9d, 0xc5, 0xdb, 0x11, 0xb1, 0x87, 0x57, 0x17, 0xec, 0x44, 0x63, 0xe8, 0xd0, 0x80, 0x03,
	0x83, 0x75, 0x82, 0x87, 0x02, 0xcd, 0x89, 0xdd, 0x78,
	0xb9, 0x00, 0x7b, 0xee, 0xe3, 0x70, 0x1b, 0xdb, 0x1e, 0x5f, 0x34, 0x39, 0x74, 0x25, 0xf6, 0x2a, 0x46, 0x9e,
	0x80, 0x00, 0xb1, 0x84, 0x25, 0x8f, 0xd7, 0xbe, 0x16, 0x0e, 0x97, 0x20, 0x55, 0x2c, 0x8b, 0xcd, 0x34, 0x80,
	0x55, 0xe8,
	0x80, 0x00, 0xb1, 0x84, 0x25, 0x8f, 0xd7, 0xbe, 0x16, 0x0e, 0x97, 0x20, 0x55, 0x2c, 0x8b, 0xcd, 0x34, 0x80,
	0x55, 0xe8,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb,
	0x30, 0x00, 0x68, 0x2b, 0x9a, 0x66, 0x75, 0x4c, 0x1b, 0xb1, 0x99, 0x0d, 0xd7, 0xdf, 0x95, 0x38, 0x00, 0x01,
	0xc2, 0xcb,
	0x23, 0x00, 0x3c, 0xa8, 0xdb, 0x51, 0xf9, 0x95, 0xc8, 0x22, 0xe0, 0xbc, 0x65, 0xc0, 0x51, 0x0a, 0x9e, 0x81,
	0x11, 0x07,
	0x23, 0x00, 0x3c, 0xa8, 0xdb, 0x51, 0xf9, 0x95, 0xc8, 0x22, 0xe0, 0xbc, 0x65, 0xc0, 0x51, 0x0a, 0x9e, 0x81,
	0x11, 0x07

};

*/

//ains4算法启动计算 放在低优先级现场处理
AT(.text.ains4_proc)WEAK
void ains4_mic_proc_cb(void)
{
    s16 *ptr = ains4_proc_ptr;

//    memcpy(ptr,test_data,640);
#if UARTDUMP_AINS4_EN
    s16 *ptr_temp = (s16 *) UartDump_buf_temp;
    dump_dma_wait();
    dump_put2ram(&UartDump_buf[0][0],ptr,FRAME_LEN*2,0); //算法前数据组包

    dump_put2ram(&UartDump_buf[1][0],ptr_temp,FRAME_LEN*2,1);//算法后数据组包
    dump_dma_kick(&UartDump_buf[0][0], (FRAME_LEN*2 + 14) * 2); //最后再一起吐数据，避免前后kick数据打架
#endif

//#if !AINS4_RDFT_HW
//    GPIOBSET |= BIT(2);
    ains4_process(ptr, (s32 *)ptr, 0);
//    GPIOBCLR |= BIT(2);
//#endif

#if AINS4_RDFT_HW
    u32 i = 0;
    ains4_process_ext_window(ptr,ains4_rdft.ains4_fft_in);
    fft_hw(&ains4_rdft.fft_cft);
    nr_process_do((s32 *)ains4_rdft.ains4_fft_out);
    ifft_hw(&ains4_rdft.ifft_cft);
    ains4_process_ext_output(ptr);
    for(i = 0; i < FRAME_LEN; i++) {
        ptr[i] = ains4_rdft.ains4_ifft_out[i];
    }
#endif

#if UARTDUMP_AINS4_EN
    for(int i = 0; i < FRAME_LEN; i++) {
        ptr_temp[i]  = ptr[i];
    }
#endif // UARTDUMP_DNN_EN

}

AT(.text.ains4_set)WEAK
void ains4_mic_output_callback_set(audio_callback_t callback)
{
    ains4_mic_cfg.callback = callback;
}

AT(.text.ains4_init)WEAK
void ains4_mic_init(u8 sample_rate, u16 samples, u8 channel)
{


#if AINS4_RDFT_HW
    ains4_rdft.fft_cft.size            = RDFT_512;
	ains4_rdft.fft_cft.input_type      = 1;
    ains4_rdft.fft_cft.window_en       = 1;
    ains4_rdft.fft_cft.isr_en          = 0;
    ains4_rdft.fft_cft.in_addr         = ains4_rdft.ains4_fft_in;
    ains4_rdft.fft_cft.out_addr        = ains4_rdft.ains4_fft_out;

    ains4_rdft.ifft_cft.size           = RDFT_512;
    ains4_rdft.ifft_cft.output_type    = 1;
    ains4_rdft.ifft_cft.window_en      = 1;
    ains4_rdft.ifft_cft.isr_en         = 0;
    ains4_rdft.ifft_cft.overlap_en     = 1;
    ains4_rdft.ifft_cft.overlap_len    = 0;
    ains4_rdft.ifft_cft.in_addr        = ains4_rdft.ains4_fft_out;
    ains4_rdft.ifft_cft.out_addr       = ains4_rdft.ains4_ifft_out;

    memset(ains4_rdft.ains3_fft_in, 0, sizeof(ains4_rdft.ains4_fft_in));
    memset(ains4_rdft.ains3_fft_out, 0, sizeof(ains4_rdft.ains4_fft_out));
    memset(ains4_rdft.ains3_ifft_out, 0, sizeof(ains4_rdft.ains4_ifft_out));
#endif

#if UARTDUMP_AINS4_EN
    memset(dump_head_buf, 0, 14*2);
    memset(UartDump_buf, 0, 2*(14 + FRAME_LEN*2));

    huart_module_init();    //初始化高速串口模块
    dump_buf_init(dump_head_buf,2, huart_putbuf, huart_wait_tx_finish);
#endif


    tog_buf_init(&ains4_tbuf, ains4_cache_buf, FRAME_LEN*2);
    memset((uint8_t *)&ains4_mic_cfg, 0, sizeof(ains4_mic_cfg));
    ains4_mic_param_set(0 ,1500, 0);


}

AT(.text.ains4_exit)WEAK
void ains4_mic_exit(void)
{


}

//mode 0：enc模式; mode 1:弱降噪模式；
//若mode=1,ains4_noise_nt 为30000，nr_level为0；
//若mode=0,ains4_noise_nt 可设，再根据ains4_noise_nt来选择nr_level，即轻 中 深三档；
AT(.text.ains4_set.set_param)WEAK
void ains4_mic_param_set(u8 mode ,s16 ains4_noise_nt, u8 nr_level)
{
    memset(ains4_cache_buf, 0, FRAME_LEN*2*2);
    memset(ains4_out_buf, 0, PROCESS_OUT_SAMPLES*2);
    memset((u8 *)&ains4_cb, 0, sizeof(ains4_cb));

#if UARTDUMP_AINS4_EN
    memset(dump_head_buf, 0, 14*2);
    memset(UartDump_buf, 0, 2*(14 + FRAME_LEN*2));

    huart_module_init();    //初始化高速串口模块
    dump_buf_init(dump_head_buf,2, huart_putbuf, huart_wait_tx_finish);
#endif

	//ains4_cb->noise_db2			   = -15;
	ains4_cb.yuan_en				= 0;

	ains4_cb.snr_thr               = (32768*5)>>1;
	if (mode==1) {
		if (ains4_cb.yuan_en==1) {
			ains4_cb.denoiseBound		    = ains4_noise_nt;  //if yuan_en=1,21000, else 12000
		} else {
			ains4_cb.denoiseBound		    = ains4_noise_nt;
		}
        ains4_cb.music_lev				= 6;
        ains4_cb.overdrive_adapt_en    = 1;
       	ains4_cb.overdrive			    = 32768/4;
        ains4_cb.gain_ceil				= 30000;
       	ains4_cb.noise_min_en			= 1;
        ains4_cb.smooth_logLrt  		= 0;
	} else {                                             //enc
		if (ains4_cb.yuan_en==1) {
			ains4_cb.denoiseBound		    = ains4_noise_nt;  //
		} else {
			ains4_cb.denoiseBound		    = ains4_noise_nt;
		}
		ains4_cb.music_lev				= 11;
        ains4_cb.overdrive_adapt_en     = 0;
        if (nr_level==1) {
			ains4_cb.overdrive			    = 32768;
			ains4_cb.smooth_logLrt  		= 0;
			ains4_cb.speech_update			= 32440;
			ains4_cb.smooth_en			    = 0;
		} else if (nr_level==2) {
			ains4_cb.overdrive			    = 40960;
			ains4_cb.smooth_logLrt  		= 16384;
			ains4_cb.speech_update			= 32440;
			ains4_cb.smooth_en			    = 0;
		} else {
			ains4_cb.overdrive			    = 40960;
			ains4_cb.smooth_logLrt  		= 16384;
			ains4_cb.speech_update			= 30016;
			ains4_cb.smooth_en			    = 1;
		}
        ains4_cb.gain_ceil				= 32767;
        ains4_cb.noise_min_en			= 0;

        //ains4_cb.overdrive			    = 32768;
        //ains4_cb.smooth_logLrt  		= 0;
	}

	//ains4_cb.smooth_en			    = 0;
	ains4_cb.modelUpdatePars0	    = 1;//0:no use HIST 1:only update one time first 2：always update
	ains4_cb.gainHB_rd			    = 32767;//0-32767
	ains4_cb.delta_k_up		    = 0;
	//ains4_cb.denoiseBound_fix		= 40;//

	ains4_cb.enr_thres				= 0;
	ains4_cb.prior_opt_idx			= 3;
	ains4_cb.low_fre_range			= 256;

	ains4_cb.hi_gain_len			= 66;
	if (mode==1) {
		ains4_cb.lquantile_sm			= 26216;
		ains4_cb.factor				= 10*32768;
	} else {
		ains4_cb.lquantile_sm			= 24966;
		ains4_cb.factor				= 30*32768;
	}

	ains4_cb.quan_gap				= 3277;
	ains4_cb.quan_gap_low_len		= 6;
	ains4_cb.quan_gap_low			= 3277;
	//ains4_cb.speech_update			= 32440;
	ains4_cb.smooth_v				= 27853; // 0.85f
	ains4_cb.enr_mean_max_en		= 1;//1:mean   0:max
	ains4_cb.enr_nr_thr			= -60;//dB
	ains4_cb.spp_en				= 1;
	ains4_cb.ymin_floor			= 200;
	ains4_cb.ymin_idx   			= 6;

	ains4_cb.noise_min_floor		= 0;


    ains4_init(&ains4_cb);
}

AT(.text.ains4_set.mute)WEAK
void ains4_mic_mute_set(uint8_t mute)
{
    ains4_mic_cfg.mute = mute;
    if(mute) {
        tog_buf_init(&ains4_tbuf, ains4_cache_buf, FRAME_LEN*2);
    }
}

AT(.text.ains4_get.mute)WEAK
uint8_t ains4_mic_mute_get(void)
{
    return ains4_mic_cfg.mute;
}

#endif

