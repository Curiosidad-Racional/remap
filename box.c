#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

xcb_screen_t *scr;
xcb_connection_t *conn;
xcb_window_t win;
unsigned int white;
xcb_visualtype_t *visual_type;


void check_request(xcb_connection_t *dpy, xcb_void_cookie_t cookie, char *msg)
{
    xcb_generic_error_t *err = xcb_request_check(dpy, cookie);
    if (err != NULL) {
        fprintf(stderr, "%s: error code: %u.\n", msg, err->error_code);
        xcb_disconnect(dpy);
        exit(-1);
    }
}

xcb_gcontext_t get_font_gc(
    xcb_connection_t *dpy, xcb_window_t win, const char *font_name)
{
    xcb_void_cookie_t ck;
    char warn[] = "Can't open font";
    xcb_font_t font = xcb_generate_id(dpy);
    ck = xcb_open_font_checked(dpy, font, strlen(font_name), font_name);
    check_request(dpy, ck, warn);
    xcb_gcontext_t gc = xcb_generate_id(dpy);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t values[] = {0xffcccccc, 0xff111111, font};
    xcb_create_gc(dpy, gc, win, mask, values);
    xcb_close_font(dpy, font);
    return gc;
}


int makewindow()
{


    conn = xcb_connect(NULL, NULL);
    scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (scr == NULL) {
        fprintf(stderr, "Can't get current screen.\n");
        xcb_disconnect(conn);
        return EXIT_FAILURE;
    }
    
    win = xcb_generate_id(conn);
    white = scr->white_pixel;

    xcb_get_input_focus_reply_t *input_focus = xcb_get_input_focus_reply(
        conn, xcb_get_input_focus(conn), NULL);

    printf("focus: %lx\n", (unsigned long)input_focus);


    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {0xff111111, XCB_EVENT_MASK_EXPOSURE};
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win,
        input_focus->focus, //window.scr->root,
        0, 0,
        300, 200,
        0,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, //XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_COPY_FROM_PARENT, //window.scr->root_visual,
        mask,
        values);
    free(input_focus);
    xcb_icccm_set_wm_name(conn, win, XCB_ATOM_STRING, 8, strlen("xcr"), "xcr");
    xcb_map_window(conn, win);
    xcb_flush(conn);

    char text[] = "Hola";
    char warn[] = "Can't draw text";
    xcb_drawable_t gc = get_font_gc(
        conn, win, "-*-fixed-medium-*-*-*-18-*-*-*-*-*-*-*");
    xcb_void_cookie_t ck = xcb_image_text_8_checked(
        conn, strlen(text), win, gc, 12, 24, text);
    check_request(conn, ck, warn);
    xcb_free_gc(conn, gc);

    return 0;
}

int destroywindow() {
    xcb_destroy_window(conn, win);
    xcb_disconnect(conn);
    return 0;
}

int main(int argc, char **argv)
{

    makewindow();
    sleep(5);
    destroywindow();

    return 0;
}