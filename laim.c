// laim (0.3.0): a lame mail server
//
// creator: moon flower fields
// website: coffin.ir
// telegram: @moonflowerfields

// very funny.
#define COMEDIAN false

#define SZ          128
#define EOF         -1
#define max_targets 16

#include <fcntl.h>
#include <stdint.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint64_t    u64;
typedef uint16_t    u16;
typedef uint8_t     u8;
typedef const char *cstr;
typedef u8          byte;

struct linux_dirent64 {
    u64  d_ino;
    u64  d_off;
    u16  d_reclen;
    u8   d_type;
    char d_name[];
};

#define UCHAR_MAX  (127 * 2 + 1)
#define ALIGN      (sizeof(size_t))
#define ONES       ((size_t) -1 / UCHAR_MAX)
#define HIGHS      (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

size_t lenstr(cstr s) {
    const char   *a = s;
    const size_t *w;
    for (; (uintptr_t) s % ALIGN; s++)
        if (!*s) return s - a;
    for (w = (const void *) s; !HASZERO(*w); w++);
    for (s = (const void *) w; *s; s++);
    return s - a;
}

const u64 zero = 0;

char out_buf[ 4096 ];
int  out_pos = 0;

void flush() {
    if (out_pos == 0) return; // a branch instead of a syscall? in this economy?!
    write(STDOUT_FILENO, out_buf, out_pos);
    out_pos = 0;
}

void memcopy(char *s, cstr d, u64 len) {
    for (u64 i = 0; i < len; i++) s[ i ] = d[ i ];
}

void writeb(cstr s, u64 len) {
    if (out_pos + len > (u64) sizeof(out_buf)) flush();
    if (len > (u64) sizeof(out_buf)) {
        write(STDOUT_FILENO, s, len);
        return;
    }
    memcopy(out_buf + out_pos, s, len);
    out_pos += len;
}

void writes(cstr s) { writeb(s, lenstr(s)); }

char in_buf[ 4096 ];
u64  in_pos = 0;
u64  in_len = 0;

int readn(char *buf, int n) {
    if (out_pos != 0) flush(); // a branch over a syscall? in this economy?!
    int avail = in_len - in_pos;
    if (avail > 0) {
        if (avail > n) avail = n;
        memcopy(buf, in_buf + in_pos, avail);
        in_pos += avail;
        return avail;
    }
    return read(STDIN_FILENO, buf, n);
}

int get_char() {
    if (in_pos == in_len) {
        in_len = readn(in_buf, sizeof(in_buf));
        in_pos = 0;
        if (in_len <= 0) return EOF;
    } else if (out_pos != 0)
        flush();
    int c = (unsigned char) in_buf[ in_pos++ ];
    if (c == '\r') return get_char();
    return c;
}

// burst into flames if uninitialized
char *glob_buf = NULL;
u64   glob_sz  = 0;

void sadd(cstr s) {
    char *prev = glob_buf;
    while (*s) *glob_buf++ = *s++;
    glob_sz += glob_buf - prev;
}

#define over __attribute__((overloadable)) void
over sit(char *p, cstr s) {
    glob_buf = p;
    glob_sz  = 0;
    sadd(s);
}
over sit(char *p, cstr s, cstr s2) {
    sit(p, s);
    sadd(s2);
}
over sit(char *p, cstr s, cstr s2, cstr s3) {
    sit(p, s);
    sadd(s2);
    sadd(s3);
}

void memzero(char *s, u64 len) {
    for (u64 i = 0; i < len; i++) s[ i ] = 0;
}

void nadd(u64 n) {
    if (!n) {
        *glob_buf++ = '0';
        glob_sz++;
        return;
    }
    char tmp[ 20 ] = { 0 };
    u8   i         = 20;
    while (n) {
        tmp[ --i ] = '0' + (n % 10);
        n /= 10;
    }
    u8 len = 20 - i;
    memcopy(glob_buf, tmp + i, len);
    glob_buf += len;
    glob_sz += len;
}
void nlod(char *p, u64 n) {
    glob_buf = p;
    glob_sz  = 0;
    nadd(n);
}

void print_u64(u64 n) {
    static char tmp[ 21 ] = { 0 };
    nlod(tmp, n);
    writeb(tmp, glob_sz);
}

char *str_u64(u64 n) {
    static char s[ 21 ];
    nlod(s, n);
    *glob_buf = 0;
    return s;
}

u64 scan_u64() {
    u64  n = 0;
    char c;
    while ((c = get_char()) != EOF) {
        if (c < '0' || c > '9') break;
        n = n * 10 + (c - '0');
    }
    return n;
}

u64 scan_u64_fd(int fd) {
    if (fd == STDIN_FILENO) return scan_u64(); // avoid buffer skips

    u64  n = 0;
    char c;
    while (read(fd, &c, 1) == 1) {
        if (c < '0' || c > '9') break;
        n = n * 10 + (c - '0');
    }
    return n;
}

long syscall(long number, ...);

u64 now() {
    struct timespec ts;
    syscall(228, 0, &ts);
    return ts.tv_sec;
}

u64 room_user_count(char *room_path) {
    static char members_path[ SZ ];
    sit(members_path, room_path, ".members/");
    *glob_buf = 0;

    int dir_fd = open(members_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return 0;

    static char buf[ 1024 ];
    u64         count = 0;
    int         n;
    while ((n = syscall(217, dir_fd, buf, sizeof(buf))) > 0) {
        for (int off = 0; off < n;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + off);
            if (d->d_name[ 0 ] != '.') {
                int fd = openat(dir_fd, d->d_name, O_RDONLY);
                if (fd >= 0) {
                    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
                        flock(fd, LOCK_UN);
                        unlinkat(dir_fd, d->d_name, 0);
                    } else {
                        count++;
                    }
                    close(fd);
                }
            }
            off += d->d_reclen;
        }
    }
    close(dir_fd);
    return count;
}

void list_users(int dir_fd, bool rooms) {
    static char buf[ 1024 ];
    u64         n;
    while ((n = syscall(217, dir_fd, buf, 1024)) > 0) {
        for (u64 off = 0; off < n;) {
            struct linux_dirent64 *d;
            d         = (struct linux_dirent64 *) (buf + off);
            cstr name = d->d_name;
            if (*name != '.' && rooms ^ (*name != '~')) {
                writeb(name, lenstr(name));
                writes(" ");
                if (rooms) {
                    static char rp[ SZ ];
                    sit(rp, "mail/", name, "/");
                    *glob_buf = 0;
                    print_u64(room_user_count(rp));
                } else {
                    char lastlogin[ SZ ] = { 0 };
                    sit(lastlogin, "mail/", name, "/.login");
                    int login_fd = open(lastlogin, O_RDONLY);
                    if (login_fd >= 0) {
                        u64 ts;
                        read(login_fd, &ts, 8);
                        print_u64(ts);
                        close(login_fd);
                    } else {
                        writes("0");
                    }
                }
                writes("\n");
            }
            off += d->d_reclen;
        }
    }
}

int  smake(char *name) { return open(name, O_RDWR | O_CREAT, 0600); }
void slock(int fd) { flock(fd, LOCK_EX); }
void sopen(int fd) { flock(fd, LOCK_UN); }

#define err(a)                                                                                                                 \
    ({                                                                                                                         \
        writes(a);                                                                                                             \
        continue;                                                                                                              \
    })

void printindex(int dir_fd, char *name, u64 after) {
    int mail_fd = openat(dir_fd, name, O_RDONLY);
    if (mail_fd < 0) return;
    flock(mail_fd, LOCK_SH);

    struct stat st;
    fstat(mail_fd, &st);
    if (st.st_size == 0) {
        close(mail_fd);
        return;
    }

    u64 ts     = scan_u64_fd(mail_fd);
    u64 expiry = scan_u64_fd(mail_fd);

    if (ts < after) {
        close(mail_fd);
        return;
    }

    static char header[ SZ ] = { 0 };
    sit(header, name, "\n");
    writeb(header, glob_sz);
    print_u64(ts);
    writes("\n");

    static char buf[ 256 ];
    int         r;
    while ((r = read(mail_fd, buf, sizeof(buf))) > 0) writeb(buf, r);

    writes("\n\neof\n");

    close(mail_fd);

    // expired goods.
    if (expiry != 0 && expiry < now()) unlinkat(dir_fd, name, 0);
}

void printmail(char path[ SZ ], u64 after) {
    mkdir(path, 0700);

    int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return;

    int cfd = openat(dir_fd, ".count", O_RDONLY);
    if (cfd < 0) {
        writes(";");
        close(dir_fd);
        return;
    }
    u64 total = 0;
    read(cfd, &total, sizeof(u64));
    close(cfd);

    for (u64 i = 0; i < total; i++) { printindex(dir_fd, str_u64(i), after); }
    close(dir_fd);
}

u64 create_index(int dirfd) {
    int cfd = openat(dirfd, ".count", O_RDWR | O_CREAT, 0600);
    if (cfd < 0) return -1;
    u64 count = 0;
    read(cfd, &count, sizeof(u64));
    count++;
    lseek(cfd, 0, SEEK_SET);
    if (write(cfd, &count, sizeof(u64)) != sizeof(u64)) {
        close(cfd);
        return -1;
    }
    ftruncate(cfd, sizeof(u64));
    close(cfd);
    return count - 1;
}

const char motd[] = "a laim server!\n";
const char docs[] = {
#embed "docs.txt"
    , 0
};
const char source[] = {
#embed "laim.c"
    , 0
};
const char makefile[] = {
#embed "Makefile"
    , 0
};

bool is_path_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
           || c == ' ' || c == '$' || c == ':';
}

int  field_sep  = -1;
bool field_more = false;

#define read_field(buf, err, fail_action, len_buf)                                                                             \
    ({                                                                                                                         \
        bool _term = false;                                                                                                    \
        field_more = false;                                                                                                    \
        for (u8 _i = 0; _i < 64; _i++) {                                                                                       \
            int _ch = get_char();                                                                                              \
            if (_ch == field_sep) {                                                                                            \
                (buf)[ _i ] = 0;                                                                                               \
                _term       = true;                                                                                            \
                field_more  = true;                                                                                            \
                if (len_buf != NULL) {                                                                                         \
                    u8 *ptr = len_buf;                                                                                         \
                    *ptr    = _i;                                                                                              \
                }                                                                                                              \
                break;                                                                                                         \
            }                                                                                                                  \
            if (_ch == '\n' || _ch == EOF) {                                                                                   \
                (buf)[ _i ] = 0;                                                                                               \
                _term       = true;                                                                                            \
                if (len_buf != NULL) {                                                                                         \
                    u8 *ptr = len_buf;                                                                                         \
                    *ptr    = _i;                                                                                              \
                }                                                                                                              \
                break;                                                                                                         \
            }                                                                                                                  \
            if (!is_path_char((char) _ch)) {                                                                                   \
                writes(err);                                                                                                   \
                fail_action;                                                                                                   \
            }                                                                                                                  \
            (buf)[ _i ] = (char) _ch;                                                                                          \
        }                                                                                                                      \
        if (!_term) {                                                                                                          \
            writes(err);                                                                                                       \
            fail_action;                                                                                                       \
        }                                                                                                                      \
    })

// timing safe
bool safe_cmp(cstr left, cstr right, u64 size) {
    bool nonmatch = false;
    bool ended    = false;
    for (u64 i = 0; i < size; i++) {
        nonmatch = ((*left != *right) && !ended) | nonmatch;
        ended    = (*left == 0 || *right == 0) | ended;
        left++, right++;
    }
    return !nonmatch;
}

void openwrite(char *filename, cstr content) {
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    write(fd, content, lenstr(content));
    close(fd);
}

char       *filename;
char       *count_filename;
int         member_fd         = -1;
bool        ephemeral         = false;
static char member_file[ SZ ] = { 0 };

void leave_room() {
    if (member_fd < 0) return;

    flock(member_fd, LOCK_UN);
    close(member_fd);
    member_fd = -1;
    unlink(member_file);
    if (ephemeral && room_user_count((char *) filename) == 0) {
        static char mp2[ SZ ];
        sit(mp2, filename, ".members/");
        *glob_buf = 0;
        rmdir(mp2);
        int cfd2   = open(count_filename, O_RDONLY);
        u64 total2 = 0;
        if (cfd2 >= 0) {
            read(cfd2, &total2, 8);
            close(cfd2);
        }
        for (u64 i = 0; i < total2; i++) {
            static char mp[ SZ ];
            sit(mp, filename, str_u64(i));
            *glob_buf = 0;
            unlink(mp);
        }
        unlink(count_filename);
        rmdir(filename);
    }
    ephemeral = false;
}

bool sendmail(char *user, u8 userlen, char *target, char *body, u64 body_len, u64 expiry) {
    char dest_path[ SZ ] = { 0 };
    sit(dest_path, "mail/", target, "/");
    mkdir(dest_path, 0700);

    int dir_fd = open(dest_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) {
        writes("err:dir\n");
        return false;
    }
    slock(dir_fd);

    u64 index = create_index(dir_fd);
    if (index == -1) {
        writes("!");
        sopen(dir_fd);
        close(dir_fd);
        return false;
    }
    sit(dest_path, "mail/", target, "/");
    nadd(index);

    int user_fd = smake(dest_path);
    if (user_fd < 0) {
        sopen(dir_fd);
        close(dir_fd);
        writes("err:smake\n");
        return false;
    }
    slock(user_fd);
    sopen(dir_fd);
    close(dir_fd);

    char *ts = str_u64(now());
    write(user_fd, ts, lenstr(ts));
    write(user_fd, "\n", 1);
    char *ex = str_u64(expiry);
    write(user_fd, ex, lenstr(ex));
    write(user_fd, "\n", 1);
    write(user_fd, user, userlen);
    write(user_fd, "\n", 1);
    write(user_fd, body, body_len);

    sopen(user_fd);
    close(user_fd);

    return true;
}

char *read_body(u64 *body_len_out) {
    static char body[ 65536 ];
    u64         body_len   = 0;
    char        buf[ 256 ] = { 0 };
    int         read_size;
    while ((read_size = readn(buf, sizeof(buf))) > 0) {
        if (body_len + read_size > sizeof(body)) {
            writes(".");
            *body_len_out = 0;
            return NULL;
        }
        memcopy(body + body_len, buf, read_size);
        body_len += read_size;
    }
    *body_len_out = body_len;
    return body;
}

// exits on any stdin
bool intersleep(u64 ms) {
    struct timeval tv = { .tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000 };
    fd_set         fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return r == 0;
}

int *__errno_location(void);
void counter(const char *path) {
    if (access(path, F_OK) == 0) return;
    int fd = open(path, O_WRONLY | O_CREAT, 0700);
    write(fd, &zero, 8);
    close(fd);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        bool blim = safe_cmp(argv[ 1 ], "--blim", 6);
        if (safe_cmp(argv[ 1 ], "--help", 6) || safe_cmp(argv[ 1 ], "-h", 2)) {
            writes(
                "laim: lame mail server\n"
                "created by: moon (coffin.ir)\n"
                "\n"
                "laim is a mail server that operates on STDIO.\n"
                "it is meant to be run with a networking program, such as socat, or busybox's tcpsvd.\n"
                "it can be served over SSL using socat or busybox's ssl_server, which pipes TLS to I/O.\n"
                "\n"
                "usage:\n"
                "    ");
            writes(argv[ 0 ]);
            writes(
                " --help\n"
                "        shows this message (non-zero exit)\n"
                "    ");
            writes(argv[ 0 ]);
            writes(
                " --open\n"
                "        extracts the program's source code and build files (non-zero exit)\n"
                "    ");
            writes(argv[ 0 ]);
            writes(
                " --boot <bind_addr> <bind_port> <ssl_joint_path>\n"
                "        re-runs laim, wrapped with ssl_server and tcpsvd, provided a busybox with those applets is "
                "installed.\n"
                "        this parameter has an equivalent, --blim, which only allows up to 16 connections from the same ip.\n"
                "\n");
            flush();
            return 1;
        } else if (safe_cmp(argv[ 1 ], "--open", 6)) {
            openwrite("laim.c", source);
            openwrite("docs.txt", docs);
            openwrite("Makefile", makefile);
            writes("done\n");
            flush();
            return 1;
        } else if (safe_cmp(argv[ 1 ], "--boot", 6) || blim) {
            if (argc != 5) {
                writes("too many / too little args, read --help\n");
                flush();
                return 1;
            }
            char  path[ 4096 ] = { 0 };
            u64   bye          = readlink("/proc/self/exe", path, sizeof(path) - 1);
            char *new_argv[]   = { "tcpsvd", "-v", argv[ 2 ], argv[ 3 ], "ssl_server", "-f", argv[ 4 ], path, NULL };
            char *new_argv_blim[]
                = { "tcpsvd", "-v", "-c", "2048", "-C", "16", argv[ 2 ], argv[ 3 ], "ssl_server", "-f", argv[ 4 ], path, NULL };
            execvp("busybox", blim ? new_argv_blim : new_argv);
            writes("failed to start. errno: ");
            print_u64((*__errno_location()));
            flush();
            return 1;
        } else {
            writes("unknown arg\n");
            flush();
            return 1;
        }
    }

    char user[ 65 ] = { 0 }, pass[ 65 ] = { 0 };
    u8   userlen = 0;
    writes("laim3\n");

    // manipulates:
    // users/    - user db, manually add users
    // mail/     - mails directory, each filename is a username
    mkdir("users", 0700);
    mkdir("mail", 0700);

laim_start:;
    memzero(user, 65);
    memzero(pass, 65);
    int action = get_char();
    if (action == 'M') {
        writes(motd);
        goto laim_start;
    } else if (action == 'L') {
        // username
        read_field(user, "4", goto laim_start, &userlen);
        // password
        read_field(pass, "5", goto laim_start, NULL);

        char user_path[ SZ ] = "users/";
        memcopy(user_path + 6, user, userlen);
        user_path[ 6 + userlen ] = 0;

        // check lastlogin before sleeping
        char last_login[ SZ ] = { 0 };
        sit(last_login, "mail/", user, "/.login");
        *glob_buf = 0;
        {
            int login_fd = open(last_login, O_RDONLY);
            u64 old_ts = 0, old_pid = 0;
            if (login_fd >= 0) {
                read(login_fd, &old_ts, 8);
                read(login_fd, &old_pid, 8);
                close(login_fd);
            }
            if (old_pid > 0) syscall(62, old_pid, 9); // kill old
            if (now() - old_ts < 5) sleep(1);         // fly swatter
        }

        if (access(user_path, F_OK) != 0) {
            writes("I");
            sleep(1);
            goto laim_start;
        }

        int fd = open(user_path, O_RDONLY);
        if (fd < 0) {
            writes("err:fopen\n");
            return 1;
        }
        char actual_pass[ 65 ]   = { 0 };
        int  pass_size           = read(fd, actual_pass, 64);
        actual_pass[ pass_size ] = 0;
        if (!safe_cmp(pass, actual_pass, 64)) {
            writes("I");
            sleep(2); // punish user for null
            goto laim_start;
        }
        memzero(actual_pass, 65);
        close(fd);
        writes("O");
        // only fallthrough
    } else if (action == EOF) {
        return 1;
    } else {
        writes("?");
        goto laim_start;
    }

    // -- authorized from here on out --

    { // write last login
        char last_login[ SZ ] = { 0 };
        sit(last_login, "mail/", user, "/.login");
        int login_fd = open(last_login, O_RDWR | O_CREAT, 0600);
        if (login_fd >= 0) {
            ftruncate(login_fd, 0);
            u64 ts  = now();
            u64 pid = syscall(39); // getpid
            write(login_fd, &ts, 8);
            write(login_fd, &pid, 8);
            close(login_fd);
        }
    }

    // remove password from memory
    memzero(pass, 65);

    // make sure user has a mail dir
    char user_filename[ SZ ] = { 0 };
    sit(user_filename, "mail/", user, "/");
    mkdir(user_filename, 0700);

    char user_count_filename[ SZ ] = { 0 };
    sit(user_count_filename, user_filename, ".count");
    counter(user_count_filename);

    static char room_path[ SZ ]       = { 0 };
    static char room_count_path[ SZ ] = { 0 };

    filename       = user_filename;
    count_filename = user_count_filename;

    int _c;
    while ((_c = get_char()) != EOF) {
        field_sep = -1;
        if (_c == '\n') continue;
        char c = _c;
        if (c == 'G') { // get all mail
            printmail(filename, 0);
            writes("O");
        } else if (c == 'F') { // get indexed mail
            int dir_fd = open(filename, O_RDONLY | O_DIRECTORY);
            if (dir_fd < 0) err("!");
            field_sep       = ',';
            field_more      = true;
            bool field_fail = false;
            while (field_more && !field_fail) {
                char idx[ SZ ];
                read_field(idx, "7", field_fail = true; goto index_mail_end, NULL);
                if (idx[ 0 ]) printindex(dir_fd, idx, 0);
            }
        index_mail_end:
            field_sep = -1;
            close(dir_fd);
            if (!field_fail) writes("O");
        } else if (c == 'W') { // whoami
            if (COMEDIAN) writes("you're... you!");
            else { writes(user); }
            writes("\n");
        } else if (c == 'E') { // count inbox size, includes deleted mails
            int count_fd = open(count_filename, O_RDONLY);
            if (count_fd < 0) err("!");
            u64 result = 0;
            u64 size   = read(count_fd, &result, 8);
            if (size < 8) {
                close(count_fd);
                err("!");
            }
            close(count_fd);
            print_u64(result);
            writes("\n");
        } else if (c == 'Z') {
            static char topic_filename[ SZ ] = { 0 };
            sit(topic_filename, filename, ".topic");
            int topic_fd = open(topic_filename, O_RDONLY);
            if (topic_fd < 0) err("!");

            char buf[ 256 ];
            int  r;
            while ((r = read(topic_fd, buf, sizeof(buf))) > 0) writeb(buf, r);
            close(topic_fd);
            writes("\n");
        } else if (c == 'B') {
            leave_room();

            char room[ 64 + 1 ];
            u8   len;
            read_field(room, "7", goto while_end, &len);
            if (len == 0) sit(room_path, "mail/~hub/");
            else { sit(room_path, "mail/~", room, "/"); }
            ephemeral = (len > 0 && room[ 0 ] == ':');
            *glob_buf = 0;
            mkdir(room_path, 0700);
            sit(room_count_path, room_path, ".count");
            *glob_buf = 0;
            int cfd   = open(room_count_path, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (cfd >= 0) {
                write(cfd, &zero, 8);
                close(cfd);
            }
            filename       = room_path;
            count_filename = room_count_path;

            static char members_path[ SZ ];
            sit(members_path, room_path, ".members/");
            *glob_buf = 0;
            mkdir(members_path, 0700);

            u64 pid = syscall(39); // getpid
            sit(member_file, members_path, str_u64(pid));
            *glob_buf = 0;

            member_fd = open(member_file, O_RDWR | O_CREAT, 0600);
            flock(member_fd, LOCK_EX);

            writes("O");
        } else if (c == 'Q') { // turn off broadcast mode
            leave_room();

            filename       = user_filename;
            count_filename = user_count_filename;
            writes("O");
        } else if (c == 'U' || c == 'R') { // unified listing
            int dir_fd = open(c == 'R' ? "mail/" : "users/", O_RDONLY | O_DIRECTORY);
            if (dir_fd < 0) {
                writes("err:dir\n");
                continue;
            }
            list_users(dir_fd, c == 'R');
            close(dir_fd);
            writes("O");
        } else if (c == 'T') { // get mail after X
            printmail(filename, scan_u64());
            writes("O");
        } else if (c == 'S') {
            char targets[ max_targets ][ SZ ];
            u8   target_count = 0;

            field_sep  = ',';
            field_more = true;
            while (field_more && target_count < max_targets) {
                read_field(targets[ target_count ], "7", goto while_end, NULL);
                if (targets[ target_count ][ 0 ]) target_count++;
            }
            field_sep = -1;

            u64   body_len;
            char *body = read_body(&body_len);
            if (!body) goto while_end; // limit

            for (u8 i = 0; i < target_count; i++) {
                if (sendmail(user, userlen, targets[ i ], body, body_len, 0)) continue;
                writes("!");
                goto while_end;
            }
            writes("O");
        } else if (c == 'H') { // perishable post
            char targets[ max_targets ][ SZ ];
            u8   target_count = 0;

            field_sep  = ',';
            field_more = true;
            while (field_more && target_count < max_targets) {
                read_field(targets[ target_count ], "7", goto while_end, NULL);
                if (targets[ target_count ][ 0 ]) target_count++;
            }
            field_sep = -1;

            u64 ttl    = scan_u64();
            u64 expiry = ttl ? now() + ttl : 0;

            u64   body_len;
            char *body = read_body(&body_len);
            if (!body) goto while_end; // limit

            for (u8 i = 0; i < target_count; i++) {
                if (sendmail(user, userlen, targets[ i ], body, body_len, expiry)) continue;
                writes("!");
                goto while_end;
            }
            writes("O");
        } else if (c == 'P') { // polling mode
            // scan for file change and stdin
            // exit on stdin or file deletion
            int ifd = inotify_init();
            inotify_add_watch(ifd, count_filename, IN_ALL_EVENTS);

            u64 after = now();

            writes(">");
            flush();
            while (true) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                FD_SET(ifd, &fds);
                int nfds = (ifd > STDIN_FILENO ? ifd : STDIN_FILENO) + 1;
                select(nfds, &fds, NULL, NULL, NULL);

                if (FD_ISSET(STDIN_FILENO, &fds)) break;

                struct inotify_event ev;
                read(ifd, &ev, sizeof(ev));

                if (ev.mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) break;

                printmail(filename, after);
                after = now();
                flush();
            }

            close(ifd);
            writes("O");
        } else if (c == 'D') {
            // replaces indexed mail with last mail
            // always delete last-to-first
            u64 index = scan_u64();

            int dir_fd = open(filename, O_PATH | O_DIRECTORY);
            if (dir_fd < 0) err("8");

            int cfd = openat(dir_fd, ".count", O_RDWR);
            if (cfd < 0) {
                close(dir_fd);
                err("8");
            }

            u64 total = 0;
            read(cfd, &total, 8);
            if (total == 0 || index >= total) {
                close(cfd);
                close(dir_fd);
                err("8");
            }

            u64 last = --total;
            if (index != last) {
                char src[ SZ ], dst[ SZ ];
                nlod(src, last);
                *glob_buf = 0;
                nlod(dst, index);
                *glob_buf = 0;
                if (syscall(264, dir_fd, src, dir_fd, dst) < 0) {
                    close(cfd);
                    close(dir_fd);
                    err("8");
                }
            }

            unlinkat(dir_fd, str_u64(last), 0);

            lseek(cfd, 0, SEEK_SET);
            write(cfd, &total, 8);
            ftruncate(cfd, 8);

            close(cfd);
            close(dir_fd);
            writes("O");
        } else if (c == 'C') { // change password
            char newpass[ 65 ] = { 0 }, newpass2[ 65 ] = { 0 };
            u8   passlen, passlen2;
            read_field(newpass, "6", goto while_end, &passlen);
            read_field(newpass2, "7", goto while_end, &passlen2);

            if (passlen != passlen2) err("K");
            if (!safe_cmp(newpass, newpass2, passlen)) err("K");

            char user_path[ SZ ] = "users/";
            memcopy(user_path + 6, user, userlen);
            user_path[ 6 + userlen ] = 0;

            int ufd = smake(user_path);
            if (ufd < 0) err("8");
            slock(ufd);
            u8 newpass_len = lenstr(newpass);
            write(ufd, newpass, newpass_len);
            ftruncate(ufd, newpass_len);

            sopen(ufd);
            close(ufd);
            writes("O");
        } else if (c == 'K') { // exit client
            break;
        } else {
            writes("?");
        }
    while_end:;
    }

    flush();
    return 0;
}
