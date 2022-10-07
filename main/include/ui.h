#ifndef _UI_H_
#define _UI_H_

typedef int (*echo_cb_t)(void);
typedef int (*start_cb_t)(void);

int ui_start(void);
int ui_set_rx_msg(const char* txt);
int ui_en_echo(int en);
int ui_set_echo_callback(echo_cb_t cb);
int ui_set_start_callback(start_cb_t cb);
int ui_lic_error(const char* dev_id);

#endif //_UI_H_
