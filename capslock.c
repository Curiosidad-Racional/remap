#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <unistd.h>


int main(void) {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);

    xcb_get_modifier_mapping_cookie_t cookie = xcb_get_modifier_mapping_unchecked(conn);
    xcb_get_modifier_mapping_reply_t *reply = xcb_get_modifier_mapping_reply(conn, cookie, NULL);
    if (reply == NULL) {
        printf("Null reply\n");
        return 0;
    }
    xcb_keycode_t key, *codes = xcb_get_modifier_mapping_keycodes(reply);
    xcb_keycode_t key_caps = xcb_key_sy

    for (unsigned int i = 0; i < 8; ++i) {
        for (unsigned int j = 0; j < reply->keycodes_per_modifier; ++j) {

        }
    }
    printf("property is \n");
    free(reply);
    xcb_disconnect(conn);

}