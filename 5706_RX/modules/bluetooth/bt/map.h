#ifndef __MAP_H
#define __MAP_H

int nibble_for_char(char c);
void bt_get_time(char *ptr, uint32_t len, uint8_t format);
void bt_map_data_callback(uint8_t *packet);
#endif // __MAP_H
