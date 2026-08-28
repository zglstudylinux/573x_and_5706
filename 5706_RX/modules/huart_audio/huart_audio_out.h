#ifndef _HUART_AUDIO_OUT_H
#define _HUART_AUDIO_OUT_H

typedef void (*audio_callback_t)(u8 *ptr, u32 samples, int ch_mode, void *params);

typedef struct {
    u8 mute;
    u8 sample_rate;
    u16 samples;
    audio_callback_t callback;
} huart_audio_out_cfg_t;

void huart_audio_out_input(u8 *ptr, u32 samples, int ch_mode, void *param);
void huart_audio_out_output_callback_set(audio_callback_t callback);
void huart_audio_out_init(u8 sample_rate, u16 samples, u8 channel);
void huart_audio_out_mute_set(uint8_t mute);
#endif //_WIRELESS_H
