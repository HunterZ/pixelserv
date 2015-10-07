pixelserv
=========

Tiny webserver that responds to all requests with "nothing".  Particularly useful for network connections with high latency and metered bandwidth.

This fork merges the mstombs and h0tw1r3 forks back together, and adds some additional cleanups and enhancements (most notably a stats reporting URL and finer-grained stats collection, as well as command-line configurable timeouts).

Releases
--------

Starting with HZ12, binary releases are available in the following flavors:
* x86 and x86-64:
  * PC versions (32- and 64-bit, respectively)
  * Built natively in Xubuntu LTS x86_64
  * Statically-linked versions are not provided because glibc has forced dynamic dependencies.
* mips:
  * MIPS K24 (for older routers such as WRT54G)
  * Built via Shibby brcm uclibc cross-compiler toolchain
  * The dynamically-linked versions should be highly-compatible with various K24 and K26 Linux router firmwares.
* tomatoware:
  * MIPS K26 (for newer routers such as RT-N16 and RT-N66U)
  * Built natively on RT-N66U via Tomatoware
  * The statically-linked versions use newer libraries from Tomatoware than are provided by most firmwares.

ARM builds are currently provided unofficially by the linksysinfo.org forum community.

Also, jerrm has been graciously providing recent releases of both Adblock and pixelserv here, including ARM builds: http://tomato-adblock.weebly.com/

Stats
-----

Stats are viewable by default at http://pixelservip/servstats.txt (for raw text format) or http://pixelservip/servstats for html format), where pixelserv ip is the ip address that pixelserv is listening on.

Explanation of stats:
* uts: uptime in seconds
* req: number of connection requests
* avg: average request size in bytes
* rmx: maximum request size in bytes
* tav: average request processing time in milliseconds
* tmx: maximum request processing time in milliseconds
* err: number of connections resulting in processing errors (syslog may have details)
* tmo: number of connections that timed out while trying to read a request from the client
* cls: number of connections that were closed by the client while reading or replying to the request
* nou: number of requests that failed to include a URL
* pth: number of requests for a path that could not be parsed
* nfe: number of requests for a file with no extension
* ufe: number of requests for an unrecognized/unhandled file extension
* gif: number of requests for GIF images
* bad: number of requests for unrecognized/unhandled HTTP methods
* txt: number of requests for plaintext data formats
* jpg: number of requests for JPEG images
* png: number of requests for PNG images
* swf: number of requests for Adobe Shockwave Flash files
* ico: number of requests for ICO files (usually favicons)
* ssl: number of SSL connection requests
* sta: number of requests for HTML stats
* stt: number of requests for plaintext stats
* 204: number of requests for /generate_204 URLs
* rdr: number of requests resulting in a redirect
* pst: number of requests for HTTP POST method
* hed: number of requests for HTTP HEAD method

Sources
-------

* https://github.com/flexiondotorg/nullserv (defunct)
* http://www.linksysinfo.org/index.php?threads/pixelserv-compiled-to-run-on-router-wrt54g.30509/page-3#post-229342
* http://www.dd-wrt.com/phpBB2/viewtopic.php?p=685201
* https://en.wikipedia.org/wiki/Transport_Layer_Security#Alert_protocol
