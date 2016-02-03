/*
 * Start a program in a pseudo terminal, and redirect its input from
 * stdin, and output to stdout.
 *
 * Originally by Rachid Koucha.  Enhanced by Lars Brinhoff.
 */

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#define __USE_BSD
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>


static struct termios old_termios;
static int fd_termios;

static void cleanup (void)
{
  tcsetattr (fd_termios, TCSANOW, &old_termios);
}

static void handler (int sig)
{
  exit(0);
}

static void fatal (const char *message, ...)
{
  va_list args;

  va_start(args, message);
  vfprintf(stderr, message, args);
  fputc('\n', stderr);
  va_end(args);

  exit(1);
}

static int open_master (void)
{
  int rc, fdm;

  fdm = posix_openpt(O_RDWR);
  if (fdm < 0)
    fatal("Error %d on posix_openpt()", errno);

  rc = grantpt(fdm);
  if (rc != 0)
    fatal("Error %d on grantpt()", errno);

  rc = unlockpt(fdm);
  if (rc != 0)
    fatal("Error %d on unlockpt()", errno);

  return fdm;
}

static void read_write (const char *name, int in, int out)
{
  char input[150];
  int rc;

  rc = read(in, input, sizeof input);
  if (rc < 0)
    {
      if (errno == EIO)
	exit (0);

      fatal("Error %d on read %s", errno, name);
    }

  write(out, input, rc);
}

static void terminal_settings(int fdm)
{
  struct termios new_termios;
  struct winsize ws;
  int rc;

  if (isatty(0))
    fd_termios = 0;
  else if (isatty(1))
    fd_termios = 1;
  else
    return;

  // Copy terminal size to the pty.
  ioctl(fd_termios, TIOCGWINSZ, &ws);
  ioctl(fdm, TIOCSWINSZ, &ws);

  // Save the defaults parameters
  rc = tcgetattr(fd_termios, &old_termios);
  if (rc == -1)
    fatal("Error %d on tcgetattr()", errno);

  // Restore terminal on exit
  atexit(cleanup);
  siginterrupt (SIGINT, 1);
  signal (SIGINT, handler);

  if (fd_termios == 0)
    {
      // Set RAW mode on stdin
      new_termios = old_termios;
      cfmakeraw (&new_termios);
      tcsetattr (0, TCSANOW, &new_termios);
    }
}

static void master (int fdm)
{
  fd_set fd_in;

  for (;;)
    {
      // Wait for data from standard input and master side of PTY
      FD_ZERO(&fd_in);
      FD_SET(0, &fd_in);
      FD_SET(fdm, &fd_in);

      if (select(fdm + 1, &fd_in, NULL, NULL, NULL) == -1)
	fatal("Error %d on select()", errno);

      // If data on standard input
      if (FD_ISSET(0, &fd_in))
	read_write("standard input", 0, fdm);

      // If data on master side of PTY
      if (FD_ISSET(fdm, &fd_in))
	read_write("master pty", fdm, 1);
    }
}

static void slave (int fds, char **av)
{
  int rc;

  // The slave side of the PTY becomes the standard input and outputs
  // of the child process
  close(0); // Close standard input (current terminal)
  close(1); // Close standard output (current terminal)
  close(2); // Close standard error (current terminal)

  // PTY becomes standard input (0)
  if (dup(fds) == -1)
    fatal("Error %d on dup()", errno);

  // PTY becomes standard output (1)
  if (dup(fds) == -1)
    fatal("Error %d on dup()", errno);

  // PTY becomes standard error (2)
  if (dup(fds) == -1)
    fatal("Error %d on dup()", errno);

  // Now the original file descriptor is useless
  close(fds);

  // Make the current process a new session leader
  setsid();

  // As the child is a session leader, set the controlling terminal to
  // be the slave side of the PTY (Mandatory for programs like the
  // shell to make them correctly manage their outputs)
  ioctl(0, TIOCSCTTY, 1);

  // Execution of the program
  rc = execvp(av[1], av + 1);
  if (rc == -1)
    fatal("Error %d on execvp()", errno);

}

int main(int ac, char *av[])
{
  int fdm, fds;

  // Check arguments
  if (ac <= 1)
    fatal("Usage: %s program_name [parameters]", av[0]);

  fdm = open_master();

  terminal_settings(fdm);

  // Open the slave side ot the PTY
  fds = open(ptsname(fdm), O_RDWR);

  // Create the child process
  if (fork())
    {
      // Close the slave side of the PTY
      close(fds);
      master(fdm);
    }
  else
    {
      // Close the master side of the PTY
      close(fdm);
      slave(fds, av);
    }

  return 0;
} // main
