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


static xcb_atom_t intern_atom(xcb_connection_t *conn, const char *atom)
{
    xcb_atom_t result = XCB_NONE;
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(conn,
            xcb_intern_atom(conn, 0, strlen(atom), atom), NULL);
    if (r)
        result = r->atom;
    free(r);
    return result;
}


int main(void) {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    xcb_grab_server(conn);
    printf("Getting focus\n");
    printf("_NET_ACTIVE_WINDOW: %u\n", intern_atom(conn, "_NET_ACTIVE_WINDOW"));
    xcb_get_input_focus_reply_t *input_focus = xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), NULL);
    printf("The window has ID %u\n", input_focus->focus);
    xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(conn, 0, 13, "_NET_WM_STATE");
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (!reply)
        return 0;
    printf("The _NET_WM_NAME atom has ID %u\n", reply->atom);


    xcb_get_property_reply_t *property = xcb_get_property_reply(
        conn, xcb_get_property(conn, false, input_focus->focus, reply->atom, XCB_ATOM_ATOM, 0, 2048), NULL);
    free(reply);
    free(input_focus);
    xcb_ungrab_server(conn);
    xcb_flush(conn);

    if (property == NULL) {
        printf("property null\n");
        return 0;
    }

    xcb_atom_t *state = (xcb_atom_t *)xcb_get_property_value(property);
    size_t size = xcb_get_property_value_length(property)/sizeof(xcb_atom_t);
    printf("len %lu\n", size);
    for (int i = 0; i < size; ++i) {
        printf("property is %u\n", state[i]);
    }
    free(property);
    xcb_disconnect(conn);

}