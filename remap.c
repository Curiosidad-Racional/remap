#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <getopt.h>
#define MAKRO_MAX 1024
//#define KEY_DELAY 100000


enum group_cls_e {
    WC_VOID, WC_EMACS, WC_TERM, WC_QUTEBROWSER, WC_OTHERS
};

enum group_key_e {
    GK_EMACS,
    GK_CTRL_X, GK_CTRL_X__R, GK_CTRL_X__T,
    GK_CTRL_C,
} group_key = GK_EMACS;


xcb_connection_t *g_conn;
xcb_window_t g_text_window = 0;
xcb_window_t g_focused_window = 0;

void store_focused_window() {
  xcb_get_input_focus_reply_t *input_focus = xcb_get_input_focus_reply(
      g_conn, xcb_get_input_focus(g_conn), NULL);
  g_focused_window = input_focus->focus;
  free(input_focus);
}

enum group_cls_e check_window_classname() {
  xcb_grab_server(g_conn);
  store_focused_window();
  xcb_get_property_reply_t *reply = xcb_get_property_reply(
      g_conn,
      xcb_get_property(
          g_conn, false, g_focused_window,
          XCB_ATOM_WM_CLASS, XCB_GET_PROPERTY_TYPE_ANY,
          0, 32), NULL);
  xcb_ungrab_server(g_conn);
  xcb_flush(g_conn);

  if (reply == NULL)
      return WC_VOID;
  int len = xcb_get_property_value_length(reply);
  if (len <= 0)
      return WC_VOID;

  char* wm_class = (char*)xcb_get_property_value(reply);
  if (strcmp(wm_class, "emacs") == 0) {
    free(reply);
    return WC_EMACS;
  } else if (strcmp(wm_class, "qutebrowser") == 0) {
    free(reply);
    return WC_QUTEBROWSER;
  } else if (strcmp(wm_class, "Alacritty") == 0 ||
             strcmp(wm_class, "urxvt") == 0 ||
             strcmp(wm_class, "st-256color") == 0) {
    free(reply);
    return WC_TERM;
  } else {
    free(reply);
    return WC_OTHERS;
  }
}


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
    xcb_connection_t *dpy, xcb_window_t win, const char *font_name,
    uint32_t fg_color, uint32_t bg_color)
{
    xcb_void_cookie_t ck;
    char warn[] = "Can't open font";
    xcb_font_t font = xcb_generate_id(dpy);
    ck = xcb_open_font_checked(dpy, font, strlen(font_name), font_name);
    check_request(dpy, ck, warn);
    xcb_gcontext_t gc = xcb_generate_id(dpy);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t values[] = {fg_color, bg_color, font};
    xcb_create_gc(dpy, gc, win, mask, values);
    xcb_close_font(dpy, font);
    return gc;
}

void show_text_window(char *text, uint32_t fg_color,
                      uint32_t bg_color, xcb_window_t focused_window) {
    if (g_text_window) {
      xcb_destroy_window(g_conn, g_text_window);
      xcb_flush(g_conn);
    }

    if (text == NULL) return;

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(g_conn)).data;
    if (screen == NULL) {
        fprintf(stderr, "Can't get current screen.\n");
        xcb_disconnect(g_conn);
        exit(EXIT_FAILURE);
    }

    g_text_window = xcb_generate_id(g_conn);
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {bg_color, XCB_EVENT_MASK_EXPOSURE};
    xcb_create_window(
        g_conn,
        XCB_COPY_FROM_PARENT,
        g_text_window,
        focused_window? focused_window : screen->root,
        0, 0,
        strlen(text) * 9 + 6, 18,
        0,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, //XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_COPY_FROM_PARENT, //window.scr->root_visual,
        mask,
        values);
    xcb_map_window(g_conn, g_text_window);
    xcb_flush(g_conn);

    char warn[] = "Can't draw text";
    xcb_drawable_t gc = get_font_gc(
        g_conn, g_text_window, "-*-fixed-medium-*-*-*-18-*-*-*-*-*-*-*",
        fg_color, bg_color);
    xcb_void_cookie_t ck = xcb_image_text_8_checked(
        g_conn, strlen(text), g_text_window, gc, 4, 14, text);
    check_request(g_conn, ck, warn);
    xcb_free_gc(g_conn, gc);
}


void delete_focused_window() {
  if (!g_focused_window) {
    store_focused_window();
    if (!g_focused_window)
      return;
  }

  xcb_intern_atom_cookie_t protocol_cookie =
      xcb_intern_atom(g_conn, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_reply_t* protocol_reply =
      xcb_intern_atom_reply(g_conn, protocol_cookie, 0);
  xcb_intern_atom_cookie_t delete_cookie =
      xcb_intern_atom(g_conn, 0, 16, "WM_DELETE_WINDOW");
  xcb_intern_atom_reply_t* delete_reply =
      xcb_intern_atom_reply(g_conn, delete_cookie, 0);
  
  xcb_client_message_event_t e = {
    .response_type = XCB_CLIENT_MESSAGE,
    .format = 32,
    .sequence = 0,
    .window = g_focused_window,
    .type = protocol_reply->atom,
    .data.data32[0] = delete_reply->atom,
    .data.data32[1] = XCB_CURRENT_TIME,
  };

  free(delete_reply);
  free(protocol_reply);

  xcb_send_event(g_conn, false, g_focused_window, XCB_EVENT_MASK_NO_EVENT, (char*)&e);
  xcb_flush(g_conn);
}


unsigned short keyboard_total = 0;
__s32 keyboard[] = {[0 ... 248] = 0};
__s32 fakboard[] = {[0 ... 248] = 0};
unsigned short fake_events_setted = 0;
struct input_event fake_events[] = {
    [0 ... 7] = {.type = EV_KEY, .code = 0, .value = 0}};

bool check_key1(const __u16 k1) {
  return keyboard[k1] && keyboard_total == keyboard[k1];
}

bool check_key2(const __u16 k1, const __u16 k2) {
  return keyboard[k2] && keyboard[k1] &&
         keyboard_total == keyboard[k1] + keyboard[k2];
}

unsigned short check_key1_digit(const __u16 k1) {
  if (!keyboard[k1]) return 0;
  for (__u16 k2 = KEY_1; k2 <= KEY_0; ++k2)
    if (keyboard[k2] && keyboard_total == keyboard[k1] + keyboard[k2])
        return k2;
  return 0;
}

bool check_key3(const __u16 k1, const __u16 k2, const __u16 k3) {
  return keyboard[k1] && keyboard[k2] && keyboard[k3] &&
         keyboard_total == keyboard[k1] + keyboard[k2] + keyboard[k3];
}

bool is_mod(const __u16 k) {
  switch (k) {
  case KEY_LEFTCTRL:
  case KEY_RIGHTCTRL:
  case KEY_LEFTALT:
  case KEY_RIGHTALT:
  case KEY_LEFTSHIFT:
  case KEY_RIGHTSHIFT:
  case KEY_LEFTMETA:
  case KEY_RIGHTMETA:
  case KEY_CAPSLOCK:
  case KEY_NUMLOCK:
  case KEY_SCROLLLOCK:
      return true;
  default:
      return false;
  }
}

bool makro_recording = false;
unsigned short makro_events_idx = 0;
struct input_event makro_events[] = {
    [0 ... MAKRO_MAX] = {.type = EV_KEY, .code = 0, .value = 0}};

void write_event(const struct input_event event) {
  //fprintf(stderr, "%d, %d, %d\n", event.type, event.code, event.value);
  fwrite(&event, sizeof(event), 1, stdout);
  //fflush(stdout);
  switch (event.value) {
  case 0:
    if (fakboard[event.code])
      fakboard[event.code] = 0;
    if (makro_recording && makro_events_idx) {
      break;
    }
    return;
  case 1:
    if (!fakboard[event.code])
      fakboard[event.code] = 1;
    if (makro_recording) {
      break;
    }
    return;
  default:
    return;
  }
  makro_events[makro_events_idx].value = event.value;
  makro_events[makro_events_idx].code = event.code;
  //fprintf(stderr, "RECORDED %d, %d, %d\n", makro_events[makro_events_idx].type, makro_events[makro_events_idx].code, makro_events[makro_events_idx].value);
  if (++makro_events_idx > MAKRO_MAX) {
    makro_recording = false;
    makro_events_idx = 0;
    char text[] = "MAKRO MAX EXCEDED";
    show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
  }
}

void write_event_code(const __u16 code, const __s32 value) {
  struct input_event event = {.type = EV_KEY, .code = code, .value = value};
  write_event(event);
}

void mod0_mod1_keyremap(const __u16 mod, const __u16 mod1,
                        const __u16 keyremap) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  fake_events[0].code = keyremap;
  fake_events_setted = 1;
}

void mod1_key0_remap(const __u16 mod, const __u16 key, const __u16 remap) {
  if (!fakboard[mod]) {
    write_event_code(mod, 1);
  }
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod1_key0_mod1_remap(const __u16 mod, const __u16 key, const __u16 mod1,
                          const __u16 remap) {
  if (!fakboard[mod]) {
    write_event_code(mod, 1);
  }
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_key0_remap(const __u16 mod, const __u16 key, const __u16 remap) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  } else if (fakboard[key]) {
    write_event_code(key, 0);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void key0_mod1_remap(const __u16 key, const __u16 mod1, const __u16 remap) {
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void key0_mod1_mod1_remap(
    const __u16 key, const __u16 mod1, const __u16 mod2, const __u16 remap) {
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  if (!fakboard[mod2]) {
    write_event_code(mod2, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_key0_mod1_remap(const __u16 mod, const __u16 key, const __u16 mod1,
                          const __u16 remap) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  } else if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_mod0_key0_mod1_remap(const __u16 mod, const __u16 mod1,
                               const __u16 key, const __u16 mod2,
                               const __u16 remap) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  }
  if (fakboard[mod1]) {
    write_event_code(mod1, 0);
  }
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod2]) {
    write_event_code(mod2, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_key0_mod1_mod1_remap(const __u16 mod, const __u16 key,
                               const __u16 mod1, const __u16 mod2,
                               const __u16 remap) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  } else if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  if (!fakboard[mod2]) {
    write_event_code(mod2, 1);
  }
  fake_events[0].code = remap;
  fake_events_setted = 1;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void key0_remap_remap(const __u16 key, const __u16 remap, const __u16 remap1) {
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  fake_events[0].code = remap;
  fake_events[0].value = 1;
  fake_events[1].code = remap;
  fake_events[1].value = 0;
  fake_events[2].code = remap1;
  fake_events[2].value = 1;
  fake_events[3].code = remap1;
  fake_events[3].value = 0;
  fake_events_setted = 4;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_key0_mod1_remap_remap(const __u16 mod, const __u16 key,
                                const __u16 mod1, const __u16 remap,
                                const __u16 remap1) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  } else if (fakboard[key]) {
    write_event_code(key, 0);
  }
  if (!fakboard[mod1]) {
    write_event_code(mod1, 1);
  }
  fake_events[0].code = remap;
  fake_events[0].value = 1;
  fake_events[1].code = remap;
  fake_events[1].value = 0;
  fake_events[2].code = remap1;
  fake_events[2].value = 1;
  fake_events[3].code = remap1;
  fake_events[3].value = 0;
  fake_events_setted = 4;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void mod0_mod0_key0_remap_mod_remap_remap(const __u16 mod, const __u16 mod1,
                                          const __u16 key, const __u16 remap,
                                          const __u16 mod2, const __u16 remap1,
                                          const __u16 remap2) {
  if (fakboard[mod]) {
    write_event_code(mod, 0);
  }
  if (fakboard[mod1]) {
    write_event_code(mod1, 0);
  }
  if (fakboard[key]) {
    write_event_code(key, 0);
  }
  fake_events[0].code = remap;
  fake_events[0].value = 1;
  fake_events[1].code = remap;
  fake_events[1].value = 0;
  fake_events[2].code = mod2;
  fake_events[2].value = 1;
  fake_events[3].code = remap1;
  fake_events[3].value = 1;
  fake_events[4].code = remap1;
  fake_events[4].value = 0;
  fake_events[5].code = remap2;
  fake_events[5].value = 1;
  fake_events[6].code = remap2;
  fake_events[6].value = 0;
  fake_events[7].code = mod2;
  fake_events[7].value = 0;
  fake_events_setted = 8;
  keyboard_total -= keyboard[key];
  keyboard[key] = 0;
}

void clear_fakes() {
  struct input_event fake_event = {.type = EV_KEY};
  for (unsigned short i = 1; i < 248; ++i) {
    if (fakboard[i] != keyboard[i]) {
      fake_event.code = i;
      fake_event.value = keyboard[i];
      write_event(fake_event);
    }
  }
  fake_events_setted = 0;
}

int main(int argc, char *argv[]) {

  char opt;
  __u16 digit;
  __u16 right_alt = KEY_RIGHTALT;
  __u16 left_right_ctrl = KEY_LEFTCTRL;
  while ((opt = getopt(argc, argv, "s")) != EOF)
    switch (opt) {
    case 's':
      right_alt = KEY_LEFTCTRL;
      left_right_ctrl = KEY_RIGHTALT;
      break;
    }

  setbuf(stdin, NULL), setbuf(stdout, NULL);
  g_conn = xcb_connect(NULL, NULL);
  if (!g_conn || xcb_connection_has_error(g_conn)) {
      fprintf(stderr, "xcb connection failed.\n");
      return 2;
  }

  unsigned int repeat = 0;
  bool skip_remap = false;
  bool select_mode = false;
  struct input_event event;
  while (fread(&event, sizeof(event), 1, stdin) == 1) {
    if (event.type == EV_MSC && event.code == MSC_SCAN)
      continue;
    if (event.type != EV_KEY) {
      fwrite(&event, sizeof(event), 1, stdout);
      //fflush(stdout);
      continue;
    }
    // fprintf(stderr, "%u, %u, %d\n", event.type, event.code, event.value);
    if (event.code == KEY_RIGHTALT)
      event.code = right_alt;
    else if (event.code == KEY_LEFTCTRL || event.code == KEY_RIGHTCTRL)
      event.code = left_right_ctrl;
    else if (event.code == KEY_RIGHTSHIFT)
      event.code = KEY_LEFTSHIFT;

    switch (event.value) {
    case 0:
      if (fake_events_setted) {
        clear_fakes();
      }
      keyboard_total += 0 - keyboard[event.code];
      keyboard[event.code] = 0;
      break;
    case 1:
      if (fake_events_setted) {
        clear_fakes();
      }
      keyboard_total += 1 - keyboard[event.code];
      keyboard[event.code] = 1;
      if (check_key2(KEY_ESC, KEY_SPACE)) {
        skip_remap = !skip_remap;
        continue;
      }
      if (skip_remap) {
        break;
      }
      // fprintf(stderr, "group: %d.\n", group_key);
      switch (group_key) {
      case GK_EMACS:
        // REPEAT
        digit = check_key1_digit(KEY_LEFTCTRL);
        if (digit) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_TERM:
          case WC_OTHERS:
            repeat = repeat * 10 + (digit-1) % 10;
            char text[16];
            sprintf(text, "C-%d", repeat);
            show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
            continue;
          }
        }
        // CTRL MOVEMENT
        else if (check_key2(KEY_LEFTCTRL, KEY_N)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_N, KEY_LEFTSHIFT,
                                   KEY_DOWN);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_N, KEY_LEFTSHIFT,
                                   KEY_DOWN);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_N, KEY_DOWN);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_P)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_P, KEY_LEFTSHIFT, KEY_UP);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_P, KEY_LEFTSHIFT, KEY_UP);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_P, KEY_UP);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_F, KEY_LEFTSHIFT,
                                   KEY_RIGHT);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_F, KEY_LEFTSHIFT,
                                   KEY_RIGHT);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_F, KEY_RIGHT);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_B)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_B, KEY_LEFTSHIFT,
                                   KEY_LEFT);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_B, KEY_LEFTSHIFT,
                                   KEY_LEFT);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_B, KEY_LEFT);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_A)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_A, KEY_LEFTSHIFT,
                                   KEY_HOME);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_A, KEY_LEFTSHIFT,
                                   KEY_HOME);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_A, KEY_HOME);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_E)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_E, KEY_LEFTSHIFT, KEY_END);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_E, KEY_LEFTSHIFT, KEY_END);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_E, KEY_END);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_V)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_V, KEY_LEFTSHIFT,
                                   KEY_PAGEDOWN);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_V, KEY_LEFTSHIFT,
                                   KEY_PAGEDOWN);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_V, KEY_PAGEDOWN);
            break;
          case WC_TERM:
            mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_V, KEY_LEFTSHIFT,
                                 KEY_PAGEDOWN);
            break;
          }
        }
        // ALT MOVEMENT
        else if (check_key2(KEY_LEFTALT, KEY_V)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_V, KEY_LEFTSHIFT,
                                   KEY_PAGEUP);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_V, KEY_LEFTSHIFT,
                                   KEY_PAGEUP);
            else
              mod0_key0_remap(KEY_LEFTALT, KEY_V, KEY_PAGEUP);
            break;
          case WC_TERM:
            mod0_key0_mod1_remap(KEY_LEFTALT, KEY_V, KEY_LEFTSHIFT,
                                 KEY_PAGEUP);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_N)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_N, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_DOWN);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_N, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_DOWN);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_N, KEY_LEFTCTRL, KEY_DOWN);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_P)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_P, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_UP);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_P, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_UP);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_P, KEY_LEFTCTRL, KEY_UP);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_F)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_RIGHT);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_RIGHT);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTCTRL, KEY_RIGHT);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_B)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_B, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_LEFT);
            break;
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_B, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_LEFT);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_B, KEY_LEFTCTRL, KEY_LEFT);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_102ND)) {
          switch (check_window_classname()) {
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_102ND, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_HOME);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_102ND, KEY_LEFTCTRL, KEY_HOME);
            break;
          }
        } else if (check_key3(KEY_LEFTALT, KEY_LEFTSHIFT, KEY_102ND)) { // KEY_RIGHTSHIFT
          switch (check_window_classname()) {
          case WC_OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_102ND, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_END);
            else
              mod0_mod0_key0_mod1_remap(KEY_LEFTALT, KEY_LEFTSHIFT, KEY_102ND,
                                        KEY_LEFTCTRL, KEY_END);
            break;
          }
        }
        // COMMANDS
        else if (check_key2(KEY_LEFTALT, KEY_X)) {
          switch (check_window_classname()) {
          case WC_OTHERS:
            mod0_key0_remap(KEY_LEFTALT, KEY_X, KEY_F6);
            break;
          }
        }
        // SELECT MODE
        else if (check_key2(KEY_LEFTCTRL, KEY_SPACE)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_OTHERS:
            select_mode = !select_mode;
            continue;
          }
        }
        // COPY-PASTE
        else if (check_key2(KEY_LEFTCTRL, KEY_Y)) {
          switch (check_window_classname()) {
          case WC_TERM:
            mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_Y, KEY_LEFTSHIFT, KEY_INSERT);
            break;
          case WC_QUTEBROWSER:
          case WC_OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_Y, KEY_V);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_W)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_W, KEY_X);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_W)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_OTHERS:
            mod0_key0_mod1_remap(KEY_LEFTALT, KEY_W, KEY_LEFTCTRL, KEY_C);
            if (select_mode)
              select_mode = false;
            break;
          }
        }
        // EDITION
        else if (check_key1(KEY_DELETE) || check_key1(KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_OTHERS:
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_D)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_key0_remap(KEY_LEFTCTRL, KEY_D, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_D)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_key0_mod1_remap(KEY_LEFTALT, KEY_D, KEY_LEFTCTRL, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_SLASH)) {
          // KEY_SLASH -> KEY_MINUS
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_SLASH, KEY_LEFTCTRL, KEY_Z);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTALT, KEY_LEFTSHIFT, KEY_SLASH)) {
          // KEY_SLASH -> KEY_MINUS
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_LEFTALT, KEY_SLASH,
                                      KEY_LEFTCTRL, KEY_Y);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_K)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_key0_mod1_remap_remap(KEY_LEFTCTRL, KEY_K, KEY_LEFTSHIFT,
                                       KEY_END, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_mod1_keyremap(KEY_LEFTALT, KEY_LEFTCTRL, KEY_BACKSPACE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_TERM:
            mod0_mod1_keyremap(KEY_LEFTCTRL, KEY_LEFTALT, KEY_BACKSPACE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
            if (select_mode)
              select_mode = false;
            break;
          case WC_OTHERS:
            mod0_mod0_key0_remap_mod_remap_remap(
                KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_BACKSPACE, KEY_HOME,
                KEY_LEFTSHIFT, KEY_END, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        }
        // SEARCH
        else if (check_key2(KEY_LEFTCTRL, KEY_S)) {
          switch (check_window_classname()) {
          case WC_OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_S, KEY_F);
            break;
          }
        }
        // GROUP KEY
        else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          switch (check_window_classname()) {
          case WC_OTHERS:
            group_key = GK_CTRL_C;
            char text[] = "C+C -> {T,C+C,C+D,C+E,C+0-9}";
            show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
            continue;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_X)) {
          switch (check_window_classname()) {
          case WC_TERM:
          case WC_OTHERS:
            group_key = GK_CTRL_X;
            char text[] = "C+X -> {2,3,5,B,E,K,O,R,T,U,(,),S+O,C+C,C+F,C+S}";
            show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
            continue;
          }
        }
        break;
      case GK_CTRL_X:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_B)) {
          key0_mod1_mod1_remap(KEY_B, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_A);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_U)) {
          key0_mod1_mod1_remap(KEY_U, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_T);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_K)) {
          key0_mod1_remap(KEY_K, KEY_LEFTCTRL, KEY_W);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_O)) {
          key0_mod1_remap(KEY_O, KEY_LEFTCTRL, KEY_TAB);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_O)) {
          key0_mod1_mod1_remap(KEY_O, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_TAB);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_2)) {
          key0_mod1_remap(KEY_2, KEY_LEFTCTRL, KEY_T);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_3)) {
          key0_mod1_remap(KEY_3, KEY_LEFTCTRL, KEY_T);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_5)) {
          key0_mod1_remap(KEY_5, KEY_LEFTCTRL, KEY_N);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_S)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          mod1_key0_remap(KEY_LEFTCTRL, KEY_F, KEY_O);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          //mod1_key0_mod1_remap(KEY_LEFTCTRL, KEY_C, KEY_LEFTSHIFT, KEY_W);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          delete_focused_window();
          continue;
        } else if (check_key1(KEY_R)) {
          group_key = GK_CTRL_X__R;
          char text[] = "C+X R -> {M,B}";
          show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
          continue;
        } else if (check_key1(KEY_T)) {
          group_key = GK_CTRL_X__T;
          char text[] = "C+X T -> {0,2,M,O,S-M,S-O}";
          show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
          continue;
        }
        // MACROS
        else if (check_key1(KEY_E)) {
          if (makro_recording) {
            makro_events_idx = 0;
            makro_recording = false;
          } else if (makro_events_idx) {
            repeat = repeat? ++repeat: 2;
            switch (repeat) {
            case 1:
              repeat = 0;
            case 0:
              for (unsigned short i = 0; i < makro_events_idx; ++i)
                write_event(makro_events[i]);
              break;
            default:
              for (unsigned short i = 0; i < makro_events_idx; ++i)
                write_event(makro_events[i]);
              while (--repeat) {
                //usleep(KEY_DELAY);
                for (unsigned short i = 0; i < makro_events_idx; ++i)
                  write_event(makro_events[i]);
              }
            }
          }
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key2(KEY_LEFTSHIFT, KEY_8)) {
          makro_events_idx = 0;
          makro_recording = true;
          group_key = GK_EMACS;
          char text[] = "RECORDING MACRO";
          show_text_window(text, 0xffcccccc, 0xffa32cc4, g_focused_window);
          continue;
        } else if (check_key2(KEY_LEFTSHIFT, KEY_9)) {
          makro_recording = false;
          while (makro_events_idx && is_mod(makro_events[makro_events_idx].code))
            --makro_events_idx;
          if (makro_events_idx && makro_events[makro_events_idx].code == KEY_9)
            --makro_events_idx;
          while (makro_events_idx && is_mod(makro_events[makro_events_idx].code))
            --makro_events_idx;
          if (makro_events_idx && makro_events[makro_events_idx].code == KEY_X)
            --makro_events_idx;
          while (makro_events_idx && is_mod(makro_events[makro_events_idx].code))
            --makro_events_idx;
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_C:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_T)) {
          key0_remap_remap(KEY_T, KEY_F6, KEY_F6);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_D)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_E)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1_digit(KEY_LEFTCTRL)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_X__R:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_M)) {
          key0_mod1_remap(KEY_M, KEY_LEFTCTRL, KEY_D);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_B)) {
          key0_mod1_mod1_remap(KEY_B, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_O);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_X__T:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_0)) {
          key0_mod1_remap(KEY_0, KEY_LEFTCTRL, KEY_W);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_2)) {
          key0_mod1_remap(KEY_2, KEY_LEFTCTRL, KEY_T);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_O)) {
          key0_mod1_remap(KEY_O, KEY_LEFTCTRL, KEY_PAGEDOWN);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_O)) {
          mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_O, KEY_LEFTCTRL, KEY_PAGEUP);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_M)) {
          key0_mod1_mod1_remap(KEY_M, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_PAGEDOWN);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_M)) {
          key0_mod1_mod1_remap(KEY_M, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_PAGEUP);
          group_key = GK_EMACS;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (!is_mod(event.code))
            continue;
        break;
      }
      // WRITE EVENT
      if (!repeat || is_mod(event.code)) break;
      show_text_window(NULL, 0, 0, g_focused_window);
      switch (fake_events_setted) {
      case 0:
        write_event(event);
        while (--repeat) {
          //usleep(KEY_DELAY);
          event.value = 0;
          write_event(event);
          event.value = 1;
          write_event(event);
        }
        break;
      case 1:
        fake_events[0].value = 1;
        write_event(fake_events[0]);
        while (--repeat) {
          //usleep(KEY_DELAY);
          fake_events[0].value = 0;
          write_event(fake_events[0]);
          fake_events[0].value = 1;
          write_event(fake_events[0]);
        }
        break;
      default:
        for (unsigned short i = 0; i < fake_events_setted; ++i)
          write_event(fake_events[i]);
        while (--repeat) {
          //usleep(KEY_DELAY);
          for (unsigned short i = 0; i < fake_events_setted; ++i)
            write_event(fake_events[i]);
        }
      }
      continue;
    case 2:
      if (group_key != GK_EMACS || repeat)
        continue;
      break;
    }

    // WRITE EVENT
    switch (fake_events_setted) {
    case 0:
      write_event(event);
      break;
    case 1:
      fake_events[0].value = event.value;
      write_event(fake_events[0]);
      break;
    default:
      for (unsigned short i = 0; i < fake_events_setted; ++i)
        write_event(fake_events[i]);
    }
  }
  if (g_text_window)
      xcb_destroy_window(g_conn, g_text_window);
  xcb_disconnect(g_conn);
  return 0;
}
