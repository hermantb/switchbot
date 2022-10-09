#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static jmp_buf timeout_env;

static void
timeout(int sig)
{
  siglongjmp(timeout_env, 1);
}

static void
delay(int sec)
{
  struct timeval del;

  del.tv_sec = sec;
  del.tv_usec = 0;
  select(0, NULL, NULL, NULL, &del);
}

int
main(int argc, char **argv)
{
  char *cp; 
  FILE *fp;
  char line[1100];
  char handle[1000];
  char sb_cmd[20];
  static const char *id = "cba20002-224d-11e6-9fb8-0002a5d5c51b";
  static const char *sd = "00000d00-0000-1000-8000-00805f9b34fb";
  static char *cmd;
  static char *mac;
  static int p1[2];
  static int p2[2];
  static pid_t pid;
  static int nretry = 3;

  if (argc == 2 && argv[1][0] == 's') {
    pipe(p1);
    close(0);
    dup(p1[0]);
    fp = popen("bluetoothctl", "r");
    setlinebuf(fp);
    write (p1[1], "scan on\n", strlen("scan on\n"));
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
     fprintf (stderr, "  <number> mac\n");
     fprintf (stderr, "  scan\n");
     exit(1);
  }
  cmd = argv[1];
  mac = argv[2];
  p1[0] = p1[1] = -1;
  p2[0] = p2[1] = -1;
  pid = -1;
  if (sigsetjmp(timeout_env, 1) != 0) {
     if (p1[0] >= 0)
	close(p1[0]);
     if (p1[1] >= 0)
	close(p1[1]);
     if (p2[0] >= 0)
	close(p2[0]);
     if (p2[1] >= 0)
	close(p2[1]);
     if (pid != -1)
        kill(pid, SIGKILL);
     if (--nretry == 0)
	return 1;
     delay(1);
  }
  signal(SIGALRM, timeout);
  alarm(10);
  pipe(p1);
  pipe(p2);
  close(0);
  dup(p1[0]);
  pid = fork();
  if (pid == 0) {
      close(1);
      dup(p2[1]);
      close(2);
      open ("/dev/null", O_WRONLY);
      execlp ("gatttool", "gatttool", "-b", mac, "-t", "random", "-I", NULL);
      exit(1);
  }
  else if (pid == -1) {
      fprintf (stderr, "Fork failed\n");
      exit(1);
  }
  fp = fdopen(p2[0], "r");
  setlinebuf(fp);
  write (p1[1], "connect\n", strlen("connect\n"));
  for (;;) {
    fgets(line, sizeof(line), fp);
    if (strstr(line, "Connection successful"))
      break;
  }
  write (p1[1], "char-desc\n", strlen("char-desc\n"));
  for (;;) {
    fgets(line, sizeof(line), fp);
    if (strstr(line, id)) {
      cp = strstr(line, "handle:");
      sscanf(cp, "%*s %[^,],", handle);
      break;
    }
  }
  snprintf (sb_cmd, sizeof(sb_cmd), "570F450105FF%02X", atoi(cmd));
  snprintf (line, sizeof(line), "char-write-cmd %s %s\n",
	    handle, cmd[0] == 'c' ? "570F450105FF64" :
		    cmd[0] == 'o' ? "570F450105FF00" :
		    sb_cmd);
  write (p1[1], line, strlen(line));
  delay(1);
  write (p1[1], "quit\n", strlen("quit\n"));
  delay(1);
  close(p1[0]);
  close(p1[1]);
  close(p2[0]);
  close(p2[1]);
  kill(pid, SIGKILL);
  return 0;
}
