#include "util.h"
#include "socket_handler.h"

#include <ctype.h> // isprint(), isdigit(), tolower()

// private data for socket_handler() use

  // HTTP 204 No Content for Google generate_204 URLs
  static const char http204[] =
  "HTTP/1.1 204 No Content\r\n"
  "Content-Length: 0\r\n"
  "Content-Type: text/html; charset=UTF-8\r\n"
//"Date: Sun, 21 Sep 2014 17:42:25 GMT\r\n"
//"Server: GFE/2.0\r\n"
//"X-Firefox-Spdy: 3.1\r\n"
  "\r\n";

  // HTML stats response pieces
  static const char httpstats1[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/html\r\n"
  "Content-length: ";
  // total content length goes between these two strings
  static const char httpstats2[] =
  "\r\n"
  "Connection: close\r\n"
  "\r\n";
  // split here because we care about the length of what follows
  static const char httpstats3[] =
  "<!DOCTYPE html><html><head><title>pixelserv statistics</title></head><body>";
  // stats text goes between these two strings
  static const char httpstats4[] =
  "</body></html>\r\n";

  // note: the -2 is to avoid counting the last line ending characters
  static const unsigned int statsbaselen = sizeof httpstats3 + sizeof httpstats4 - 2;

  // TXT stats response pieces
  static const char txtstats1[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/plain\r\n"
  "Content-length: ";
  // total content length goes between these two strings
  static const char txtstats2[] =
  "\r\n"
  "Connection: close\r\n"
  "\r\n";
  // split here because we care about the length of what follows
  static const char txtstats3[] =
  "\r\n";

  static const char httpredirect[] =
  "HTTP/1.1 307 Temporary Redirect\r\n"
  "Location: %s\r\n"
  "Content-type: text/plain\r\n"
  "Content-length: 0\r\n"
  "Connection: close\r\n\r\n";

  static const char httpnullpixel[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/gif\r\n"
  "Content-length: 42\r\n"
  "Connection: close\r\n"
  "\r\n"
  "GIF89a" // header
  "\1\0\1\0"  // little endian width, height
  "\x80"    // Global Colour Table flag
  "\0"    // background colour
  "\0"    // default pixel aspect ratio
  "\1\1\1"  // RGB
  "\0\0\0"  // RBG black
  "!\xf9"  // Graphical Control Extension
  "\4"  // 4 byte GCD data follow
  "\1"  // there is transparent background color
  "\0\0"  // delay for animation
  "\0"  // transparent colour
  "\0"  // end of GCE block
  ","  // image descriptor
  "\0\0\0\0"  // NW corner
  "\1\0\1\0"  // height * width
  "\0"  // no local color table
  "\2"  // start of image LZW size
  "\1"  // 1 byte of LZW encoded image data
  "D"    // image data
  "\0"  // end of image data
  ";";  // GIF file terminator

  static const char httpnulltext[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/html\r\n"
  "Content-length: 0\r\n"
  "Connection: close\r\n"
  "\r\n";

  static const char http501[] =
  "HTTP/1.1 501 Method Not Implemented\r\n"
  "Connection: close\r\n"
  "\r\n";

  static const char httpnull_png[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/png\r\n"
  "Content-length: 67\r\n"
  "Connection: close\r\n"
  "\r\n"
  "\x89"
  "PNG"
  "\r\n"
  "\x1a\n"  // EOF
  "\0\0\0\x0d" // 13 bytes length
  "IHDR"
  "\0\0\0\1\0\0\0\1"  // width x height
  "\x08"  // bit depth
  "\x06"  // Truecolour with alpha
  "\0\0\0"  // compression, filter, interlace
  "\x1f\x15\xc4\x89"  // CRC
  "\0\0\0\x0a"  // 10 bytes length
  "IDAT"
  "\x78\x9c\x63\0\1\0\0\5\0\1"
  "\x0d\x0a\x2d\xb4"  // CRC
  "\0\0\0\0"  // 0 length
  "IEND"
  "\xae\x42\x60\x82";  // CRC

  static const char httpnull_jpg[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/jpeg\r\n"
  "Content-length: 159\r\n"
  "Connection: close\r\n"
  "\r\n"
  "\xff\xd8"  // SOI, Start Of Image
  "\xff\xe0"  // APP0
  "\x00\x10"  // length of section 16
  "JFIF\0"
  "\x01\x01"  // version 1.1
  "\x01"      // pixel per inch
  "\x00\x48"  // horizontal density 72
  "\x00\x48"  // vertical density 72
  "\x00\x00"  // size of thumbnail 0 x 0
  "\xff\xdb"  // DQT
  "\x00\x43"  // length of section 3+64
  "\x00"      // 0 QT 8 bit
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xff\xff\xff\xff\xff\xff\xff"
  "\xff\xc0"  // SOF
  "\x00\x0b"  // length 11
  "\x08\x00\x01\x00\x01\x01\x01\x11\x00"
  "\xff\xc4"  // DHT Define Huffman Table
  "\x00\x14"  // length 20
  "\x00\x01"  // DC table 1
  "\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x03"
  "\xff\xc4"  // DHT
  "\x00\x14"  // length 20
  "\x10\x01"  // AC table 1
  "\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00"
  "\xff\xda"  // SOS, Start of Scan
  "\x00\x08"  // length 8
  "\x01"    // 1 component
  "\x01\x00"
  "\x00\x3f\x00"  // Ss 0, Se 63, AhAl 0
  "\x37" // image
  "\xff\xd9";  // EOI, End Of image

static const char httpnull_swf[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: application/x-shockwave-flash\r\n"
  "Content-length: 25\r\n"
  "Connection: close\r\n"
  "\r\n"
  "FWS"
  "\x05"  // File version
  "\x19\x00\x00\x00"  // litle endian size 16+9=25
  "\x30\x0A\x00\xA0"  // Frame size 1 x 1
  "\x00\x01"  // frame rate 1 fps
  "\x01\x00"  // 1 frame
  "\x43\x02"  // tag type is 9 = SetBackgroundColor block 3 bytes long
  "\x00\x00\x00"  // black
  "\x40\x00"  // tag type 1 = show frame
  "\x00\x00";  // tag type 0 - end file

static const char httpnull_ico[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/x-icon\r\n"
  "Cache-Control: max-age=2592000\r\n"
  "Content-length: 70\r\n"
  "Connection: close\r\n"
  "\r\n"
  "\x00\x00" // reserved 0
  "\x01\x00" // ico
  "\x01\x00" // 1 image
  "\x01\x01\x00" // 1 x 1 x >8bpp colour
  "\x00" // reserved 0
  "\x01\x00" // 1 colour plane
  "\x20\x00" // 32 bits per pixel
  "\x30\x00\x00\x00" // size 48 bytes
  "\x16\x00\x00\x00" // start of image 22 bytes in
  "\x28\x00\x00\x00" // size of DIB header 40 bytes
  "\x01\x00\x00\x00" // width
  "\x02\x00\x00\x00" // height
  "\x01\x00" // colour planes
  "\x20\x00" // bits per pixel
  "\x00\x00\x00\x00" // no compression
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00" // end of header
  "\x00\x00\x00\x00" // Colour table
  "\x00\x00\x00\x00" // XOR B G R
  "\x80\xF8\x9C\x41"; // AND ?

// private functions for socket_handler() use
#ifdef HEX_DUMP
// from http://sws.dett.de/mini/hexdump-c/
static void hex_dump(void *data, int size)
{
  /* dumps size bytes of *data to stdout. Looks like:
   * [0000] 75 6E 6B 6E 6F 77 6E 20   30 FF 00 00 00 00 39 00 unknown 0.....9.
   * (in a single line of course)
   */

  char *p = data;
  char c;
  int n;
  char bytestr[4] = {0};
  char addrstr[10] = {0};
  char hexstr[16*3 + 5] = {0};
  char charstr[16*1 + 5] = {0};
  for (n = 1; n <= size; n++) {
    if (n%16 == 1) {
      // store address for this line
      snprintf(addrstr, sizeof addrstr, "%.4x",
         ((unsigned int)p-(unsigned int)data) );
    }

    c = *p;
    if (isprint(c) == 0) {
      c = '.';
    }

    // store hex str (for left side)
    snprintf(bytestr, sizeof bytestr, "%02X ", *p);
    strncat(hexstr, bytestr, sizeof hexstr - strlen(hexstr) - 1);

    // store char str (for right side)
    snprintf(bytestr, sizeof bytestr, "%c", c);
    strncat(charstr, bytestr, sizeof charstr - strlen(charstr) - 1);

    if (n%16 == 0) {
      // line completed
      printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
      hexstr[0] = 0;
      charstr[0] = 0;
    } else if (n%8 == 0) {
      // half line: add whitespaces
      strncat(hexstr, "  ", sizeof hexstr - strlen(hexstr) - 1);
      strncat(charstr, " ", sizeof charstr - strlen(charstr) - 1);
    }

    p++; // next byte
  }

  if (strlen(hexstr) > 0) {
    // print rest of buffer if not empty
    printf("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
  }
}
#endif // HEX_DUMP

// redirect utility functions
char* strstr_last(const char* const str1, const char* const str2) {
  char *strp;
  int len1, len2;
  len2 = strlen(str2);
  if (len2==0) {
    return (char *) str1;
  }
  len1 = strlen(str1);
  if (len1 - len2 <= 0) {
    return 0;
  }
  strp = (char *)(str1 + len1 - len2);
  while (strp != str1) {
    if (*strp == *str2 && strncmp(strp, str2, len2) == 0) {
      return strp;
    }
    strp--;
  }
  return 0;
}

char from_hex(const char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

void urldecode(char* const decoded, char* const encoded) {
  char* pstr = encoded;
  char* pbuf = decoded;

  while (*pstr) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    } else {
      *pbuf++ = *pstr;
    }
    pstr++;
  }
  *pbuf = '\0';
}

double elapsed_time_msec(const struct timespec start_time) {
  struct timespec current_time = {0, 0};
  struct timespec diff_time = {0, 0};

  if (!start_time.tv_sec &&
      !start_time.tv_nsec) {
    MYLOG(LOG_WARNING, "check_time(): returning because start_time not set");
    return -1.0;
  }

  get_time(&current_time);

  diff_time.tv_sec = difftime(current_time.tv_sec, start_time.tv_sec) + 0.5;
  diff_time.tv_nsec = current_time.tv_nsec - start_time.tv_nsec;
  if (diff_time.tv_nsec < 0) {
    // normalize nanoseconds
    diff_time.tv_sec  -= 1;
    diff_time.tv_nsec += 1000000000;
  }

  return diff_time.tv_sec * 1000 + ((double)diff_time.tv_nsec / 1000000);
}

#ifdef DEBUG
void child_signal_handler(int sig)
{
  if (sig != SIGTERM
   && sig != SIGUSR2) {
    syslog(LOG_WARNING, "Child process ignoring unsupported signal number: %d", sig);
    return;
  }

  if (sig == SIGTERM) {
    // Ignore this signal while we are quitting
    signal(SIGTERM, SIG_IGN);
  }

  syslog(LOG_INFO, "Child process caught signal %d near line number %lu of file %s", sig, LINE_NUMBER, __FILE__);

  if (sig == SIGTERM) {
    // exit program on SIGTERM
    syslog(LOG_NOTICE, "Child process exit on SIGTERM");
    exit(EXIT_SUCCESS);
  }

  return;
}

#define TIME_CHECK(x) {\
  if (do_warning) {\
    do_warning = 0;\
    time_msec = elapsed_time_msec(start_time);\
    if (time_msec > warning_time) {\
      syslog(LOG_WARNING, "Elapsed time %f msec exceeded warning_time=%d msec following operation: %s", time_msec, warning_time, x);\
    }\
  }\
}

#else
# define TIME_CHECK(x,y...)
#endif //DEBUG

void socket_handler(int argc
                   ,char* argv[]
                   ,const int new_fd
                   ,const time_t select_timeout
                   ,const int pipefd
                   ,const char* const stats_url
                   ,const char* const stats_text_url
                   ,const char* const program_name
                   ,const int do_204
                   ,const int do_redirect
                   ,const char ssl_alert
#ifdef DEBUG
                   ,const int warning_time
#endif //DEBUG
                   ) {
  // NOTES:
  // - from here on, all exit points should be counted or at least logged
  // - exit() should not be called from the child process
  response_struct pipedata = {FAIL_GENERAL, 0, 0.0};
  struct timeval timeout = {select_timeout, 0};
  int rv = 0;
  char buf[CHAR_BUF_SIZE + 1];
  char *bufptr = NULL;
  char *url = NULL;
  char* aspbuf = NULL;
  const char* response = httpnulltext;
  int rsize = sizeof httpnulltext - 1;
  char* version_string = NULL;
  char* stat_string = NULL;
  struct timespec start_time = {0, 0};
  const char SSL_no[] = {
    '\x15'        // Alert (21)
   ,'\x03','\x00' // Version 3.0
   ,'\x00','\x02' // length 02
   ,'\x02'        // fatal
   ,ssl_alert     // 0 close notify, 0x28 Handshake failure 40, 0x31 TLS access denied 49
   ,'\x00'        // string terminator (not part of actual response)
  };
#ifdef DEBUG
  double time_msec = 0.0;
  int do_warning = (warning_time > 0);

  SET_LINE_NUMBER(__LINE__);

  // set up signal handling
  {
    struct sigaction sa;
    sa.sa_handler = child_signal_handler;
    sigemptyset(&sa.sa_mask);

    // set signal handler for termination
    if (sigaction(SIGTERM, &sa, NULL)) {
      syslog(LOG_WARNING, "sigaction(SIGTERM) reported error: %m");
    }

    // set signal handler for info
    sa.sa_flags = SA_RESTART; // prevent EINTR from interrupted library calls
    if (sigaction(SIGUSR2, &sa, NULL)) {
      syslog(LOG_WARNING, "sigaction(SIGUSR2) reported error: %m");
    }
  }

  SET_LINE_NUMBER(__LINE__);
#endif

  // note the time
  get_time(&start_time);

  // the socket is connected, but we need to perform a check for incoming data
  // since we're using blocking checks, we first want to set a timeout
  if (setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval)) < 0) {
    syslog(LOG_WARNING, "setsockopt(timeout) reported error: %m");
  }

  // select() is used because we want to give up after a specified timeout
  //  period, in case the client is messing with us

  SET_LINE_NUMBER(__LINE__);

  // read some data from the socket to buf
  rv = recv(new_fd, buf, CHAR_BUF_SIZE, 0);
  if (rv < 0) {               // some kind of error
    if (errno == ECONNRESET) {
      MYLOG(LOG_WARNING, "recv() reported connection error: %m");
      pipedata.status = FAIL_CLOSED;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      MYLOG(LOG_WARNING, "recv() reported timeout error: %m");
      pipedata.status = FAIL_TIMEOUT;
    } else {
      syslog(LOG_ERR, "recv() reported error: %m");
      pipedata.status = FAIL_GENERAL;
    }
  } else if (rv == 0) {       // EOF
    MYLOG(LOG_ERR, "client closed connection without sending any data");
    pipedata.status = FAIL_CLOSED;
  } else {                    // got some data
    TIME_CHECK("initial recv()");
    buf[rv] = '\0';
    TESTPRINT("\nreceived %d bytes\n'%s'\n", rv, buf);

    pipedata.rx_total = rv;
#ifdef HEX_DUMP
    hex_dump(buf, rv);
#endif
    if (buf[0] == '\x16') {
      TESTPRINT("SSL handshake request received\n");
      pipedata.status = SEND_SSL;
      response = SSL_no;
      rsize = sizeof SSL_no - 1;
    } else {
      char *req = strtok_r(buf, "\r\n", &bufptr);
      char *method = strtok(req, " ");
      if (method == NULL) {
        syslog(LOG_ERR, "client did not specify method");
      } else {
        TESTPRINT("method: '%s'\n", method);
        if (strcmp(method, "GET")) {  //methods are case-sensitive
          // something other than GET - send 501 response
          if (!strcmp(method, "POST")) {
            // POST
            pipedata.status = SEND_POST;
          } else if (!strcmp(method, "HEAD")) {
            // HEAD (TODO: send header of what the actual response type would be?)
            pipedata.status = SEND_HEAD;
          } else {
            // something else, possibly even non-HTTP
            syslog(LOG_WARNING, "Sending HTTP 501 response for unknown HTTP method or non-SSL, non-HTTP request: %s", method);
            pipedata.status = SEND_BAD;
          }
          TESTPRINT("Sending 501 response\n");
          response = http501;
          rsize = sizeof http501 - 1;
        } else {
          // GET
          // ----------------------------------------------
          // send default from here, no matter what happens
          pipedata.status = DEFAULT_REPLY;
          // trim up to non path chars
          char *path = strtok(NULL, " ");//, " ?#;=");     // "?;#:*<>[]='\"\\,|!~()"
          if (path == NULL) {
            pipedata.status = SEND_NO_URL;
            syslog(LOG_ERR, "client did not specify URL for GET request");
          } else if (!strcmp(path, stats_url)) {
            pipedata.status = SEND_STATS;
            version_string = get_version(argc, argv);
            stat_string = get_stats(1, 0);
            rsize = asprintf(&aspbuf,
                             "%s%u%s%s%s<br>%s%s",
                             httpstats1,
                             statsbaselen + strlen(version_string) + 4 + strlen(stat_string),
                             httpstats2,
                             httpstats3,
                             version_string,
                             stat_string,
                             httpstats4);
            free(version_string);
            free(stat_string);
            response = aspbuf;
          } else if (!strcmp(path, stats_text_url)) {
            pipedata.status = SEND_STATSTEXT;
            version_string = get_version(argc, argv);
            stat_string = get_stats(0, 1);
            rsize = asprintf(&aspbuf,
                             "%s%u%s%s\n%s%s",
                             txtstats1,
                             strlen(version_string) + 1 + strlen(stat_string) + 2,
                             txtstats2,
                             version_string,
                             stat_string,
                             txtstats3);
            free(version_string);
            free(stat_string);
            response = aspbuf;
          } else if (do_204 && !strcasecmp(path, "/generate_204")) {
            pipedata.status = SEND_204;
            response = http204;
            rsize = sizeof http204 - 1;
          } else {
            // pick out encoded urls (usually advert redirects)
//                  if (do_redirect && strstr(path, "=http") && strchr(path, '%')) {
            if (do_redirect && strcasestr(path, "=http")) {
              char *decoded = malloc(strlen(path)+1);
              urldecode(decoded, path);

              SET_LINE_NUMBER(__LINE__);

              // double decode
              urldecode(path, decoded);

              SET_LINE_NUMBER(__LINE__);

              free(decoded);
              url = strstr_last(path, "http://");
              if (url == NULL) {
                url = strstr_last(path, "https://");
              }
              // WORKAROUND: google analytics block - request bomb on pages with conversion callbacks (see in chrome)
              if (url) {
                char *tok = NULL;

                SET_LINE_NUMBER(__LINE__);

                for (tok = strtok_r(NULL, "\r\n", &bufptr); tok; tok = strtok_r(NULL, "\r\n", &bufptr)) {
                  char *hkey = strtok(tok, ":");
                  char *hvalue = strtok(NULL, "\r\n");
                  if (strstr(hkey, "Referer") && strstr(hvalue, url)) {
                    url = NULL;
                    TESTPRINT("Not redirecting likely callback URL: %s:%s\n", hkey, hvalue);
                    break;
                  }
                }

                SET_LINE_NUMBER(__LINE__);

              }
            }
            if (do_redirect && url) {
              pipedata.status = SEND_REDIRECT;
              rsize = asprintf(&aspbuf, httpredirect, url);
              response = aspbuf;
              TESTPRINT("Sending redirect: %s\n", url);
              url = NULL;
            } else {
              char *file = strrchr(strtok(path, "?#;="), '/');
              if (file == NULL) {
                pipedata.status = SEND_BAD_PATH;
                syslog(LOG_ERR, "invalid file path %s", path);
              } else {
                TESTPRINT("file: '%s'\n", file);
                char *ext = strrchr(file, '.');
                if (ext == NULL) {
                  pipedata.status = SEND_NO_EXT;
                  MYLOG(LOG_ERR, "no file extension %s from path %s", file, path);
                } else {
                  TESTPRINT("ext: '%s'\n", ext);
                  if (!strcasecmp(ext, ".gif")) {
                    TESTPRINT("Sending gif response\n");
                    pipedata.status = SEND_GIF;
                    response = httpnullpixel;
                    rsize = sizeof httpnullpixel - 1;
                  } else if (!strcasecmp(ext, ".png")) {
                    TESTPRINT("Sending png response\n");
                    pipedata.status = SEND_PNG;
                    response = httpnull_png;
                    rsize = sizeof httpnull_png - 1;
                  } else if (!strncasecmp(ext, ".jp", 3)) {
                    TESTPRINT("Sending jpg response\n");
                    pipedata.status = SEND_JPG;
                    response = httpnull_jpg;
                    rsize = sizeof httpnull_jpg - 1;
                  } else if (!strcasecmp(ext, ".swf")) {
                    TESTPRINT("Sending swf response\n");
                    pipedata.status = SEND_SWF;
                    response = httpnull_swf;
                    rsize = sizeof httpnull_swf - 1;
                  } else if (!strcasecmp(ext, ".ico")) {
                    TESTPRINT("Sending ico response\n");
                    pipedata.status = SEND_ICO;
                    response = httpnull_ico;
                    rsize = sizeof httpnull_ico - 1;
                  } else if (!strncasecmp(ext, ".js", 3)) {  // .jsx ?
                    pipedata.status = SEND_TXT;
                    TESTPRINT("Sending txt response\n");
                    response = httpnulltext;
                    rsize = sizeof httpnulltext - 1;
                  } else {
                    TESTPRINT("Sending ufe response\n");
                    pipedata.status = SEND_UNK_EXT;
                    MYLOG(LOG_ERR, "unrecognized file extension %s from path %s", ext, path);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (pipedata.status != FAIL_TIMEOUT) {
    TIME_CHECK("response selection");
  }

  // done processing socket connection; now handle selected result action
  if (pipedata.status == FAIL_GENERAL) {
    // log general error status in case it wasn't caught above
    syslog(LOG_WARNING, "browser request processing completed with FAIL_GENERAL status");
  } else if (pipedata.status != FAIL_TIMEOUT && pipedata.status != FAIL_CLOSED) {
    SET_LINE_NUMBER(__LINE__);

    // only attempt to send response if we've chosen a valid response type
    //
    // send response
    // this is currently a blocking call, so zero should not be returned
    rv = send(new_fd, response, rsize, MSG_NOSIGNAL);
    if (rv < 0) { // check for error message, but don't bother checking that all bytes sent
      if (errno == EPIPE || errno == ECONNRESET) {
        // client closed socket sometime after initial check
        MYLOG(LOG_WARNING, "attempt to send response for status=%d resulted in send() error: %m", pipedata.status);
        pipedata.status = FAIL_CLOSED;
      } else {
        // some other error
        syslog(LOG_ERR, "attempt to send response for status=%d resulted in send() error: %m", pipedata.status);
        pipedata.status = FAIL_GENERAL;
      }
    } else if (rv != rsize) {
      syslog(LOG_WARNING, "send() reported only %d of %d bytes sent; status=%d", rv, rsize, pipedata.status);
    }
    // free memory allocated by asprintf() (if any)
    if (aspbuf) {
      free(aspbuf);
      aspbuf = NULL;
    }
  }
  // *** NOTE: pipedata.status should not be altered after this point ***

  TIME_CHECK("response send()");

  // signal the socket connection that we're done writing
  errno = 0;
  if (shutdown(new_fd, SHUT_WR) < 0) {
    if (errno == ENOTCONN) {
      MYLOG(LOG_WARNING, "shutdown(new_fd, SHUT_WR) reported error: %m");
    } else {
      syslog(LOG_WARNING, "shutdown(new_fd, SHUT_WR) reported error: %m");
    }
  } else if (pipedata.status != FAIL_TIMEOUT &&
             pipedata.status != FAIL_CLOSED) { // only check for additional data if we didn't detect a timeout or close initially
    TIME_CHECK("socket write shutdown()");
    SET_LINE_NUMBER(__LINE__);

    // socket may still be open for read, so read any data that is still waiting
    errno = 0;
    do {
      rv = recv(new_fd, buf, CHAR_BUF_SIZE, 0);
      if (rv > 0) {
        pipedata.rx_total += rv;
      }
    } while (rv > 0); // rv=0 means peer performed orderly shutdown
    if (rv < 0) {
      if (errno == ECONNRESET) {
        MYLOG(LOG_WARNING, "Final recv() reported connection error: %m");
      } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        MYLOG(LOG_WARNING, "Final recv() reported timeout error: %m");
      } else {
        syslog(LOG_WARNING, "Final recv() reported error: %m");
      }
    }

    TIME_CHECK("final recv() loop");
  }

  SET_LINE_NUMBER(__LINE__);

  // signal that we're done reading
  errno = 0;
  if (shutdown(new_fd, SHUT_RD) < 0) {
    if (errno == ENOTCONN) {
      MYLOG(LOG_WARNING, "shutdown(new_fd, SHUT_RD) reported error: %m");
    } else {
      syslog(LOG_WARNING, "shutdown(new_fd, SHUT_RD) reported error: %m");
    }
  }

  TIME_CHECK("socket read shutdown()")
  SET_LINE_NUMBER(__LINE__);

  // close the connection
  errno = 0;
  if (close(new_fd) < 0) {
    if (errno == ENOTCONN) {
      MYLOG(LOG_WARNING, "close(new_fd) reported error: %m");
    } else {
      syslog(LOG_WARNING, "close(new_fd) reported error: %m");
    }
  }

  TIME_CHECK("socket close()");

  // store time delta in milliseconds
  pipedata.run_time = elapsed_time_msec(start_time);

  SET_LINE_NUMBER(__LINE__);

  // write pipedata to pipe
  // note that the parent must not perform a blocking pipe read without checking
  //  for available data, or else it may deadlock when we don't write anything
  rv = write(pipefd, &pipedata, sizeof(pipedata));
  if (rv < 0) {
    syslog(LOG_WARNING, "write() to pipe reported error: %m");
  } else if (rv == 0) {
    syslog(LOG_WARNING, "write() reports no data written to pipe but no error");
  } else if (rv != sizeof(pipedata)) {
    syslog(LOG_WARNING, "write() reports writing only %d bytes of expected %u", rv, sizeof(pipedata));
  }

  TIME_CHECK("pipe write()");
  SET_LINE_NUMBER(__LINE__);

  // child no longer needs write pipe, so close descriptor
  // this is probably redundant since we are about to exit() anyway
  if (close(pipefd) < 0) {
    syslog(LOG_WARNING, "close(pipefd) reported error: %m");
  }

  TIME_CHECK("pipe close()");

  if (pipedata.status == FAIL_GENERAL) {
    // complain (possibly again) about general failure status, in case it wasn't
    //  caught previously
    syslog(LOG_WARNING, "connection handler exiting with FAIL_GENERAL status");
  }
}
