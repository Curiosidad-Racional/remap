#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xdo.h>

enum window_class { EMACS, OTHERS, QUTEBROWSER, TERM };
enum prefix { NONE, CTRL_X, CTRL_C } prefix = NONE;
xdo_t *xdo;

enum window_class check_window_classname() {
  Window focuswin = 0;
  if (xdo_get_focused_window_sane(xdo, &focuswin) == 0) {
    XClassHint classhint;
    XGetClassHint(xdo->xdpy, focuswin, &classhint);
    XFree(classhint.res_name);
    // fprintf(stderr, "%s\n", classhint.res_class);
    if (strcmp(classhint.res_class, "Emacs") == 0) {
      XFree(classhint.res_class);
      return EMACS;
    } else if (strcmp(classhint.res_class, "qutebrowser") == 0) {
      XFree(classhint.res_class);
      return QUTEBROWSER;
    } else if (strcmp(classhint.res_class, "Alacritty") == 0 ||
               strcmp(classhint.res_class, "URxvt") == 0 ||
               strcmp(classhint.res_class, "st-256color") == 0) {
      XFree(classhint.res_class);
      return TERM;
    } else {
      XFree(classhint.res_class);
      return OTHERS;
    }
  } else {
    return EMACS;
  }
}

unsigned short keyboard_total = 0;
__s32 keyboard[249] = {[0 ... 248] = 0};
__s32 fakboard[249] = {[0 ... 248] = 0};
unsigned short fake_events_setted = 0;
struct input_event fake_events[8] = {
    [0 ... 7] = {.type = EV_KEY, .code = 0, .value = 0}};

bool check_key1(const __u16 k1) {
  return keyboard[k1] && keyboard_total == keyboard[k1];
}

bool check_key2(const __u16 k1, const __u16 k2) {
  return keyboard[k1] && keyboard[k2] &&
         keyboard_total == keyboard[k1] + keyboard[k2];
}

bool check_key3(const __u16 k1, const __u16 k2, const __u16 k3) {
  return keyboard[k1] && keyboard[k2] && keyboard[k3] &&
         keyboard_total == keyboard[k1] + keyboard[k2] + keyboard[k3];
}

void write_event(const struct input_event event) {
  // fprintf(stderr, "%d, %d, %d\n", event.type, event.code, event.value);
  fwrite(&event, sizeof(event), 1, stdout);
  switch (event.value) {
  case 0:
    if (fakboard[event.code])
      fakboard[event.code] = 0;
    break;
  case 1:
    if (!fakboard[event.code])
      fakboard[event.code] = 1;
    if (prefix != NONE)
      prefix = NONE;
    break;
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

int main(void) {

  setbuf(stdin, NULL), setbuf(stdout, NULL);
  xdo = xdo_new(NULL);

  bool skip_remap = false;
  bool select_mode = false;
  struct input_event event;
  while (fread(&event, sizeof(event), 1, stdin) == 1) {
    if (event.type == EV_MSC && event.code == MSC_SCAN)
      continue;
    if (event.type != EV_KEY) {
      fwrite(&event, sizeof(event), 1, stdout);
      continue;
    }
    // fprintf(stderr, "%u, %u, %d\n", event.type, event.code, event.value);
    if (event.code == KEY_RIGHTALT)
      event.code = KEY_LEFTCTRL;
    else if (event.code == KEY_LEFTCTRL || event.code == KEY_RIGHTCTRL)
      event.code = KEY_RIGHTALT;

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
      switch (prefix) {
      case NONE:
        // CTRL MOVEMENT
        if (check_key2(KEY_LEFTCTRL, KEY_N)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_N, KEY_LEFTSHIFT,
                                   KEY_DOWN);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_N, KEY_DOWN);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_P)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_P, KEY_LEFTSHIFT, KEY_UP);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_P, KEY_UP);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_F, KEY_LEFTSHIFT,
                                   KEY_RIGHT);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_F, KEY_RIGHT);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_B)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_B, KEY_LEFTSHIFT,
                                   KEY_LEFT);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_B, KEY_LEFT);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_A)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_A, KEY_LEFTSHIFT,
                                   KEY_HOME);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_A, KEY_HOME);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_E)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_E, KEY_LEFTSHIFT, KEY_END);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_E, KEY_END);
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_V)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTCTRL, KEY_E, KEY_LEFTSHIFT,
                                   KEY_PAGEDOWN);
            else
              mod0_key0_remap(KEY_LEFTCTRL, KEY_E, KEY_PAGEDOWN);
            break;
          }
        }
        // ALT MOVEMENT
        else if (check_key2(KEY_LEFTALT, KEY_V)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTSHIFT,
                                   KEY_PAGEUP);
            else
              mod0_key0_remap(KEY_LEFTALT, KEY_F, KEY_PAGEUP);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_N)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_N, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_DOWN);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_N, KEY_LEFTCTRL, KEY_DOWN);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_P)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_P, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_UP);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_P, KEY_LEFTCTRL, KEY_UP);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_F)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_RIGHT);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_F, KEY_LEFTCTRL, KEY_RIGHT);
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_B)) {
          switch (check_window_classname()) {
          case OTHERS:
            if (select_mode)
              mod0_key0_mod1_mod1_remap(KEY_LEFTALT, KEY_B, KEY_LEFTCTRL,
                                        KEY_LEFTSHIFT, KEY_LEFT);
            else
              mod0_key0_mod1_remap(KEY_LEFTALT, KEY_B, KEY_LEFTCTRL, KEY_LEFT);
            break;
          }
        }
        // SELECT MODE
        else if (check_key2(KEY_LEFTCTRL, KEY_SPACE)) {
          switch (check_window_classname()) {
          case OTHERS:
            select_mode = !select_mode;
            break;
          }
        }
        // COPY-PASTE
        else if (check_key2(KEY_LEFTCTRL, KEY_Y)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_Y, KEY_V);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_W)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_W, KEY_X);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_W)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod0_key0_mod1_remap(KEY_LEFTALT, KEY_W, KEY_LEFTCTRL, KEY_C);
            if (select_mode)
              select_mode = false;
            break;
          }
        }
        // EDITION
        else if (check_key2(KEY_LEFTCTRL, KEY_D)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod0_key0_remap(KEY_LEFTCTRL, KEY_D, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_D)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod0_key0_mod1_remap(KEY_LEFTALT, KEY_D, KEY_LEFTCTRL, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_SLASH)) {
          // KEY_SLASH -> KEY_MINUS
          switch (check_window_classname()) {
          case OTHERS:
          case QUTEBROWSER:
            mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_SLASH, KEY_LEFTCTRL, KEY_Z);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTALT, KEY_LEFTSHIFT, KEY_SLASH)) {
          // KEY_SLASH -> KEY_MINUS
          switch (check_window_classname()) {
          case OTHERS:
            mod0_mod0_key0_mod1_remap(KEY_LEFTSHIFT, KEY_LEFTALT, KEY_SLASH,
                                      KEY_LEFTCTRL, KEY_Y);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_K)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod0_key0_mod1_remap_remap(KEY_LEFTCTRL, KEY_K, KEY_LEFTSHIFT,
                                       KEY_END, KEY_DELETE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key2(KEY_LEFTALT, KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case OTHERS:
            mod0_mod1_keyremap(KEY_LEFTALT, KEY_LEFTCTRL, KEY_BACKSPACE);
            if (select_mode)
              select_mode = false;
            break;
          }
        } else if (check_key3(KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_BACKSPACE)) {
          switch (check_window_classname()) {
          case OTHERS:
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
          case OTHERS:
            mod1_key0_remap(KEY_LEFTCTRL, KEY_S, KEY_F);
            break;
          }
        }
        // PREFIX
        else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          switch (check_window_classname()) {
          case OTHERS:
            prefix = CTRL_C;
            continue;
            break;
          }
        } else if (check_key2(KEY_LEFTCTRL, KEY_X)) {
          switch (check_window_classname()) {
          case OTHERS:
            prefix = CTRL_X;
            continue;
            break;
          }
        }
        break;
      case CTRL_X:
        if (check_key1(KEY_K)) {
          key0_mod1_remap(KEY_K, KEY_LEFTCTRL, KEY_W);
        } else if (check_key1(KEY_2)) {
          key0_mod1_remap(KEY_2, KEY_LEFTCTRL, KEY_T);
        } else if (check_key1(KEY_3)) {
          key0_mod1_remap(KEY_3, KEY_LEFTCTRL, KEY_T);
        } else if (check_key1(KEY_5)) {
          key0_mod1_remap(KEY_5, KEY_LEFTCTRL, KEY_N);
        } else if (check_key2(KEY_LEFTCTRL, KEY_F)) {
          mod1_key0_remap(KEY_LEFTCTRL, KEY_F, KEY_O);
        } else if (check_key2(KEY_LEFTCTRL, KEY_C)) {
          mod1_key0_mod1_remap(KEY_LEFTCTRL, KEY_C, KEY_LEFTSHIFT, KEY_W);
        }
        break;
      case CTRL_C:
        if (check_key1(KEY_T)) {
          key0_remap_remap(KEY_T, KEY_F6, KEY_F6);
        }
        break;
      }
      break;
    case 2:
      if (prefix != NONE)
        continue;
    }

    switch (fake_events_setted) {
    case 0:
      write_event(event);
      break;
    case 1:
      fake_events[0].value = event.value;
      write_event(fake_events[0]);
      break;
    default:
      for (unsigned short i = 0; i < fake_events_setted; ++i) {
        write_event(fake_events[i]);
      }
    }
  }
  xdo_free(xdo);
  return 0;
}
