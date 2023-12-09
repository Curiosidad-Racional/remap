#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>

int main() {
  setbuf(stdout, NULL);
  struct input_event event = {.type = EV_KEY, .code = 1, .value = 0};
  fwrite(&event, sizeof(struct input_event), 1, stdout);

  return 0;
}
