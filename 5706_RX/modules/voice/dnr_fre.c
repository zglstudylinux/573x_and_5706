#include "include.h"
#include "api_alg.h"
#include "dnr_fre.h"

/*
 * 文件名称: dnr_fre.c
 * 功能描述: 本文件为软件dnn_L1处理模块
    AT(.buf.dnr_fre);
    AT(.rodata.dnr_fre)
    AT(.text.dnr_fre_proc)
    AT(.text.dnr_fre_init)

    注意mic_pcm_t 实际配置类型

 ****************************************************************************************
    code + rodata : 8.4k
    buf           : 6.7k
    time          : 500us /2.5ms   240M
 */

#if DNR_FRE_EN
typedef s16                     mic_pcm_t;

#define UARTDUMP_DNR_FRE_EN          1                           //DUMP数据一键配置，需要使用定制上位机
#define DNR_FRE_INFO_PRINT           0
#define FRAME_LEN                    120                         //算法处理帧长
#define PROCESS_OUT_SAMPLES          120                         //每次存取帧长

static struct tog_bug_tag dnr_fre_tbuf AT(.buf.dnr_fre);                    //乒乓buf控制
static mic_pcm_t dnr_fre_cache_buf[FRAME_LEN*2] AT(.buf.dnr_fre);           //乒乓buf缓存
static mic_pcm_t dnr_fre_tmp_buf[PROCESS_OUT_SAMPLES] AT(.buf.dnr_fre);     //输出buf中转缓存
static void *dnr_fre_proc_ptr = dnr_fre_cache_buf;

static dnr_fre_cb_t dnr_fre_cb AT(.buf.dnr_fre);
static dnr_fre_mic_cfg_t dnr_fre_mic_cfg AT(.buf.dnr_fre);

#if UARTDUMP_DNR_FRE_EN
static u8 dump_head_buf[14*2] AT(.buf.dnr_fre);
static u8 UartDump_buf[2][14 + FRAME_LEN*2] AT(.buf.dnr_fre);
static u8 UartDump_buf_temp[14 + FRAME_LEN*2] AT(.buf.dnr_fre);
static u8 huart_buffer_rx[128] AT(.buf.dnr_fre);
#endif

#if DNR_FRE_INFO_PRINT
AT(.com_text.dnr_fre)
const char dnr_fre_info[] = "DNR_FRE_EN samples = %d, isrcnt = %d (SR_%d) %d\n";

AT(.text.dnr_fre_proc)
static void info_printf(void)
{
    static u32 ticks = 0;
    static u32 isr_cnt = 0;
    isr_cnt++;
    if (tick_check_expire(ticks,1000)) {
        my_printf(dnr_fre_info, FRAME_LEN, isr_cnt, FRAME_LEN*isr_cnt, sizeof(mic_pcm_t));
        isr_cnt = 0;
        ticks = tick_get();
    }
}
#endif

AT(.text.dnr_fre_proc)
void dnr_fre_mic_audio_input(u8 *ptr, u32 samples, int ch_mode, void *params)
{
    if (!dnr_fre_mic_cfg.mute && wireless_cb.alg_en) {

        while(samples > 0) {
            uint rlen = (samples > PROCESS_OUT_SAMPLES)? PROCESS_OUT_SAMPLES : samples;
            if(tog_buf_get(&dnr_fre_tbuf, (u8 *)dnr_fre_tmp_buf, rlen*sizeof(mic_pcm_t))) {
                tog_buf_rd_toggle(&dnr_fre_tbuf);
            }

            // 填充block, true表示算法攒帧完毕, kick低优先级线程处理算法
            if(tog_buf_put(&dnr_fre_tbuf, ptr, rlen*sizeof(mic_pcm_t))) {
                dnr_fre_proc_ptr = tog_bug_get_w_block(&dnr_fre_tbuf);
                tog_buf_wr_toggle(&dnr_fre_tbuf);
                dnr_fre_mic_cfg.kick_proc_done++;
                dnr_fre_mic_proc_kick_start();
            }

            memcpy(ptr, dnr_fre_tmp_buf, rlen*sizeof(mic_pcm_t));
            if(dnr_fre_mic_cfg.callback) {
                dnr_fre_mic_cfg.callback((void *)ptr, rlen, ch_mode, params);
            }

            samples -= rlen;
        }
    } else {
        if(dnr_fre_mic_cfg.callback) {
            dnr_fre_mic_cfg.callback((void *)ptr, samples, ch_mode, params);
        }
    }

}

//dnr_fre算法启动计算 放在低优先级现场处理
AT(.text.dnr_fre_proc)
void dnr_fre_mic_proc_cb(void)
{
    mic_pcm_t *rptr = dnr_fre_proc_ptr;
#if DNR_FRE_INFO_PRINT
    info_printf();
#endif
    s32 p_left;
    for (u16 i = 0; i < FRAME_LEN; ++i) {
        p_left = rptr[i];
        p_left = __builtin_muls_shift15(p_left, SOFT_GAIN_P16_0DB);
        rptr[i] = s_clip16(p_left);
    }
#if UARTDUMP_DNR_FRE_EN
    s16 *ptr_temp = (s16 *) UartDump_buf_temp;
    dump_dma_wait();
    dump_put2ram(&UartDump_buf[0][0],rptr,FRAME_LEN*2,0); //算法前数据组包

    dump_put2ram(&UartDump_buf[1][0],ptr_temp,FRAME_LEN*2,1);//算法后数据组包
    dump_dma_kick(&UartDump_buf[0][0], (FRAME_LEN*2 + 14) * 2); //最后再一起吐数据，避免前后kick数据打架
#endif

    dnr_fre_process(rptr, NULL, 0);

#if UARTDUMP_DNR_FRE_EN
    for(int i = 0; i < FRAME_LEN; i++) {
        ptr_temp[i]  = rptr[i];
    }
#endif

    for (u16 i = 0; i < FRAME_LEN; ++i) {
        p_left = rptr[i];
        p_left = __builtin_muls_shift15(p_left, SOFT_GAIN_N16DB);
        rptr[i] = s_clip16(p_left);
    }

    dnr_fre_mic_cfg.kick_proc_done--;
}

AT(.text.dnr_fre_set)
void dnr_fre_mic_output_callback_set(audio_callback_t callback)
{
    dnr_fre_mic_cfg.callback = callback;
}

AT(.text.dnr_fre_init)
void dnr_fre_mic_init(u8 sample_rate, u16 samples, u8 channel)
{
    memset((uint8_t *)&dnr_fre_mic_cfg, 0, sizeof(dnr_fre_mic_cfg));
    tog_buf_init(&dnr_fre_tbuf, dnr_fre_cache_buf, FRAME_LEN*sizeof(mic_pcm_t));

    dnr_fre_mic_param_set(0);

//    ///算法初始化处先mute，等连上无线麦再打开/接收端发送使能再打开
//    dnr_fre_mic_mute_set(1);

#if UARTDUMP_DNR_FRE_EN
    memset(dump_head_buf, 0, 14*2);
    memset(UartDump_buf, 0, 2*(14 + FRAME_LEN*2));

    huart_t huart0;
    memset(&huart0, 0x00, sizeof(huart0));
    huart0.rx_port = HUART_TR_PB3;        //HUART串口选择: PA15: 0, PB2: 1, PB3: 2, PB4: 3, PB15: 4, PE10: 5, PE13: 6, VUSB: 7
    huart0.tx_port = HUART_TR_PB3;
    huart0.rxbuf_loop = 1;
    huart0.rxbuf      = huart_buffer_rx;
    huart0.rxbuf_size = 128;
    huart0.txisr_en = 1;
    huart0.rxisr_en = 0;
    huart_init(&huart0, 8000000);        //波特率设置默认是1.5MHz

    dump_buf_init(dump_head_buf,2, huart_tx, huart_wait_txdone);
#endif
}

AT(.text.dnr_fre_exit)
void dnr_fre_mic_exit(void)
{

}

AT(.text.dnr_fre_set.param)
void dnr_fre_mic_param_set(s16 dnr_fre_nt)
{
    memset((uint8_t *)&dnr_fre_cb, 0, sizeof(dnr_fre_cb));
	dnr_fre_cb.overdrive			    = 32768;
	dnr_fre_cb.smooth_en			    = 1;
	dnr_fre_cb.enr_thres				= 0;
	dnr_fre_cb.prior_opt_idx			= 10;
	dnr_fre_cb.low_fre_range			= 0;
	dnr_fre_cb.smooth_v				    = 31129; // 0.9f
	dnr_fre_cb.music_lev				= 16;


	dnr_fre_cb.denoiseBound		        = 16384;  //降噪量：20*log10(denoiseBound/32768) . 16384就相当于噪声降6db
	dnr_fre_cb.enr_nr_thr				= -35;//dB  门限：低于此门限为噪声
	dnr_fre_cb.in_attack				= 32768;//Q15 ms 检测的att时间，peak， 32768=1ms
	dnr_fre_cb.in_release				= 3276800;//Q15 ms 检测的release时间，peak， 32768=1ms
	dnr_fre_cb.fs						= 16000;
	dnr_fre_cb.noise_init				= 6;
    dnr_fre_init(&dnr_fre_cb);
}

AT(.text.dnr_fre_set.mute)
void dnr_fre_mic_mute_set(uint8_t mute)
{
    dnr_fre_mic_cfg.mute = mute;
    if (mute) {
        while(dnr_fre_mic_cfg.kick_proc_done){
            printf("#");
        }
        tog_buf_init(&dnr_fre_tbuf, dnr_fre_cache_buf, FRAME_LEN*sizeof(mic_pcm_t));
    }
}

AT(.text.dnr_fre_get.mute)
uint8_t dnr_fre_mic_mute_get(void)
{
    return dnr_fre_mic_cfg.mute;
}
#else
void dnr_fre_mic_proc_cb(void){}
#endif
