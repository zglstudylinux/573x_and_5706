#include "include.h"

AT(.com_rodata.usb_sd.cfg)
bool cfg_usb_sd_muxio_en = SD_USB_MUX_IO_EN;

//COM_CHAR str_sd_insert[] = "sd insert\n";
//COM_CHAR str_sd_remove[] = "sd remove\n";

#if SD_USB_MUX_IO_EN
volatile static u8 usb_chk_sta = 0;
void usb_get_chk_sta(void)
{
	while (usb_chk_sta) {
		WDT_CLR();
	}
}
#endif

//AT(.com_text.const)
//const char usb_detect_str[] = "USB STA:%d\r\n";
//AT(.com_text.const)
//const char usb_insert_str[] = "udisk insert\n";
//AT(.com_text.const)
//const char usb_remove_str[] = "udisk remove\n";


#if USB_SUPPORT_EN
AT(.com_text.detect)
void usb_detect(void)
{
    if (!is_usb_support()) {
        return;
    }

    if(!adapter_usb_init_en()){
        return;
    }

    u8 usb_sta;
#if USB_DET_VER_SEL
    usb_sta = usb_connect();
#else
    usb_sta = usbchk_connect(USBCHK_ONLY_DEVICE);
#endif

    if (usb_sta == USB_PC_CONNECTED) {
        if (dev_online_filter(DEV_USBPC)) {
            msg_enqueue(EVT_PC_INSERT);
        }
    } else {
        if (dev_offline_filter(DEV_USBPC)) {
            msg_enqueue(EVT_PC_REMOVE);
        }
    }
}
#endif // USB_SUPPORT_EN



