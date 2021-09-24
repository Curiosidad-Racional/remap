#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <unistd.h>

/* int main(void) { */
/* xcb_get_property_cookie_t cookie; */
/*     xcb_get_property_reply_t *reply; */

/*     /\* These atoms are predefined in the X11 protocol. *\/ */
/*     xcb_atom_t property = XCB_ATOM_WM_NAME; */
/*     xcb_atom_t type = XCB_ATOM_STRING; */

/*     // TODO: a reasonable long_length for WM_NAME? */
/*     cookie = xcb_get_property(c, 0, window, property, type, 0, 0); */
/*     if ((reply = xcb_get_property_reply(c, cookie, NULL))) { */
/*         int len = xcb_get_property_value_length(reply); */
/*         if (len == 0) { */
/*             printf("TODO\n"); */
/*             free(reply); */
/*             return; */
/*         } */
/*         printf("WM_NAME is %.*s\n", len, */
/*                (char*)xcb_get_property_value(reply)); */
/*     } */
/*     free(reply); */
/* } */


int main(void) {
    sleep(5);
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    xcb_grab_server(conn);
    xcb_get_input_focus_reply_t *input_focus = xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), NULL);
    xcb_get_property_reply_t *wm_class = xcb_get_property_reply(
        conn, xcb_get_property(conn, false, input_focus->focus, XCB_ATOM_WM_CLASS, XCB_GET_PROPERTY_TYPE_ANY, 0, 32), NULL);
    xcb_ungrab_server(conn);
    xcb_flush(conn);

    if (wm_class == NULL)
        return 0;
    int len = xcb_get_property_value_length(wm_class);
    if (len <= 0)
        return 0;
    
    free(input_focus);
    printf("WM_CLASS is %s\n", (char*)xcb_get_property_value(wm_class));
    if (strcmp((char*)xcb_get_property_value(wm_class), "emacs") == 0)
        printf("emacs ok\n");
    free(wm_class);
    xcb_disconnect(conn);

}