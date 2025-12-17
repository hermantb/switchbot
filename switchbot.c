#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

/* https://github.com/OpenWonderLabs/python-host.git */
/* /var/lib/bluetooth/[ctrl mac]/<data> */
/* .cache/.bluetoothctl_history */

#define NRETRY  10
#define TIMEOUT 10

#define BLUETOOTHCTL "bluetoothctl"
#define POWER_OFF    "power off\n"
#define POWER_ON     "power on\n"
#define SCAN_LE      "scan le\n"
#define SCAN_OFF     "scan off\n"
#define SCAN_CLEAR   "scan.clear\n"
#define GATT_LIST_AT "gatt.list-attributes\n"
#define QUIT         "quit\n"

static jmp_buf timeout_env;

static void
timeout (int sig)
{
  siglongjmp (timeout_env, 1);
}

static void
delay (double sec)
{
  struct timeval del;

  del.tv_sec = (int) sec;
  del.tv_usec = (int) ((sec - del.tv_sec) * 1000000.0);
  select (0, NULL, NULL, NULL, &del);
}

int
main (int argc, char **argv)
{
  FILE *fp;
  char *cp;
  int value;
  char line[1100];
  char last_line[1000];
  static int i, count, nretry, debug, retval = 0;
  static char *cmd;
  static char *mac;
  static int p1[2];
  static int p2[2];
  static pid_t pid;

  debug = getenv ("DEBUG_SWITCHBOT") != NULL;
  putenv ("XDG_CACHE_HOME=/tmp"); /* save .bluetoothctl_history in /tmp */
  if (argc == 2 && (argv[1][0] == 's' || argv[1][0] == 'S')) {
    printf ("Press control-c to exit scan.\n");
    pipe (p1);
    close (0);
    dup (p1[0]);
    fp = popen (BLUETOOTHCTL, "r");
    setlinebuf (fp);
    write (p1[1], POWER_OFF, strlen (POWER_OFF));
    delay (0.5);
    write (p1[1], POWER_ON, strlen (POWER_ON));
    delay (0.5);
    write (p1[1], SCAN_OFF, strlen (SCAN_OFF));
    write (p1[1], SCAN_CLEAR, strlen (SCAN_CLEAR));
    write (p1[1], SCAN_LE, strlen (SCAN_LE));
    for (;;) {
      fgets (line, sizeof (line), fp);
      printf ("%s", line);
    }
  }
  if (argc < 3) {
    fprintf (stderr, "Usage:\n");
    fprintf (stderr, "  close mac(s)\n");
    fprintf (stderr, "  open mac(s)\n");
    fprintf (stderr, "  <number> mac(s)\n");
    fprintf (stderr, "  scan\n");
    exit (1);
  }
  nretry = NRETRY;
  cmd = argv[1];
  for (i = 2; i < argc; i++) {
    mac = argv[i];
    p1[0] = p1[1] = -1;
    p2[0] = p2[1] = -1;
    pid = -1;
    if (sigsetjmp (timeout_env, 1) != 0) {
      alarm (0);
      if (p1[0] >= 0)
        close (p1[0]);
      if (p1[1] >= 0)
        close (p1[1]);
      if (p2[0] >= 0)
        close (p2[0]);
      if (p2[1] >= 0)
        close (p2[1]);
      if (pid != -1)
        kill (pid, SIGKILL);
      while (waitpid (-1, &value, WNOHANG) >= 0);
      if (--nretry == 0)
        retval = 1;
      else
        i--;
      delay (0.1);
    }
    else {
      signal (SIGALRM, timeout);
      alarm (TIMEOUT);
      pipe (p1);
      pipe (p2);
      close (0);
      dup (p1[0]);
      pid = fork ();
      if (pid == 0) {
        close (1);
        dup (p2[1]);
        close (2);
        open ("/dev/null", O_WRONLY);
        execlp (BLUETOOTHCTL, BLUETOOTHCTL, NULL);
        exit (1);
      }
      else if (pid == -1) {
        fprintf (stderr, "Fork failed\n");
        exit (1);
      }
      fp = fdopen (p2[0], "r");
      setlinebuf (fp);
      write (p1[1], SCAN_OFF, strlen (SCAN_OFF));
      write (p1[1], POWER_ON, strlen (POWER_ON));
      delay (0.5);
      snprintf (line, sizeof (line), "connect %s\n", mac);
      write (p1[1], line, strlen (line));
      count = 1;
      for (;;) {
        fgets (line, sizeof (line), fp);
        if (debug)
          printf ("%s\n", line);
        if (strstr (line, "Connection successful"))
          break;
        if (strstr (line, "not available")) {
          if (count-- > 0) {
            write (p1[1], POWER_OFF, strlen (POWER_OFF));
            delay (0.5);
            write (p1[1], POWER_ON, strlen (POWER_ON));
            delay (0.5);
          }
          write (p1[1], SCAN_CLEAR, strlen (SCAN_CLEAR));
          write (p1[1], SCAN_LE, strlen (SCAN_LE));
          delay (3);
          write (p1[1], SCAN_OFF, strlen (SCAN_OFF));
          snprintf (line, sizeof (line), "connect %s\n", mac);
          write (p1[1], line, strlen (line));
        }
      }
      snprintf (line, sizeof (line), "trust %s\n", mac);
      write (p1[1], line, strlen (line));
      write (p1[1], GATT_LIST_AT, strlen (GATT_LIST_AT));
      for (;;) {
        memcpy (last_line, line, sizeof (last_line));
        last_line[sizeof (last_line) - 1] = 0;
        fgets (line, sizeof (line), fp);
        if (debug)
          printf ("%s\n", line);
        if (strstr (line, "cba20002-224d-11e6-9fb8-0002a5d5c51b"))
          break;
      }
      cp = last_line;
      while (*cp == ' ' || *cp == '\t')
        cp++;
      snprintf (line, sizeof (line), "gatt.select-attribute %s", cp);
      write (p1[1], line, strlen (line));
      if (cmd[0] == 'c' || cmd[0] == 'C')
        value = 100;
      else if (cmd[0] == 'o' || cmd[0] == 'O')
        value = 0;
      else {
        value = atoi (cmd);
        if (value < 0)
          value = 0;
        else if (value > 100)
          value = 100;
      }
      snprintf (line, sizeof (line),
                "gatt.write \"0x57 0x0F 0x45 0x01 0x05 0xFF 0x%02X\"\n",
                value);
      write (p1[1], line, strlen (line));
      delay (0.1);
      write (p1[1], QUIT, strlen (QUIT));
      delay (0.1);
      close (p1[0]);
      close (p1[1]);
      close (p2[0]);
      close (p2[1]);
      kill (pid, SIGKILL);
      while (waitpid (-1, &value, WNOHANG) >= 0);
      nretry = NRETRY;
    }
  }
  return retval;
}
