#include <stdio.h>
#include <stdlib.h>
#include <X11/XKBlib.h>

/* Compile this with -lX11 */

int main ()
{
  Display *display;
  Status status;
  unsigned state;

  display = XOpenDisplay (getenv ("DISPLAY"));
  if (!display)
    return 1;

  if (XkbGetIndicatorState (display, XkbUseCoreKbd, &state) != Success)
    return 2;

  printf ("Caps Lock is %s\n", (state & 1) ? "on" : "off");
  XCloseDisplay(display);
  return 0;
}
