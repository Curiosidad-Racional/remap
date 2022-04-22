#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <getopt.h>
#define MAKRO_MAX     1024
#define WRITE_DELAY  20000
#define REPEAT_DELAY 20000

bool verbose = false;
enum group_cls_e {
    WC_VOID, WC_EMACS, WC_TERM, WC_QUTEBROWSER, WC_OTHERS
};

enum group_key_e {
    GK_NULL,
    GK_CTRL_X, GK_CTRL_X__R, GK_CTRL_X__T,
    GK_CTRL_C,
} group_key = GK_NULL;


xcb_connection_t *g_conn;
xcb_window_t g_text_window = 0;
xcb_window_t g_focused_window = 0;

void store_focused_window() {
  xcb_get_input_focus_reply_t *input_focus = xcb_get_input_focus_reply(
      g_conn, xcb_get_input_focus(g_conn), NULL);
  g_focused_window = input_focus->focus;
  free(input_focus);
}

bool temp_remap = false;
enum group_cls_e check_window_classname() {
  if (temp_remap)
      return WC_OTHERS;
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
  free(reply);
  if (strcmp(wm_class, "Alacritty") == 0 ||
      strcmp(wm_class, "emacs") == 0) {
    return WC_EMACS;
  } else if (strcmp(wm_class, "qutebrowser") == 0) {
    return WC_QUTEBROWSER;
  } else if (strcmp(wm_class, "urxvt") == 0 ||
             strcmp(wm_class, "st-256color") == 0) {
    return WC_TERM;
  } else {
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

char *key_names[] = {
    "KEY_RESERVED",
    "KEY_ESC",
    "KEY_1",
    "KEY_2",
    "KEY_3",
    "KEY_4",
    "KEY_5",
    "KEY_6",
    "KEY_7",
    "KEY_8",
    "KEY_9",
    "KEY_0",
    "KEY_MINUS",
    "KEY_EQUAL",
    "KEY_BACKSPACE",
    "KEY_TAB",
    "KEY_Q",
    "KEY_W",
    "KEY_E",
    "KEY_R",
    "KEY_T",
    "KEY_Y",
    "KEY_U",
    "KEY_I",
    "KEY_O",
    "KEY_P",
    "KEY_LEFTBRACE",
    "KEY_RIGHTBRACE",
    "KEY_ENTER",
    "KEY_LEFTCTRL",
    "KEY_A",
    "KEY_S",
    "KEY_D",
    "KEY_F",
    "KEY_G",
    "KEY_H",
    "KEY_J",
    "KEY_K",
    "KEY_L",
    "KEY_SEMICOLON",
    "KEY_APOSTROPHE",
    "KEY_GRAVE",
    "KEY_LEFTSHIFT",
    "KEY_BACKSLASH",
    "KEY_Z",
    "KEY_X",
    "KEY_C",
    "KEY_V",
    "KEY_B",
    "KEY_N",
    "KEY_M",
    "KEY_COMMA",
    "KEY_DOT",
    "KEY_SLASH",
    "KEY_RIGHTSHIFT",
    "KEY_KPASTERISK",
    "KEY_LEFTALT",
    "KEY_SPACE",
    "KEY_CAPSLOCK",
    "KEY_F1",
    "KEY_F2",
    "KEY_F3",
    "KEY_F4",
    "KEY_F5",
    "KEY_F6",
    "KEY_F7",
    "KEY_F8",
    "KEY_F9",
    "KEY_F10",
    "KEY_NUMLOCK",
    "KEY_SCROLLLOCK",
    "KEY_KP7",
    "KEY_KP8",
    "KEY_KP9",
    "KEY_KPMINUS",
    "KEY_KP4",
    "KEY_KP5",
    "KEY_KP6",
    "KEY_KPPLUS",
    "KEY_KP1",
    "KEY_KP2",
    "KEY_KP3",
    "KEY_KP0",
    "KEY_KPDOT",
    "unknown",
    "KEY_ZENKAKUHANKAKU",
    "KEY_102ND",
    "KEY_F11",
    "KEY_F12",
    "KEY_RO",
    "KEY_KATAKANA",
    "KEY_HIRAGANA",
    "KEY_HENKAN",
    "KEY_KATAKANAHIRAGANA",
    "KEY_MUHENKAN",
    "KEY_KPJPCOMMA",
    "KEY_KPENTER",
    "KEY_RIGHTCTRL",
    "KEY_KPSLASH",
    "KEY_SYSRQ",
    "KEY_RIGHTALT",
    "KEY_LINEFEED",
    "KEY_HOME",
    "KEY_UP",
    "KEY_PAGEUP",
    "KEY_LEFT",
    "KEY_RIGHT",
    "KEY_END",
    "KEY_DOWN",
    "KEY_PAGEDOWN",
    "KEY_INSERT",
    "KEY_DELETE",
    "KEY_MACRO",
    "KEY_MUTE",
    "KEY_VOLUMEDOWN",
    "KEY_VOLUMEUP",
    "KEY_POWER",
    "KEY_KPEQUAL",
    "KEY_KPPLUSMINUS",
    "KEY_PAUSE",
    "KEY_SCALE",
    "KEY_KPCOMMA",
    "KEY_HANGEUL",
    "KEY_HANJA",
    "KEY_YEN",
    "KEY_LEFTMETA",
    "KEY_RIGHTMETA",
    "KEY_COMPOSE",
    "KEY_STOP",
    "KEY_AGAIN",
    "KEY_PROPS",
    "KEY_UNDO",
    "KEY_FRONT",
    "KEY_COPY",
    "KEY_OPEN",
    "KEY_PASTE",
    "KEY_FIND",
    "KEY_CUT",
    "KEY_HELP",
    "KEY_MENU",
    "KEY_CALC",
    "KEY_SETUP",
    "KEY_SLEEP",
    "KEY_WAKEUP",
    "KEY_FILE",
    "KEY_SENDFILE",
    "KEY_DELETEFILE",
    "KEY_XFER",
    "KEY_PROG1",
    "KEY_PROG2",
    "KEY_WWW",
    "KEY_MSDOS",
    "KEY_COFFEE",
    "KEY_ROTATE_DISPLAY",
    "KEY_CYCLEWINDOWS",
    "KEY_MAIL",
    "KEY_BOOKMARKS",
    "KEY_COMPUTER",
    "KEY_BACK",
    "KEY_FORWARD",
    "KEY_CLOSECD",
    "KEY_EJECTCD",
    "KEY_EJECTCLOSECD",
    "KEY_NEXTSONG",
    "KEY_PLAYPAUSE",
    "KEY_PREVIOUSSONG",
    "KEY_STOPCD",
    "KEY_RECORD",
    "KEY_REWIND",
    "KEY_PHONE",
    "KEY_ISO",
    "KEY_CONFIG",
    "KEY_HOMEPAGE",
    "KEY_REFRESH",
    "KEY_EXIT",
    "KEY_MOVE",
    "KEY_EDIT",
    "KEY_SCROLLUP",
    "KEY_SCROLLDOWN",
    "KEY_KPLEFTPAREN",
    "KEY_KPRIGHTPAREN",
    "KEY_NEW",
    "KEY_REDO",
    "KEY_F13",
    "KEY_F14",
    "KEY_F15",
    "KEY_F16",
    "KEY_F17",
    "KEY_F18",
    "KEY_F19",
    "KEY_F20",
    "KEY_F21",
    "KEY_F22",
    "KEY_F23",
    "KEY_F24",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "KEY_PLAYCD",
    "KEY_PAUSECD",
    "KEY_PROG3",
    "KEY_PROG4",
    "KEY_DASHBOARD",
    "KEY_SUSPEND",
    "KEY_CLOSE",
    "KEY_PLAY",
    "KEY_FASTFORWARD",
    "KEY_BASSBOOST",
    "KEY_PRINT",
    "KEY_HP",
    "KEY_CAMERA",
    "KEY_SOUND",
    "KEY_QUESTION",
    "KEY_EMAIL",
    "KEY_CHAT",
    "KEY_SEARCH",
    "KEY_CONNECT",
    "KEY_FINANCE",
    "KEY_SPORT",
    "KEY_SHOP",
    "KEY_ALTERASE",
    "KEY_CANCEL",
    "KEY_BRIGHTNESSDOWN",
    "KEY_BRIGHTNESSUP",
    "KEY_MEDIA",
    "KEY_SWITCHVIDEOMODE",
    "KEY_KBDILLUMTOGGLE",
    "KEY_KBDILLUMDOWN",
    "KEY_KBDILLUMUP",
    "KEY_SEND",
    "KEY_REPLY",
    "KEY_FORWARDMAIL",
    "KEY_SAVE",
    "KEY_DOCUMENTS",
    "KEY_BATTERY",
    "KEY_BLUETOOTH",
    "KEY_WLAN",
    "KEY_UWB",
    "KEY_UNKNOWN",
    "KEY_VIDEO_NEXT",
    "KEY_VIDEO_PREV",
    "KEY_BRIGHTNESS_CYCLE",
    "KEY_BRIGHTNESS_AUTO",
    "KEY_DISPLAY_OFF",
    "KEY_WWAN",
    "KEY_RFKILL",
    "KEY_MICMUTE",
    };

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
  if (verbose)
    fprintf(stderr, "%d, %s, %d\n", event.type, key_names[event.code], event.value);
  fwrite(&event, sizeof(struct input_event), 1, stdout);
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
  if (++makro_events_idx > MAKRO_MAX) {
    makro_recording = false;
    makro_events_idx = 0;
    char text[] = "MAKRO MAX EXCEDED";
    show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
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
  // First undo keyup
  for (unsigned short i = 1; i < 248; ++i) {
    if (!keyboard[i] && fakboard[i] != keyboard[i]) {
      fake_event.code = i;
      fake_event.value = keyboard[i];
      write_event(fake_event);
    }
  }
  // Second undo keydown
  for (unsigned short i = 1; i < 248; ++i) {
    if (keyboard[i] && fakboard[i] != keyboard[i]) {
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
  while ((opt = getopt(argc, argv, "sv")) != EOF)
    switch (opt) {
    case 's':
      right_alt = KEY_LEFTCTRL;
      left_right_ctrl = KEY_RIGHTALT;
      break;
    case 'v':
      verbose = true;
      break;
    }

  setbuf(stdin, NULL), setbuf(stdout, NULL);
  g_conn = xcb_connect(NULL, NULL);
  if (!g_conn || xcb_connection_has_error(g_conn)) {
      fprintf(stderr, "xcb connection failed.\n");
      return 2;
  }

  char text_repeat[32];
  char text[256];
  bool skip_remap = false;
  bool select_mode = false;
  const struct input_event evsyn = {.type = EV_SYN, .code = SYN_REPORT, .value = 0};
  struct input_event event = {.type = EV_KEY, .code = 1, .value = 0};
  unsigned int repeat = sizeof(key_names)/sizeof(key_names[0]);
  fwrite(&event, sizeof(struct input_event), 1, stdout);
  //fflush(stdout);
  while (++(event.code) < repeat) {
    write_event(evsyn);
    //fflush(stdout);
    usleep(WRITE_DELAY);
    fwrite(&event, sizeof(struct input_event), 1, stdout);
    //fflush(stdout);
  }
  repeat = 0;
  while (fread(&event, sizeof(event), 1, stdin) == 1) {
    if (event.type == EV_MSC && event.code == MSC_SCAN)
      continue;
    if (event.type != EV_KEY) {
      fwrite(&event, sizeof(struct input_event), 1, stdout);
      //fflush(stdout);
      continue;
    }
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
      if (check_key2(KEY_CAPSLOCK, KEY_LEFTSHIFT)) {
        if (skip_remap)
          skip_remap = false;
        temp_remap = true;
        continue;
      }
      if (check_key2(KEY_CAPSLOCK, KEY_LEFTALT)) {
        skip_remap = !skip_remap;
        continue;
      }
      if (skip_remap) {
        break;
      }
      switch (group_key) {
      case GK_NULL:
        // REPEAT
        digit = check_key1_digit(KEY_LEFTCTRL);
        if (digit) {
          switch (check_window_classname()) {
          case WC_QUTEBROWSER:
          case WC_TERM:
          case WC_OTHERS:
            repeat = repeat * 10 + (digit-1) % 10;
            if (!repeat) break;
            if (makro_recording) {
              sprintf(text_repeat, "[RM] C-%d", repeat);
            } else {
              sprintf(text_repeat, "C-%d", repeat);
            }
            show_text_window(text_repeat, 0xffcccccc, 0xff4530ff, g_focused_window);
            continue;
          }
        }
        // REPLACEMENTS
        else if (check_key2(KEY_RIGHTALT, KEY_ESC)) {
          mod1_key0_remap(KEY_RIGHTALT, KEY_ESC, KEY_GRAVE); // KEY_BACKSLASH
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
            if (repeat) {
              strcpy(text, text_repeat);
              strcat(text, " ");
            } else if (makro_recording) {
              strcpy(text, "[RM] ");
            } else {
              text[0] = '\0';
            }
            strcat(text, "C+C -> {T,C+A,C+B,C+C,C+D,C+E,C+F,C+N,C+P,C+0-9}");
            show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
            continue;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_X)) {
          switch (check_window_classname()) {
          case WC_TERM:
          case WC_OTHERS:
            group_key = GK_CTRL_X;
            if (repeat) {
              strcpy(text, text_repeat);
              strcat(text, " ");
            } else if (makro_recording) {
              strcpy(text, "[RM] ");
            } else {
              text[0] = '\0';
            }
            strcat(text, "C+X -> {2,3,5,B,E,K,O,R,T,U,(,),S+O,C+C,C+F,C+S}");
            show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
            continue;
          }
        } else if (temp_remap && (
                    check_key1(KEY_ENTER) ||
                    check_key1(KEY_TAB) ||
                    check_key1(KEY_ESC)))
          temp_remap = false;
        break;
      case GK_CTRL_X:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_B)) {
          key0_mod1_mod1_remap(KEY_B, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_A);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_U)) {
          key0_mod1_mod1_remap(KEY_U, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_T);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_K)) {
          key0_mod1_remap(KEY_K, KEY_LEFTCTRL, KEY_W);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_O)) {
          key0_mod1_remap(KEY_O, KEY_LEFTCTRL, KEY_TAB);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_O)) {
          key0_mod1_mod1_remap(KEY_O, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_TAB);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_2)) {
          key0_mod1_remap(KEY_2, KEY_LEFTCTRL, KEY_T);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_3)) {
          key0_mod1_remap(KEY_3, KEY_LEFTCTRL, KEY_T);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_5)) {
          key0_mod1_remap(KEY_5, KEY_LEFTCTRL, KEY_N);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_S)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          mod1_key0_remap(KEY_LEFTCTRL, KEY_F, KEY_O);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          //mod1_key0_mod1_remap(KEY_LEFTCTRL, KEY_C, KEY_LEFTSHIFT, KEY_W);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          delete_focused_window();
          continue;
        } else if (check_key1(KEY_R)) {
          group_key = GK_CTRL_X__R;
          if (repeat) {
            strcpy(text, text_repeat);
            strcat(text, " ");
          } else if (makro_recording) {
            strcpy(text, "[RM] ");
          } else {
            text[0] = '\0';
          }
          strcat(text, "C+X R -> {M,B}");
          show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
          continue;
        } else if (check_key1(KEY_T)) {
          group_key = GK_CTRL_X__T;
          if (repeat) {
            strcpy(text, text_repeat);
            strcat(text, " ");
          } else if (makro_recording) {
            strcpy(text, "[RM] ");
          } else {
            text[0] = '\0';
          }
          strcat(text, "C+X T -> {0,2,M,O,S-M,S-O}");
          show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
          continue;
        }
        // MACROS
        else if (check_key1(KEY_E)) {
          if (makro_recording) {
            makro_events_idx = 0;
            makro_recording = false;
          } else if (makro_events_idx) {
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
                write_event(evsyn);
                usleep(REPEAT_DELAY);
                for (unsigned short i = 0; i < makro_events_idx; ++i)
                  write_event(makro_events[i]);
              }
            }
          }
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key2(KEY_LEFTSHIFT, KEY_8)) {
          makro_events_idx = 0;
          makro_recording = true;
          group_key = GK_NULL;
          strcpy(text, "[RECORDING MACRO]");
          show_text_window(text, 0xffcccccc, 0xff4530ff, g_focused_window);
          continue;
        } else if (check_key2(KEY_LEFTSHIFT, KEY_9)) {
          if (makro_recording) {
            makro_recording = false;
            while (makro_events_idx && is_mod(makro_events[makro_events_idx - 1].code))
              --makro_events_idx;
            if (makro_events_idx && makro_events[makro_events_idx - 1].code == KEY_X)
              --makro_events_idx;
            while (makro_events_idx
                   && makro_events[makro_events_idx - 1].code == KEY_LEFTCTRL
                   && makro_events[makro_events_idx - 1].value == 1)
              --makro_events_idx;
            if (verbose) {
              fprintf(stderr, "RECORDED MACRO [\n");
              for (unsigned short i = 0; i < makro_events_idx; ++i)
                fprintf(stderr, "  %d, %s, %d\n",
                        makro_events[i].type,
                        key_names[makro_events[i].code],
                        makro_events[i].value);
              fprintf(stderr, "] RECORDED MACRO\n");
            }
          }
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_C:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_T)) {
          key0_remap_remap(KEY_T, KEY_F6, KEY_F6);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_A)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_B)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_D)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_E)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_N)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTCTRL, KEY_P)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1_digit(KEY_LEFTCTRL)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_X__R:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_M)) {
          key0_mod1_remap(KEY_M, KEY_LEFTCTRL, KEY_D);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_B)) {
          key0_mod1_mod1_remap(KEY_B, KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_O);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (!is_mod(event.code))
            continue;
        break;
      case GK_CTRL_X__T:
        if (check_key1(KEY_ESC) || check_key2(KEY_LEFTCTRL, KEY_G)) {
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
          continue;
        } else if (check_key1(KEY_0)) {
          key0_mod1_remap(KEY_0, KEY_LEFTCTRL, KEY_W);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_2)) {
          key0_mod1_remap(KEY_2, KEY_LEFTCTRL, KEY_T);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_O)) {
          key0_mod1_remap(KEY_O, KEY_LEFTCTRL, KEY_PAGEDOWN);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_O)) {
          mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_O, KEY_LEFTCTRL, KEY_PAGEUP);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key1(KEY_M)) {
          key0_mod1_mod1_remap(KEY_M, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_PAGEDOWN);
          group_key = GK_NULL;
          show_text_window(NULL, 0, 0, g_focused_window);
        } else if (check_key2(KEY_LEFTSHIFT, KEY_M)) {
          key0_mod1_mod1_remap(KEY_M, KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_PAGEUP);
          group_key = GK_NULL;
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
          write_event(evsyn);
          usleep(REPEAT_DELAY);
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
          write_event(evsyn);
          usleep(REPEAT_DELAY);
          fake_events[0].value = 0;
          write_event(fake_events[0]);
          fake_events[0].value = 1;
          write_event(fake_events[0]);
        }
        break;
      default:
        write_event(fake_events[0]);
        for (unsigned short i = 1; i < fake_events_setted; ++i) {
          write_event(evsyn);
          usleep(WRITE_DELAY);
          write_event(fake_events[i]);
        }
        while (--repeat) {
          write_event(evsyn);
          usleep(REPEAT_DELAY);
          write_event(fake_events[0]);
          for (unsigned short i = 1; i < fake_events_setted; ++i) {
            write_event(evsyn);
            usleep(WRITE_DELAY);
            write_event(fake_events[i]);
          }
        }
      }
      continue;
    case 2:
      if (group_key != GK_NULL || repeat)
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
      write_event(fake_events[0]);
      for (unsigned short i = 1; i < fake_events_setted; ++i) {
        write_event(evsyn);
        usleep(WRITE_DELAY);
        write_event(fake_events[i]);
      }
    }
  }
  if (g_text_window)
      xcb_destroy_window(g_conn, g_text_window);
  xcb_disconnect(g_conn);
  return 0;
}
