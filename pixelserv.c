/*
* pixelserv.c a small mod to public domain server.c -- a stream socket server demo
* from http://beej.us/guide/bgnet/
* single pixel http string from http://proxytunnel.sourceforge.net/pixelserv.php
*/

#include "util.h"
#include "socket_handler.h"

#include <sys/wait.h>   // waitpid()

#ifdef DROP_ROOT
# include <pwd.h>       // getpwnam()
#endif

#include <fcntl.h>      // fcntl() and related

#ifdef TEST
# include <arpa/inet.h> // inet_ntop()
#endif

void signal_handler(int sig)
{
  if (sig != SIGTERM
   && sig != SIGUSR1
#ifdef DEBUG
   && sig != SIGUSR2
#endif
  ) {
    syslog(LOG_WARNING, "Ignoring unsupported signal number: %d", sig);
    return;
  }
#ifdef DEBUG
  if (sig == SIGUSR2) {
    syslog(LOG_INFO, "Main process caught signal %d near line number %lu of file %s", sig, LINE_NUMBER, __FILE__);
  } else {
#endif
    if (sig == SIGTERM) {
      // Ignore this signal while we are quitting
      signal(SIGTERM, SIG_IGN);
    }

    // log stats
    char* stats_string = get_stats(0, 0);
    syslog(LOG_INFO, "%s", stats_string);
    free(stats_string);

    if (sig == SIGTERM) {
      // exit program on SIGTERM
      syslog(LOG_NOTICE, "exit on SIGTERM");
      exit(EXIT_SUCCESS);
    }
#ifdef DEBUG
  }
#endif
  return;
}

#ifdef TEST
// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(( (struct sockaddr_in*) sa )->sin_addr);
  }

  return &(( (struct sockaddr_in6*) sa )->sin6_addr);
}
#endif

int main (int argc, char* argv[]) // program start
{
  int sockfd = 0;  // listen on sock_fd
  int new_fd = 0;  // new connection on new_fd
  struct sockaddr_storage their_addr;  // connector's address information
  socklen_t sin_size;
  int yes = 1;
  char* version_string;
  time_t select_timeout = DEFAULT_TIMEOUT;
  int rv = 0;
  char* ip_addr = DEFAULT_IP;
  int use_ip = 0;
  struct addrinfo hints, *servinfo;
  int error = 0;
#ifdef TEST
  char ntop_buf[INET6_ADDRSTRLEN];
#endif
  int pipefd[2];  // IPC pipe ends (0 = read, 1 = write)
  response_struct pipedata = { FAIL_GENERAL, 0 };
  char* ports[MAX_PORTS];
  ports[0] = DEFAULT_PORT;
  ports[1] = SECOND_PORT;
  char *port = NULL;
  fd_set readfds;
  fd_set selectfds;
  int sockfds[MAX_PORTS];
  int select_rv = 0;
  int nfds = 0;
  int num_ports = 0;
  int i;
#ifdef IF_MODE
  char *ifname = "";
  int use_if = 0;
#endif
#ifdef DROP_ROOT
  char *user = DEFAULT_USER;  // used to be long enough
  struct passwd *pw = 0;
#endif
  char* stats_url = DEFAULT_STATS_URL;
  char* stats_text_url = DEFAULT_STATS_TEXT_URL;
  int do_204 = 1;
#ifndef TEST
  int do_foreground = 0;
#endif // !TEST
  int do_redirect = 1;
#ifdef DEBUG
  int warning_time = 0;
#endif //DEBUG
  char ssl_alert = DEFAULT_SSL_ALERT;

  SET_LINE_NUMBER(__LINE__);

  // command line arguments processing
  for (i = 1; i < argc && error == 0; ++i) {
    if (argv[i][0] == '-') {
      // handle arguments that don't require a subsequent argument
      switch (argv[i][1]) {
        case '2': do_204 = 0;                                 continue;
#ifndef TEST
        case 'f': do_foreground = 1;                          continue;
#endif // !TEST
        case 'r':
          // deprecated - ignore
//          do_redirect = 1;
        continue;
        case 'R': do_redirect = 0;                            continue;
        // no default here because we want to move on to the next section
      }
      // handle arguments that require a subsequent argument
      if ((i + 1) < argc) {
        // switch on parameter letter and process subsequent argument
        switch (argv[i++][1]) {
          case 'c':
            ssl_alert = (char)strtol(argv[i], NULL, 10);
          continue;
#ifdef IF_MODE
          case 'n':
            ifname = argv[i];
            use_if = 1;
          continue;
#endif
          case 'o':
            errno = 0;
            select_timeout = strtol(argv[i], NULL, 10);
            if (errno || select_timeout <= 0) {
              error = 1;
            }
          continue;
          case 'p':
            if (num_ports < MAX_PORTS) {
              ports[num_ports++] = argv[i];
            } else {
              error = 1;
            }
          continue;
          case 's': stats_url = argv[i];                      continue;
          case 't': stats_text_url = argv[i];                 continue;
#ifdef DROP_ROOT
          case 'u': user = argv[i];                           continue;
#endif
#ifdef DEBUG
          case 'w':
            errno = 0;
            warning_time = strtol(argv[i], NULL, 10);
            if (errno || warning_time <= 0) {
              error = 1;
            }
          continue;
#endif //DEBUG
          default:  error = 1;                                continue;
        }
      } else {
        error = 1;
      }
    } else if (use_ip == 0) {  // assume its a listening IP address
      ip_addr = argv[i];
      use_ip = 1;
    } else {
      error = 1;  // fix bug with 2 IP like args
    } // -
  } // for

  SET_LINE_NUMBER(__LINE__);

  if (error) {
    printf("Usage:%s"
           " [IP No/hostname (all)]"
           " [-2 (disables HTTP 204 reply to generate_204 URLs)]"
           " [-c ssl_alert_code (49 - access denied)]"
#ifndef TEST
           " [-f (stay in foreground - don't daemonize)]"
#endif // !TEST
#ifdef IF_MODE
           " [-n i/f (all)]"
#endif // IF_MODE
           " [-o select_timeout (%d seconds)]"
           " [-p port ("
           DEFAULT_PORT
           ") & ("
           SECOND_PORT
           ")]"
           " [-r (deprecated - ignored)]"
           " [-R (disables redirect to encoded path in tracker links)]"
           " [-s /relative_stats_html_URL ("
           DEFAULT_STATS_URL
           ")"
           " [-t /relative_stats_txt_URL ("
           DEFAULT_STATS_TEXT_URL
           ")"
#ifdef DROP_ROOT
           " [-u user (\"nobody\")]"
#endif // DROP_ROOT
#ifdef DEBUG
           " [-w warning_time (warn when elapsed connection time exceeds value in msec)"
#endif //DEBUG
           "\n", argv[0], DEFAULT_TIMEOUT);
    exit(EXIT_FAILURE);
  }

  SET_LINE_NUMBER(__LINE__);

  openlog("pixelserv", LOG_PID | LOG_CONS | LOG_PERROR, LOG_DAEMON);
  version_string = get_version(argc, argv);
  if (version_string) {
    syslog(LOG_INFO, "%s", version_string);
    free(version_string);
  } else {
    exit(EXIT_FAILURE);
  }

  SET_LINE_NUMBER(__LINE__);

#ifndef TEST
  if (!do_foreground && daemon(0, 0)) {
    syslog(LOG_ERR, "failed to daemonize, exit: %m");
    exit(EXIT_FAILURE);
  }
#endif

  SET_LINE_NUMBER(__LINE__);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;  // AF_UNSPEC - AF_INET restricts to IPV4
  hints.ai_socktype = SOCK_STREAM;
  if (!use_ip) {
    hints.ai_flags = AI_PASSIVE;  // use my IP
  }

  // if no ports selected on command line, use the defaults
  if (!num_ports) {
    num_ports = 2;
  }

  // clear the set
  FD_ZERO(&readfds);

  SET_LINE_NUMBER(__LINE__);

  for (i = 0; i < num_ports; i++) {
    port = ports[i];

    rv = getaddrinfo(use_ip ? ip_addr : NULL, port, &hints, &servinfo);
    if (rv) {
      syslog( LOG_ERR, "getaddrinfo: %s", gai_strerror(rv) );
      exit(EXIT_FAILURE);
    }

    if ( ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 1)
      || (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))
      || (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &yes, sizeof(int)))  // send short packets straight away
#ifdef IF_MODE
      || (use_if && (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname))))  // only use selected i/f
#endif
      || (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))
      || (listen(sockfd, BACKLOG))
      || (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK))  // set non-blocking mode
       ) {
#ifdef IF_MODE
      syslog(LOG_ERR, "Abort: %m - %s:%s:%s", ifname, ip_addr, port);
#else
      syslog(LOG_ERR, "Abort: %m - %s:%s", ip_addr, port);
#endif
      exit(EXIT_FAILURE);
    }

    SET_LINE_NUMBER(__LINE__);

    sockfds[i] = sockfd;
    // add descriptor to the set
    FD_SET(sockfd, &readfds);
    if (sockfd > nfds) {
      nfds = sockfd;
    }

    freeaddrinfo(servinfo); // all done with this structure
#ifdef IF_MODE
    syslog(LOG_NOTICE, "Listening on %s:%s:%s", ifname, ip_addr, port);
#else
    syslog(LOG_NOTICE, "Listening on %s:%s", ip_addr, port);
#endif
  }

  SET_LINE_NUMBER(__LINE__);

  // set up signal handling
  {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    // set signal handler for termination
    if (sigaction(SIGTERM, &sa, NULL)) {
      syslog(LOG_ERR, "SIGTERM %m");
      exit(EXIT_FAILURE);
    }

    // attempt to set SIGCHLD to ignore
    // in K26 this should cause children to be automatically reaped on exit
    // in K24 it will accomplish nothing, so we still need to use waitpid()
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
      syslog(LOG_WARNING, "SIGCHLD %m");
    }

    // set signal handler for info
    sa.sa_flags = SA_RESTART; // prevent EINTR from interrupted library calls
    if (sigaction(SIGUSR1, &sa, NULL)) {
      syslog(LOG_ERR, "SIGUSR1 %m");
      exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    // set signal handler for debug
    sa.sa_flags = SA_RESTART; // prevent EINTR from interrupted library calls
    if (sigaction(SIGUSR2, &sa, NULL)) {
      syslog(LOG_ERR, "SIGUSR2 %m");
      exit(EXIT_FAILURE);
    }
#endif
  }

  SET_LINE_NUMBER(__LINE__);

#ifdef DROP_ROOT // no longer fatal error if doesn't work
  if ( (pw = getpwnam(user)) == NULL ) {
    syslog(LOG_WARNING, "Unknown user \"%s\"", user);
  }
  else if ( setuid(pw->pw_uid) ) {
    syslog(LOG_WARNING, "setuid %d: %m", pw->pw_uid);
  }
#endif

  SET_LINE_NUMBER(__LINE__);

  // cause failed pipe I/O calls to result in error return values instead of
  //  SIGPIPE signals
  signal(SIGPIPE, SIG_IGN);

  SET_LINE_NUMBER(__LINE__);

  // open pipe for children to use for writing data back to main
  if (pipe(pipefd) == -1) {
    syslog(LOG_ERR, "pipe() error: %m");
    exit(EXIT_FAILURE);
  }
  // set non-blocking read mode
  // note that writes are left as blocking because otherwise weird things happen
  if (fcntl(pipefd[0], F_SETFL, fcntl(pipefd[0], F_GETFL) | O_NONBLOCK) == -1) {
    syslog(LOG_ERR, "fcntl() error setting O_NONBLOCK on read end of pipe: %m");
    exit(EXIT_FAILURE);
  }

  SET_LINE_NUMBER(__LINE__);

  // FYI, this shows 4096 on mips K26
  MYLOG(LOG_INFO, "opened pipe with buffer size %d bytes", PIPE_BUF);

  // also have select() monitor the read end of the stats pipe
  FD_SET(pipefd[0], &readfds);
  // note if pipe read descriptor is the largest fd number we care about
  if (pipefd[0] > nfds) {
    nfds = pipefd[0];
  }

  // nfds now contains the largest fd number of interest;
  //  increment by 1 for use with select()
  ++nfds;

  sin_size = sizeof their_addr;

  SET_LINE_NUMBER(__LINE__);

  // main accept() loop
  while(1) {

    SET_LINE_NUMBER(__LINE__);

    // only call select() if we have something more to process
    if (select_rv <= 0) {
      // select() modifies its fd set, so make a working copy
      // readfds should not be referenced after this point, as it must remain
      //  intact
      selectfds = readfds;
      // NOTE: MACRO needs "_GNU_SOURCE"; without this the select gets
      //       interrupted with errno EINTR
      select_rv = TEMP_FAILURE_RETRY(select(nfds, &selectfds, NULL, NULL, NULL));
      if (select_rv < 0) {
        syslog(LOG_ERR, "main select() error: %m");
        exit(EXIT_FAILURE);
      } else if (select_rv == 0) {
        // this should be pathological, as we don't specify a timeout
        syslog(LOG_WARNING, "main select() returned zero (timeout?)");
        continue;
      }
    }

    SET_LINE_NUMBER(__LINE__);

    // find first socket descriptor that is ready to read (if any)
    // note that even though multiple sockets may be ready, we only process one
    //  per loop iteration; subsequent ones will be handled on subsequent passes
    //  through the loop
    for (i = 0, sockfd = 0; i < num_ports; i++) {
      if ( FD_ISSET(sockfds[i], &selectfds) ) {
        // select sockfds[i] for servicing during this loop pass
        sockfd = sockfds[i];
        --select_rv;
        break;
      }
    }

    SET_LINE_NUMBER(__LINE__);

    // if select() didn't return due to a socket connection, check for pipe I/O
    if (!sockfd && FD_ISSET(pipefd[0], &selectfds)) {
      // perform a single read from pipe
      rv = read(pipefd[0], &pipedata, sizeof(pipedata));
      if (rv < 0) {
        syslog(LOG_WARNING, "error reading from pipe: %m");
      } else if (rv == 0) {
        syslog(LOG_WARNING, "pipe read() returned zero");
      } else if (rv != sizeof(pipedata)) {
        syslog(LOG_WARNING, "pipe read() got %d bytes, but %u bytes were expected - discarding", rv, sizeof(pipedata));
      } else {
        // process response type
        switch (pipedata.status) {
          case FAIL_GENERAL:   ++err; break;
          case FAIL_TIMEOUT:   ++tmo; break;
          case FAIL_CLOSED:    ++cls; break;
          case SEND_GIF:       ++gif; break;
          case SEND_TXT:       ++txt; break;
          case SEND_JPG:       ++jpg; break;
          case SEND_PNG:       ++png; break;
          case SEND_SWF:       ++swf; break;
          case SEND_ICO:       ++ico; break;
          case SEND_BAD:       ++bad; break;
          case SEND_SSL:       ++ssl; break;
          case SEND_STATS:     ++sta; break;
          case SEND_STATSTEXT: ++stt; break;
          case SEND_204:       ++noc; break;
          case SEND_REDIRECT:  ++rdr; break;
          case SEND_NO_EXT:    ++nfe; break;
          case SEND_UNK_EXT:   ++ufe; break;
          case SEND_NO_URL:    ++nou; break;
          case SEND_BAD_PATH:  ++pth; break;
          case SEND_POST:      ++pst; break;
          case SEND_HEAD:      ++hed; break;
          default:
            syslog(LOG_ERR, "Socket handler child process reported unknown response value: %d", pipedata.status);
        }

        SET_LINE_NUMBER(__LINE__);

        // count only positive receive sizes
        if (pipedata.rx_total <= 0) {
          MYLOG(LOG_WARNING, "pipe read() got nonsensical rx_total data value %d - ignoring", pipedata.rx_total);
        } else {
          // calculate as a double and add rounding factor for implicit integer
          //  truncation
          avg += ((double)(pipedata.rx_total - avg) / ++act) + 0.5;
          // look for a new high score
          if (pipedata.rx_total > rmx) {
            rmx = pipedata.rx_total;
          }
        }

        // update request time stats
        // calculate moving average, adding 0.5 for rounding
        tav += ((pipedata.run_time - tav) / ++tct) + 0.5;
        // look for a new high score, adding 0.5 for rounding
        if (pipedata.run_time + 0.5 > tmx) {
          tmx = (pipedata.run_time + 0.5);
        }
      }
      --select_rv;
      continue;
    }

    SET_LINE_NUMBER(__LINE__);

    // if select() returned but no fd's of interest were found, give up
    // note that this is bad because it means that select() will probably never
    //  block again because something will always be waiting on the unhandled
    //  file descriptor
    // on the other hand, this should be a pathological case unless something is
    //  added to FD_SET that is not checked before this point
    if (!sockfd) {
      syslog(LOG_WARNING, "select() returned a value of %d but no file descriptors of interest are ready for read", select_rv);
      // force select_rv to zero so that select() will be called on the next
      //  loop iteration
      select_rv = 0;
      continue;
    }

    new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
    if (new_fd < 0) {
      if (errno == EAGAIN
       || errno == EWOULDBLOCK) {
        // client closed connection before we got a chance to accept it
        MYLOG(LOG_INFO, "accept: %m");
        count++;
        cls++;
      } else {
        syslog(LOG_WARNING, "accept: %m");
      }
      continue;
    }

    count++;

    SET_LINE_NUMBER(__LINE__);

    if (fork() == 0) {
      // this is the child process
      //
      // detach child from signal handler
      signal(SIGTERM, SIG_DFL); // default is kill?
      signal(SIGUSR1, SIG_DFL); // default is ignore?
#ifdef DEBUG
      signal(SIGUSR2, SIG_DFL); // default is ignore?
#endif
      // close unneeded file handles inherited from the parent process
      close(sockfd);

      // note that only the read end is closed
      // even main() should leave the write end open so that children can
      //  inherit it
      close(pipefd[0]);

#ifdef TEST
      inet_ntop(their_addr.ss_family, get_in_addr( (struct sockaddr *) &their_addr ), ntop_buf, sizeof ntop_buf);
      printf("server: got connection from %s\n", ntop_buf);
#endif
      // call handler function
      socket_handler(argc
                    ,argv
                    ,new_fd
                    ,select_timeout
                    ,pipefd[1]
                    ,stats_url
                    ,stats_text_url
                    ,argv[0]
                    ,do_204
                    ,do_redirect
                    ,ssl_alert
#ifdef DEBUG
                    ,warning_time
#endif //DEBUG
                    );
      exit(0);
    } // end of forked child process

    SET_LINE_NUMBER(__LINE__);

    // this is guaranteed to be the parent process, as the child calls exit()
    //  above when it's done instead of proceeding to this point
    close(new_fd);  // parent doesn't need this

    SET_LINE_NUMBER(__LINE__);

    // reap any zombie child processes that have exited
    // irony note: I wrote this while watching The Walking Dead :p
    for (errno = 0; waitpid(-1, 0, WNOHANG) > 0 || (errno && errno != ECHILD); errno = 0) {
      if (errno && errno != ECHILD) {
        syslog(LOG_WARNING, "waitpid() reported error: %m");
      }
    }

    SET_LINE_NUMBER(__LINE__);
  } // end of perpetual accept() loop
//  Never get here while(1)
//  return (EXIT_SUCCESS);
}
