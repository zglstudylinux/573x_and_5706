#include "include.h"
#include "func.h"
#include "usb_device/usb_enum.h"

AT(.text.func.adapter.msg)
void func_adapter_message(u16 msg)
{
    switch (msg) {
        case MSG_MUSIC_PLAY_PAUSE:
#if WARNING_POWER_ON
    bsp_res_play(RES_IDX_POWERON);

    //等待开机提示音播报完，并完成设备插入检测
    do {
        func_process();
    } while(bsp_res_is_playing());
#endif // WARNING_POWER_ON
            break;

    case MSG_SYS_1S:
#if WIRELESS_CON_PWR_CTR
        ws_pwr_ctr_tx_cmd_process();
#endif
        break;

#if ADAPTER_USB_MIC_RX_EN | ADAPTER_USB_SPK_EN
    case EVT_PC_INSERT:
        printf("EVT_PC_INSERT\n");
        ude_info_init();
        usb_device_enter(UDE_ENUM_TYPE);
        sys_cb.ude_flag = true;
        break;

    case EVT_PC_REMOVE:
        pc_remove();
        usb_device_exit();
        sys_cb.ude_flag = false;
        printf("EVT_PC_REMOVE\n");
        break;

    case EVT_UDE_SET_VOL:
        ude_set_spk_vol_cb();
        break;

    case EVT_UDE_SET_VOL_L_R:
        ude_set_spk_volume_db_l_r();
        break;

    case EVT_UDE_SET_MUTE:
        printf("UDE_SET_MUTE\n");
        break;
#endif

    default:
        func_message(msg);
        break;
    }
}
