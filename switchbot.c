#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char **argv)
{
  char *cmd;
  char *mac; 
  char *cp; 
  int p[2];
  FILE *fp;
  char line[1000];
  char handle[1000];
  const char *id = "cba20002-224d-11e6-9fb8-0002a5d5c51b";
  const char *sd = "00000d00-0000-1000-8000-00805f9b34fb";

  if (argc == 2 && argv[1][0] == 's') {
    pipe(p);
    close(0);
    dup(p[0]);
    fp = popen("bluetoothctl", "r");
    setlinebuf(fp);
    write (p[1], "scan on\n", strlen("scan on\n"));
    for (;;) {
	fgets(line, sizeof(line), fp);
	if (strstr(line, sd))
	  printf ("%s", line);
    }
  }
  if (argc < 3) {
     fprintf (stderr, "Usage:\n");
     fprintf (stderr, "  close mac\n");
     fprintf (stderr, "  open mac\n");
     fprintf (stderr, "  scan\n");
     exit(1);
  }
  alarm(10);
  pipe(p);
  close(0);
  dup(p[0]);
  cmd = argv[1];
  mac = argv[2];
  snprintf (line, sizeof(line), "gatttool -b %s -t random -I", mac);
  fp = popen(line, "r");
  setlinebuf(fp);
  write (p[1], "connect\n", strlen("connect\n"));
  for (;;) {
    fgets(line, sizeof(line), fp);
    if (strstr(line, "Connection successful"))
      break;
  }
  write (p[1], "char-desc\n", strlen("char-desc\n"));
  for (;;) {
    fgets(line, sizeof(line), fp);
    if (strstr(line, id)) {
      cp = strstr(line, "handle:");
      sscanf(cp, "%*s %[^,],", handle);
      break;
    }
  }
  snprintf (line, sizeof(line), "char-write-cmd %s %s\n",
	    handle, cmd[0] == 'c' ? "570F450105FF64" : "570F450105FF00");
  write (p[1], line, strlen(line));
  alarm(0);
  sleep(1);
  write (p[1], "quit\n", strlen("quit\n"));
  close(p[0]);
  close(p[1]);
  fclose(fp);
}
