#include "include.h"
#include "func.h"
AT(.text.func.wireless_mic)
void func_adapter_message(u16 msg)
{
    switch (msg) {
#if WIRELESS_CON_AND_BT_PAIRING_EN 
        case MSG_PAIRING:
            if(wireless_cb.connected_sta & BIT(0)) {
                wireless_disconnect_req(0);
            }
            if(wireless_cb.connected_sta & BIT(1)) {
                wireless_disconnect_req(1);
            }
            printf("MSG_PAIRING\n");
            wireless_bonding_clr();
            wireless_adapter_pairing_enable(1);
            break;
#endif
        case MSG_SYS_1S:
#if WIRELESS_CON_PWR_CTR
            ws_pwr_ctr_tx_cmd_process();
#endif
            break;

    default:
        func_message(msg);
        break;
    }
}
