#include "include.h"
#include "func.h"
#include "func_device.h"

#define TRACE_EN                1

#if TRACE_EN
#define TRACE(...)              printf(__VA_ARGS__)
#define TRACE_R(...)            print_r(__VA_ARGS__)
#else
#define TRACE(...)
#define TRACE_R(...)
#endif // TRACE_EN

#if FUNC_DEVICE_EN
enum {
    DEVICE_STA_INIT_IDLE,
    DEVICE_STA_INIT_CONNECT,
    DEVICE_STA_START_ACTION,
    DEVICE_STA_DELAY,

    DEVICE_STA_IDLE,
    DEVICE_STA_SCAN,

    DEVICE_STA_BROADCAST_TX,
    DEVICE_STA_BROADCAST_IDLE,
};

static struct {
    uint8_t init_flag;
    uint8_t init_state;
    uint8_t con_state;
    uint32_t ticks;
    uint tick_delay;
} device_cb;

//无线MIC发射端处理流程
//MIC_ADC -> encoder_prio_trans_audio_input --> mic_enc_audio_input -> wireless_d2a_put_tx_frame -> memcpy(txbuf, enc_buf, size) -> DEVICE_TX
//                                          |-> dac0_out_audio_input -> DAC
static wl_link_list_t mic_tx_cfg_tbl[] = {
/*  模块类型，                  使能，          初始化，                  输入接口，                       设置输出*/
    {MIC_TYPE,                  1,              mic_init,                 NULL,                            mic_audio_output_callback_set},
    {ENCODER_PRIO_TRANS_TYPE,   1,              encoder_prio_trans_init,  encoder_prio_trans_audio_input,  encoder_prio_trans_audio_output_callback_set},    //缓存到编码缓冲区，并转到低优先级处理
#if WIRELESS_MIC_DNR_FRE_EN
    {DNR_FRE_TYPE,              1,              dnr_fre_mic_init,         dnr_fre_mic_audio_input,         dnr_fre_mic_output_callback_set},
#endif
#if WIRELESS_MIC_DNR_EN
    {MIC_DNR_TYPE,              1,              mic_dnr_audio_init,         mic_dnr_audio_input,        mic_dnr_audio_output_callback_set},
#endif
#if WIRELESS_MIC_EQ_DRC_EN
    {MIC_EQ_DRC,                   1,           mic_eq_drc_init,          mic_eq_drc_audio_input,          mic_eq_drc_audio_output_callback_set},
#endif // WIRELESS_MIC_EQ_DRC_EN

#if WIRELESS_MIC_SOFT_VOL_EN
    {MIC_DRC,                   1,              drc_mic_init,             drc_mic_audio_input,             drc_mic_audio_output_callback_set},
#endif
#if WIRELESS_MIC_AINS4_EN
    {AINS_TYPE,                 1,              ains4_mic_init,           ains4_mic_audio_input,           ains4_mic_output_callback_set},
#endif
#if WIRELESS_MIC_DNN_L2_EN
    {DNN_L2_TYPE,               1,              dnn_L2_mic_init,          dnn_L2_mic_audio_input,          dnn_L2_mic_output_callback_set},
#endif
#if WIRELESS_MIC_DNN_L3_EN
    {DNN_L3_TYPE,               1,              dnn_L3_mic_init,          dnn_L3_mic_audio_input,          dnn_L3_mic_output_callback_set},
#endif
#if WIRELESS_MIC_ECHO_EN
    {ECHO_TYPE,                 1,              echo_audio_init,          echo_audio_input,                echo_audio_output_callback_set},
#endif
#if WIRELESS_MIC_MAGIC_EN
    {MAGCI_PITCH_SHIFT_TYPE,    1,              magic_audio_init,         magic_audio_input,               magic_audio_output_callback_set},
#endif
    {MIC_ENC_TYPE,              1,              mic_enc_init,             mic_enc_audio_input,             mic_enc_audio_output_callback_set},
};


AT(.text.func.device_cb)
void func_device_init(void)
{
    if (!device_cb.init_flag) {
        device_cb.init_flag = 1;
        lock_code_wl_com();
        wl_link_list_init(mic_tx_cfg_tbl, (sizeof(mic_tx_cfg_tbl)/sizeof(wl_link_list_t)), WIRELESS_MIC_SAMPLE_RATE_SELECT, WIRELESS_MIC_SAMPLES_SELECT, WIRELESS_MIC_CHANNEL_SELECT);
    }

    device_cb.init_state = DEVICE_STA_INIT_IDLE;
}
#if WIRELESS_MIC_BROADCAST_EN
AT(.text.func.device)
static void func_device_process_do(void)
{
    switch(device_cb.init_state) {
    case DEVICE_STA_INIT_IDLE:
        ble_per_adv_set_enable(1);
        device_cb.init_state = DEVICE_STA_BROADCAST_TX;
        break;
    case DEVICE_STA_BROADCAST_TX:
        device_cb.init_state = DEVICE_STA_BROADCAST_IDLE;
        break;

    default:
        break;
    }
}
#else
AT(.text.func.device)
static void func_device_process_do(void)
{
    u8 addr[6];

    if(wireless_cb.change_flag) {
        wireless_cb.change_flag = 0;
#if WIRELESS_CON_BONDING_EN
        wireless_bond_nb_updatae();
#endif
        switch(wireless_cb.change_sta[0]) {
        case 0:     //connect success
            device_cb.init_state = DEVICE_STA_START_ACTION;
            break;

        case 1:     //connect fail
            if(!wireless_mic_is_bonding()) {
                device_cb.ticks      = tick_get();
                device_cb.tick_delay = 100+bt_get_rand()%500;
                device_cb.init_state = DEVICE_STA_DELAY;
                break;
            }
            //no break

        case 2:     //disconnect
            device_cb.ticks      = tick_get();
            device_cb.tick_delay = 500+bt_get_rand()%500;
            device_cb.init_state = DEVICE_STA_DELAY;
            break;
        }
    }

    switch(device_cb.init_state) {
    case DEVICE_STA_INIT_IDLE:
        TRACE("emit, init(%d)\n", wireless_mic_is_bonding());
        if(wireless_get_link_info_addr(addr)) {
            //有回连信息，开始回连
            TRACE("emit, con_req: ");
            TRACE_R(addr, 6);
            ble_create_con_for_addr(addr, 2500);
            device_cb.init_state  = DEVICE_STA_INIT_CONNECT;
        } else {
            //没有回连信息，搜索组队
            TRACE("emit, scan_en\n");
            ble_scan_set_enable(1);
            device_cb.init_state  = DEVICE_STA_SCAN;
            device_cb.ticks       = tick_get();
        }
        break;

    case DEVICE_STA_INIT_CONNECT:
        break;

    case DEVICE_STA_START_ACTION:
        TRACE("emit, con_sta(%d): %x\n", wireless_mic_is_bonding(), wireless_cb.connected_sta);

        if(wireless_cb.connected_sta) {
            //已连接，关闭扫描
            TRACE("emit, con_complete\n");
            device_cb.init_state = DEVICE_STA_IDLE;
        } else if(wireless_mic_is_bonding() && wireless_get_link_info_addr(addr)) {
            //有回连信息，开始回连
            TRACE("emit, con_req: ");
            TRACE_R(addr, 6);
            ble_create_con_for_addr(addr, 2500);
            device_cb.init_state  = DEVICE_STA_INIT_CONNECT;
        } else {
            //没有回连信息，搜索组队
            TRACE("emit, scan_en\n");
            ble_scan_set_enable(1);
            device_cb.init_state = DEVICE_STA_SCAN;
            device_cb.ticks      = tick_get();
        }
        break;

    case DEVICE_STA_SCAN:
        if(tick_check_expire(device_cb.ticks, 4000)) {     //省电时可以间歇性扫描
            ble_scan_set_enable(0);
            device_cb.ticks      = tick_get();
            device_cb.tick_delay = 1000+bt_get_rand()%500;
            device_cb.init_state = DEVICE_STA_DELAY;
        }
        break;

    case DEVICE_STA_DELAY:
        if(tick_check_expire(device_cb.ticks, device_cb.tick_delay)) {
            device_cb.init_state = DEVICE_STA_START_ACTION;
        }
        break;
    }
}
#endif
AT(.text.func.process.device)
static void func_device_process(void)
{
#if TRACE_EN
    static u8 sta = 0xff;
    if(sta != device_cb.init_state) {
        sta = device_cb.init_state;
        TRACE("emit, state: %d\n", sta);
    }
#endif
#if WIRELESS_MIC_BROADCAST_EN
    if(device_cb.init_state != DEVICE_STA_BROADCAST_IDLE) {
#else
    if(device_cb.init_state != DEVICE_STA_IDLE || wireless_cb.change_flag) {
#endif
        func_device_process_do();
    }

//    led_disp_proc();
    wireless_sta_proc();
    func_process();
}

AT(.text.func.device)
static void func_device_enter(void)
{
#if WIRELESS_CON_BONDING_EN
    wireless_bond_init();
#endif
    func_device_init();
    func_bt_init();
#if USER_CF_UI_CONFIG
    if(sys_cb.power_on_sta) {
        func_cb.sta = FUNC_PWROFF;
    }
#endif
    /*
    sys_clk_req(INDEX_KARAOK, SYS_160M);     //本地麦测试   //先抬高主频
    wireless_device_init();
    wireless_mic_kick();
    while(1){
        WDT_CLR();
    }
    */
}

AT(.text.func.device)
static void func_device_exit(void)
{
//    le_mic_discon_all();
    mic_stop();
    bt_off();
    func_cb.last = FUNC_DEVICE;
//    sys_cb.bt_is_inited = 0;
    device_cb.init_flag = 0;
}

AT(.text.func.device_cb)
void func_device(void)
{
    printf("%s\n", __func__);
    func_device_enter();

    while (func_cb.sta == FUNC_DEVICE) {
        func_device_process();
        func_device_message(msg_dequeue());
        //func_device_display();
    }

    func_device_exit();
}

#endif // FUNC_DEVICE_EN
