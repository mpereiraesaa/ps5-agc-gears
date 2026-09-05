/* Host unit tests for the ps5log client library (no server required). */
#if defined(__linux__)
#define _DEFAULT_SOURCE 1
#endif

#include "ps5log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

#define CHECK_STR(actual, expected) do { \
    if (strcmp((actual), (expected)) != 0) { \
        fprintf(stderr, "FAIL %s:%d: got \"%s\" expected \"%s\"\n", \
                __FILE__, __LINE__, (actual), (expected)); \
        failures++; \
    } \
} while (0)

static void test_defaults(void) {
    ps5log_config cfg;
    ps5log_config_defaults(&cfg);
    CHECK(cfg.server[0] == 0);
    CHECK(cfg.port == PS5LOG_DEFAULT_PORT);
    CHECK(cfg.enabled == 1);
    CHECK(cfg.udp == 0);
    CHECK(cfg.connect_timeout_ms == 500);
    CHECK(cfg.send_timeout_ms == 200);
    CHECK(cfg.tag[0] == 0);
}

static void test_parse_config(void) {
    static const char text[] =
        "# dev.conf example\r\n"
        "\n"
        "  DEV_SERVER = 8.8.8.8  \n"
        "DEV_PORT=9301\n"
        "DEV_UDP=1\n"
        "DEV_CONNECT_TIMEOUT_MS=250\n"
        "DEV_SEND_TIMEOUT_MS=50\n"
        "DEV_TAG=host-test\n"
        "UNKNOWN_KEY=whatever\n"
        "DEV_ENABLED=0";  /* no trailing newline on purpose */
    ps5log_config cfg;
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(text, sizeof(text) - 1, &cfg) == 7);
    CHECK_STR(cfg.server, "8.8.8.8");
    CHECK(cfg.port == 9301);
    CHECK(cfg.udp == 1);
    CHECK(cfg.connect_timeout_ms == 250);
    CHECK(cfg.send_timeout_ms == 50);
    CHECK_STR(cfg.tag, "host-test");
    CHECK(cfg.enabled == 0);
}

static void test_parse_config_rejects_bad_values(void) {
    ps5log_config cfg;
    static const char bad_port[] = "DEV_PORT=70000\n";
    static const char bad_bool[] = "DEV_UDP=yes\n";
    static const char no_equals[] = "DEV_SERVER\n";
    static const char long_server[] =
        "DEV_SERVER=0123456789012345678901234567890123456789012345678901234567890123456789\n";
    static const char empty[] = "\n\n# only comments\n";
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(bad_port, sizeof(bad_port) - 1, &cfg) == -1);
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(bad_bool, sizeof(bad_bool) - 1, &cfg) == -1);
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(no_equals, sizeof(no_equals) - 1, &cfg) == -1);
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(long_server, sizeof(long_server) - 1, &cfg) == -1);
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_parse_config(empty, sizeof(empty) - 1, &cfg) == 0);
    CHECK(cfg.port == PS5LOG_DEFAULT_PORT);
}

static void test_load_config_from_files(void) {
    char missing[] = "/nonexistent/ps5log-test/dev.conf";
    char path[] = "/tmp/ps5log-test-conf-XXXXXX";
    int fd = mkstemp(path);
    const char *paths[2];
    const char *used = NULL;
    ps5log_config cfg;
    static const char text[] = "DEV_SERVER=8.8.4.4\nDEV_PORT=1234\n";
    CHECK(fd >= 0);
    CHECK(write(fd, text, sizeof(text) - 1) == (ssize_t)(sizeof(text) - 1));
    close(fd);
    paths[0] = missing;
    paths[1] = path;
    ps5log_config_defaults(&cfg);
    CHECK(ps5log_load_config(paths, 2, &cfg, &used) == 0);
    CHECK(used == path);
    CHECK_STR(cfg.server, "8.8.4.4");
    CHECK(cfg.port == 1234);

    ps5log_config_defaults(&cfg);
    CHECK(ps5log_load_config(paths, 1, &cfg, &used) == 1);
    CHECK(used == NULL);
    CHECK(cfg.server[0] == 0);
    unlink(path);
}

static void test_format_record(void) {
    char out[128];
    size_t n = ps5log_format_record(out, sizeof(out), 42, 123456789012ull,
                                    "MARK", "fence\tdone\nnext line\r");
    CHECK_STR(out, "42\t123456789012\tMARK\tfence\tdone next line \n");
    CHECK(n == strlen(out));
    n = ps5log_format_record(out, sizeof(out), 1, 2, "", "x");
    CHECK_STR(out, "1\t2\tINFO\tx\n");
    n = ps5log_format_record(out, sizeof(out), 1, 2, "bad level\twith tab", "x");
    CHECK_STR(out, "1\t2\tbad_level_with_tab\tx\n");
    n = ps5log_format_record(out, sizeof(out), 0, 0, NULL, NULL);
    CHECK_STR(out, "0\t0\tINFO\t\n");
    (void)n;
}

static void test_format_record_truncates_with_newline(void) {
    char out[24];
    char big[512];
    size_t n;
    memset(big, 'z', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    n = ps5log_format_record(out, sizeof(out), 999999, 888888, "INFO", big);
    CHECK(n == sizeof(out) - 1);
    CHECK(out[n] == 0);
    CHECK(out[n - 1] == '\n');
    /* 24-byte buffer: 22 content bytes (19-byte prefix + "zzz") + "\n" + NUL. */
    CHECK(strncmp(out, "999999\t888888\tINFO\tzzz", 22) == 0);
    CHECK(out[22] == '\n');

    n = ps5log_format_record(out, 1, 1, 1, "INFO", "x");
    CHECK(n == 0 && out[0] == 0);
    n = ps5log_format_record(out, 2, 1, 1, "INFO", "x");
    CHECK(n == 1 && out[0] == '\n' && out[1] == 0);
}

static void test_format_hello(void) {
    char out[256];
    ps5log_format_hello(out, sizeof(out), "PPSA99998", "agc gears", 0x1a2bull,
                        "", 0, 0);
    CHECK_STR(out, "HELLO ps5log/1 title=PPSA99998 app=agc_gears boot=0x1a2b\n");
    ps5log_format_hello(out, sizeof(out), NULL, "", 0, "night", 1, 77);
    CHECK_STR(out, "HELLO ps5log/1 title=unknown app=unknown boot=0x0 tag=night "
                   "resume=1 next_seq=77\n");
}

static uint64_t elapsed_ms(const struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)(now.tv_sec - start->tv_sec) * 1000ull +
           (uint64_t)(now.tv_nsec - start->tv_nsec) / 1000000ull;
}

static void test_disabled_config_never_opens_socket(void) {
    ps5log_config cfg;
    ps5log_stats stats;
    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "127.0.0.1");
    cfg.enabled = 0;
    CHECK(ps5log_init(&cfg, "T", "A", 1) == 1);
    CHECK(!ps5log_enabled());
    ps5log_stats_get(&stats);
    CHECK(stats.disabled_reason != NULL);
    CHECK_STR(stats.disabled_reason, "DEV_ENABLED=0");
    CHECK(ps5log_line(PS5LOG_INFO, "dropped") == -1);
    ps5log_stats_get(&stats);
    CHECK(stats.lines_dropped == 1);
    CHECK(stats.lines_sent == 0);
    ps5log_close("test");
}

static void test_unreachable_server_falls_back_to_mirror(void) {
    ps5log_config cfg;
    ps5log_stats stats;
    struct timespec start;
    char path[] = "/tmp/ps5log-test-mirror-XXXXXX";
    int mirror = mkstemp(path);
    char buffer[256];
    ssize_t got;
    CHECK(mirror >= 0);

    ps5log_config_defaults(&cfg);
    /* 127.0.0.1 with a port nobody listens on: immediate ECONNREFUSED. */
    strcpy(cfg.server, "127.0.0.1");
    cfg.port = 1;
    cfg.connect_timeout_ms = 300;
    clock_gettime(CLOCK_MONOTONIC, &start);
    CHECK(ps5log_init(&cfg, "PPSA00000", "mirror-test", 0xabc) == 1);
    CHECK(elapsed_ms(&start) < 2000);
    CHECK(!ps5log_enabled());
    ps5log_stats_get(&stats);
    CHECK(stats.disabled_reason != NULL);
    CHECK(stats.last_errno == ECONNREFUSED || stats.last_errno == ETIMEDOUT);

    ps5log_set_mirror_fd(mirror);
    CHECK(ps5log_line(PS5LOG_MARK, "only mirrored") == 1);
    CHECK(ps5log_printf(PS5LOG_INFO, "frame=%d", 7) == 1);
    CHECK(ps5log_hex64(PS5LOG_INFO, "fence", 0x1100) == 1);
    ps5log_stats_get(&stats);
    CHECK(stats.lines_mirrored == 3);
    CHECK(stats.lines_dropped == 3);
    CHECK(ps5log_next_seq() == 4);
    ps5log_set_mirror_fd(-1);
    ps5log_close("test");

    lseek(mirror, 0, SEEK_SET);
    got = read(mirror, buffer, sizeof(buffer) - 1);
    CHECK(got > 0);
    buffer[got > 0 ? got : 0] = 0;
    CHECK(strncmp(buffer, "1\t", 2) == 0);
    CHECK(strstr(buffer, "\tMARK\tonly mirrored\n") != NULL);
    CHECK(strstr(buffer, "\tINFO\tframe=7\n") != NULL);
    CHECK(strstr(buffer, "\tINFO\tfence=0x1100\n") != NULL);
    close(mirror);
    unlink(path);
}

static void test_bad_server_address(void) {
    ps5log_config cfg;
    ps5log_stats stats;
    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "dev-pc.local");
    CHECK(ps5log_init(&cfg, "T", "A", 1) == 1);
    ps5log_stats_get(&stats);
    CHECK(stats.disabled_reason != NULL);
    CHECK(strstr(stats.disabled_reason, "dotted IPv4") != NULL);
    CHECK(ps5log_reconnect() == 1);
    ps5log_close("test");
}

/* Loopback listener so capture can be tested without the Python server. */
static int listen_loopback(uint16_t *port_out) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(fd, 4) == 0);
    CHECK(getsockname(fd, (struct sockaddr *)&addr, &len) == 0);
    *port_out = ntohs(addr.sin_port);
    return fd;
}

static ssize_t recv_some(int fd, char *buffer, size_t cap) {
    struct pollfd pfd;
    size_t total = 0;
    pfd.fd = fd;
    pfd.events = POLLIN;
    while (total + 1 < cap && poll(&pfd, 1, 300) > 0) {
        ssize_t n = recv(fd, buffer + total, cap - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
    }
    buffer[total] = 0;
    return (ssize_t)total;
}

static void test_stdio_capture_roundtrip(void) {
    ps5log_config cfg;
    ps5log_stats stats;
    uint16_t port = 0;
    int listener = listen_loopback(&port);
    int peer;
    int real_stdout = dup(STDOUT_FILENO);
    char buffer[2048];

    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "127.0.0.1");
    cfg.port = port;
    CHECK(ps5log_init(&cfg, "PPSA77777", "capture", 0x77) == 0);
    peer = accept(listener, NULL, NULL);
    CHECK(peer >= 0);

    CHECK(ps5log_capture_stdio(PS5LOG_CAPTURE_STDIO | PS5LOG_CAPTURE_IGNORE_SIGPIPE) == 0);
    ps5log_stats_get(&stats);
    CHECK((stats.capture_flags & PS5LOG_CAPTURE_STDIO) == PS5LOG_CAPTURE_STDIO);
    printf("captured stdout %d\n", 1);           /* line-buffered: flushed by \n */
    fprintf(stderr, "captured stderr %d\n", 2);  /* unbuffered */
    CHECK(ps5log_line(PS5LOG_MARK, "structured after capture") == 0);
    ps5log_close("capture-test");                /* restores fd 1 and fd 2 */
    ps5log_stats_get(&stats);
    CHECK(stats.capture_flags == 0);

    CHECK(recv_some(peer, buffer, sizeof(buffer)) > 0);
    CHECK(strncmp(buffer, "HELLO ps5log/1 title=PPSA77777 app=capture boot=0x77\n", 53) == 0);
    {
        /* This test runs before any other stdout use in the binary, so line
         * buffering must be in effect: the printf line has to arrive BEFORE
         * the structured record sent right after it, not at close(). */
        const char *out_line = strstr(buffer, "captured stdout 1\n");
        const char *err_line = strstr(buffer, "captured stderr 2\n");
        const char *mark = strstr(buffer, "\tMARK\tstructured after capture\n");
        const char *bye = strstr(buffer, "BYE seq=1 reason=capture-test\n");
        CHECK(out_line != NULL && err_line != NULL && mark != NULL && bye != NULL);
        CHECK(out_line && mark && out_line < mark);
        CHECK(err_line && mark && err_line < mark);
        CHECK(mark && bye && mark < bye);
    }
    close(peer);

    /* fd 1 must be back on the original target: a write must not reach any
     * socket and the saved duplicate must still be the same file. */
    {
        struct stat a, b;
        CHECK(fstat(STDOUT_FILENO, &a) == 0 && fstat(real_stdout, &b) == 0);
        CHECK(a.st_dev == b.st_dev && a.st_ino == b.st_ino);
    }
    close(real_stdout);
    close(listener);

    /* Capture on a disabled channel changes nothing. */
    CHECK(ps5log_capture_stdio(PS5LOG_CAPTURE_STDOUT) == 1);
    ps5log_stats_get(&stats);
    CHECK(stats.capture_flags == 0);
}

static void test_capture_released_when_peer_vanishes(void) {
    ps5log_config cfg;
    uint16_t port = 0;
    int listener = listen_loopback(&port);
    int peer;
    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "127.0.0.1");
    cfg.port = port;
    cfg.send_timeout_ms = 100;
    CHECK(ps5log_init(&cfg, "PPSA77777", "vanish", 0x1) == 0);
    peer = accept(listener, NULL, NULL);
    CHECK(peer >= 0);
    CHECK(ps5log_capture_stdio(PS5LOG_CAPTURE_STDOUT | PS5LOG_CAPTURE_IGNORE_SIGPIPE) == 0);
    close(peer);
    close(listener);
    /* The first failed structured send disables the channel and must restore
     * fd 1 so later printf() calls cannot hit a dead socket. */
    {
        int rc = 0;
        int attempts;
        for (attempts = 0; attempts < 5 && ps5log_enabled(); ++attempts)
            rc = ps5log_line(PS5LOG_INFO, "into the void");
        CHECK(rc == -1);
        CHECK(!ps5log_enabled());
    }
    {
        ps5log_stats stats;
        ps5log_stats_get(&stats);
        CHECK(stats.capture_flags == 0);
        CHECK(stats.disabled_reason != NULL);
    }
    ps5log_close("test");
}

static void test_raw_fragments_share_the_stream(void) {
    ps5log_config cfg;
    uint16_t port = 0;
    int listener = listen_loopback(&port);
    int peer;
    char buffer[1024];
    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "127.0.0.1");
    cfg.port = port;
    CHECK(ps5log_init(&cfg, "PPSA99998", "raw", 0x5) == 0);
    peer = accept(listener, NULL, NULL);
    CHECK(peer >= 0);
    /* The AGC lab's log_text/log_hex64 emit one line as several fragments. */
    CHECK(ps5log_raw("LOG_BOOT_MONOTONIC_NS=", 22) == 0);
    CHECK(ps5log_raw("0x1a2b", 6) == 0);
    CHECK(ps5log_raw("\n", 1) == 0);
    CHECK(ps5log_raw("", 0) == 0);
    CHECK(ps5log_line(PS5LOG_MARK, "after raw") == 0);
    ps5log_close("raw-test");
    CHECK(recv_some(peer, buffer, sizeof(buffer)) > 0);
    CHECK(strstr(buffer, "\nLOG_BOOT_MONOTONIC_NS=0x1a2b\n1\t") != NULL);
    CHECK(strstr(buffer, "\tMARK\tafter raw\n") != NULL);
    close(peer);
    close(listener);
    CHECK(ps5log_raw("x", 1) == -1);   /* channel closed */
}

static void test_ipv4_parser_edge_cases(void) {
    ps5log_config cfg;
    ps5log_stats stats;
    const char *bad[] = {"256.1.1.1", "1.2.3", "1.2.3.4.5", "1..2.3", "01234.1.1.1",
                         "1.2.3.4 ", " 1.2.3.4", "a.b.c.d", "1.2.3.-4"};
    size_t i;
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        ps5log_config_defaults(&cfg);
        strcpy(cfg.server, bad[i]);
        CHECK(ps5log_init(&cfg, "T", "A", 1) == 1);
        ps5log_stats_get(&stats);
        CHECK(stats.disabled_reason && strstr(stats.disabled_reason, "dotted IPv4") != NULL);
    }
    /* A syntactically valid address must get past the parser (and then fail
     * to connect, which is a different reason). */
    ps5log_config_defaults(&cfg);
    strcpy(cfg.server, "127.0.0.1");
    cfg.port = 1;
    CHECK(ps5log_init(&cfg, "T", "A", 1) == 1);
    ps5log_stats_get(&stats);
    CHECK(stats.disabled_reason && strstr(stats.disabled_reason, "dotted IPv4") == NULL);
    ps5log_close("test");
}

static void test_monotonic_clock(void) {
    uint64_t a = ps5log_monotonic_ns();
    uint64_t b = ps5log_monotonic_ns();
    CHECK(a != 0);
    CHECK(b >= a);
}

int main(void) {
    /* Capture test first: it must observe an unused stdout (see ps5log.h). */
    test_stdio_capture_roundtrip();
    test_capture_released_when_peer_vanishes();
    test_defaults();
    test_parse_config();
    test_parse_config_rejects_bad_values();
    test_load_config_from_files();
    test_format_record();
    test_format_record_truncates_with_newline();
    test_format_hello();
    test_disabled_config_never_opens_socket();
    test_unreachable_server_falls_back_to_mirror();
    test_bad_server_address();
    test_raw_fragments_share_the_stream();
    test_ipv4_parser_edge_cases();
    test_monotonic_clock();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("ps5log host tests passed\n");
    return 0;
}
