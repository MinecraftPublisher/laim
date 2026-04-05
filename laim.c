// laim (0.2.3): a lame mail server
// centeralized design, manual admin sign-up
// files are easily parsable by other programs, and this can act as a sort of IPC as well.
// use --boot to run this program over TCP+TLS
// connect to it with `ncat --ssl <address> <port>`
// or `nc <address> <port>` if not running under SSL
//
// stripped using elfkickers' sstrip
// compressed using `upx --ultra-brute laim --lzma --no-align`
//
// created by Moon Flower Fields (https://coffin.ir/)
// or @moonflowerfields on Telegram

// very funny.
#define COMEDIAN false

#define SZ  64 + 5 + 1 + 1
#define EOF -1

#include <fcntl.h>
#include <stdint.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef uint64_t u64;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef u8       byte;
typedef byte bool;
const bool true  = 1;
const bool false = 0;

struct linux_dirent64 {
    u64  d_ino;
    u64  d_off;
    u16  d_reclen;
    u8   d_type;
    char d_name[];
};

char get_char() {
GETCHAR_START:;
    char buf[ 1 ];
    if (read(STDIN_FILENO, buf, 1) == 0) return EOF;
    if (buf[ 0 ] == '\r') goto GETCHAR_START; // band-aid fix.
    return buf[ 0 ];
}

char *sadd(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

void memcopy(char *s, const char *d, u64 len) {
    for (u64 i = 0; i < len; i++) s[ i ] = d[ i ];
}

void memzero(char* s, u64 len) {
    for (u64 i = 0; i < len; i++) s[i] = 0;
}

char *nadd(char *p, u64 n) {
    if (!n) {
        *p++ = '0';
        return p;
    }
    char tmp[ 20 ];
    int  i = 20;
    while (n) {
        tmp[ --i ] = '0' + (n % 10);
        n /= 10;
    }
    int len = 20 - i;
    memcopy(p, tmp + i, len);
    return p + len;
}

void print_u64(u64 n) {
    char  tmp[ 20 ];
    char *p = nadd(tmp, n);
    write(STDOUT_FILENO, tmp, p - tmp);
}

typedef struct {
    char buf[ 21 ];
    int  len;
} u64str;

u64str str_u64(u64 n) {
    u64str s;
    char  *p = nadd(s.buf, n);
    *p       = 0;
    s.len    = p - s.buf;
    return s;
}

u64 scan_u64() {
    u64  n = 0;
    char tsbuf[ 21 ];
    int  len = read(STDIN_FILENO, tsbuf, sizeof(tsbuf) - 1);
    for (int i = 0; i < len; i++) {
        if (tsbuf[ i ] >= '0' && tsbuf[ i ] <= '9') n = n * 10 + (tsbuf[ i ] - '0');
        else
            break;
    }
    return n;
}

u64 lenstr(const char *str) {
    u64 i = 0;
    while (str[i]) i++;
    return i;
}

void writes(const char *string) { write(STDOUT_FILENO, string, lenstr(string)); }

void list_users(int dir_fd) {
    char buf[ 1024 ];
    int  n;
    while ((n = syscall(217, dir_fd, buf, sizeof(buf))) > 0) {
        for (int off = 0; off < n;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + off);
            if (d->d_name[ 0 ] != '.') {
                write(STDOUT_FILENO, d->d_name, lenstr(d->d_name));
                char  lastlogin[ SZ ] = { 0 };
                char *p               = lastlogin;
                p                     = sadd(p, "mail/");
                p                     = sadd(p, d->d_name);
                p                     = sadd(p, "/.login");
                int login_fd          = open(lastlogin, O_RDONLY);
                if (login_fd >= 0) {
                    u64 ts;
                    read(login_fd, &ts, 8);
                    writes(" ");
                    print_u64(ts);
                    writes("\n");
                    close(login_fd);
                } else {
                    writes(" 0\n");
                }
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

void printmail(char path[ SZ ], u64 after) {
    mkdir(path, 0700); // make it if it doesnt exist

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

    for (u64 i = 0; i < total; i++) {
        u64str name    = str_u64(i);
        int    mail_fd = openat(dir_fd, name.buf, O_RDONLY);
        if (mail_fd < 0) continue;
        flock(mail_fd, LOCK_SH);

        struct stat st;
        fstat(mail_fd, &st);
        if (st.st_size == 0) {
            close(mail_fd);
            continue;
        }

        u64  ts = 0;
        char tsbuf[ 21 ];
        int  n = read(mail_fd, tsbuf, sizeof(tsbuf) - 1);
        if (n <= 0) {
            close(mail_fd);
            continue;
        }
        tsbuf[ n ] = 0;
        for (int j = 0; j < n; j++) {
            if (tsbuf[ j ] >= '0' && tsbuf[ j ] <= '9') ts = ts * 10 + (tsbuf[ j ] - '0');
            else {
                lseek(mail_fd, j - n, SEEK_CUR);
                break;
            }
        }

        if (ts < after) {
            close(mail_fd);
            continue;
        }

        char *p, header[ 64 + 1 ] = { 0 };
        p    = nadd(header, i);
        *p++ = '\n';
        write(STDOUT_FILENO, header, p - header);
        p = nadd(header, ts);
        write(STDOUT_FILENO, header, p - header);

        char buf[ 256 ];
        int  r;
        while ((r = read(mail_fd, buf, sizeof(buf))) > 0) write(STDOUT_FILENO, buf, r);

        writes("\n\neof\n");
        close(mail_fd);
    }
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
	, 
};

// used for both passwords and usernames.
// not my fault if you wanna use a weird pass.
bool is_path_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
           || c == ' ' || c == '$';
}

#define read_field(buf, err, fail_action, len_buf)                                                                             \
    ({                                                                                                                         \
        bool _term = false;                                                                                                    \
        for (u8 _i = 0; _i < 64; _i++) {                                                                                       \
            (buf)[ _i ] = get_char();                                                                                          \
            if ((buf)[ _i ] == '\n') {                                                                                         \
                (buf)[ _i ] = 0;                                                                                               \
                _term       = true;                                                                                            \
                if (len_buf != NULL) {                                                                                         \
                    u8 *ptr = len_buf;                                                                                         \
                    *ptr    = _i;                                                                                              \
                }                                                                                                              \
                break;                                                                                                         \
            }                                                                                                                  \
            if (!is_path_char((buf)[ _i ])) {                                                                                  \
                writes(err);                                                                                                   \
                fail_action;                                                                                                   \
            }                                                                                                                  \
        }                                                                                                                      \
        if (!_term) {                                                                                                          \
            writes(err);                                                                                                       \
            fail_action;                                                                                                       \
        }                                                                                                                      \
    })

// timing safe or whatever.
bool safe_cmp(char *left, char *right, u64 size) {
    bool nonmatch = false;
    bool ended    = false;
    for (u64 i = 0; i < size; i++) {
        nonmatch = ((*left != *right) && !ended) | nonmatch;
        ended    = (*left == 0 || *right == 0) | ended;
        left++, right++;
    }
    return !nonmatch;
}

void openwrite(char *filename, const char *content) {
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    write(fd, content, lenstr(content));
    close(fd);
}

int main(int argc, char **argv) {
    char path[ 4096 ] = {0};
	readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (argc > 1) {
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
                "\n");
            return 1;
        } else if (safe_cmp(argv[ 1 ], "--open", 6)) {
            openwrite("laim.c", source);
            openwrite("docs.txt", docs);
            openwrite("Makefile", makefile);
            writes("done\n");
            return 1;
        } else if (safe_cmp(argv[ 1 ], "--boot", 6)) {
            if (argc != 5) {
                writes("too many / too little args, read --help\n");
                return 1;
            }
            char *new_argv[] = { "tcpsvd", "-v", argv[ 2 ], argv[ 3 ], "ssl_server", "-f", argv[ 4 ], path };
            execvp("busybox", new_argv);
        } else {
            writes("unknown arg\n");
            return 1;
        }
    }

    char user[ 64 + 1 ] = { 0 }, pass[ 64 + 1 ] = { 0 };
    u8   userlen = 0;
    // v=1
    writes("laim\2\n");

    // manipulates:
    // users/    - user db, manually add users
    // mail/     - mails directory, each filename is a username
    mkdir("users", 0700);
    mkdir("mail", 0700);
    mkdir("mail/broadcast", 0700);

__BACK_TO_START:;
    memzero(user, 65);
    memzero(pass, 65);
    int action = get_char();
    if (action == 'M') {
        writes(motd);
        goto __BACK_TO_START;
    } else if (action == 'L') {
        // username
        read_field(user, "4", goto __BACK_TO_START, &userlen);
        // password
        read_field(pass, "5", goto __BACK_TO_START, NULL);

        char user_path[ 5 + 1 + 64 + 1 ] = "users/";
        memcopy(user_path + 6, user, userlen);
        user_path[ 6 + userlen ] = 0;

        if (access(user_path, F_OK) != 0) {
            writes("N");
            goto __BACK_TO_START;
        }

        int fd = open(user_path, O_RDONLY);
        if (fd < 0) {
            writes("err:fopen\n");
            return 1;
        }
        char actual_pass[ 65 ]   = { 0 };
        int  pass_size           = read(fd, actual_pass, 64);
        actual_pass[ pass_size ] = 0;
        if (!safe_cmp(pass, actual_pass, 64)) { // safe timing or whatever.
            writes("I");
            sleep(2); // punish user
            goto __BACK_TO_START;
        }
        memzero(actual_pass, 65);
        close(fd);
        writes("O");
        // only fallthrough
    } else if (action == EOF) {
        return 1;
    } else {
        writes("?");
        goto __BACK_TO_START;
    }

    // -- authorized from here on out --

    { // write last login
        char  last_login[ SZ ] = { 0 };
        char *p                = last_login;
        p                      = sadd(p, "mail/");
        p                      = sadd(p, user);
        p                      = sadd(p, "/.login");
        int login_fd           = open(last_login, O_RDWR | O_CREAT, 0600);
        if (login_fd >= 0) {
            ftruncate(login_fd, 0);
            u64 ts = time(NULL);
            write(login_fd, &ts, 8);
            close(login_fd);
        }
    }

    bool broadcast_mode = false; // kinda useless. may delete. TODO

    // remove password from memory
    memzero(pass, 65);

    // make sure user has a mail dir
    char  user_filename[ SZ ] = { 0 };
    char *p                   = user_filename;
    p                         = sadd(p, "mail/");
    p                         = sadd(p, user);
    p                         = sadd(p, "/");
    mkdir(user_filename, 0700);

    char user_count_filename[ SZ ] = { 0 };
    p                              = sadd(user_count_filename, user_filename);
    p                              = sadd(p, ".count");

    char broadcast_filename[ SZ ]       = "mail/broadcast/";
    char broadcast_count_filename[ SZ ] = "mail/broadcast/.count";

    char *filename       = user_filename;
    char *count_filename = user_count_filename;

    int _c;
    while ((_c = get_char()) != EOF) {
        if (_c == '\n') continue;
        char c = _c;
        if (c == 'G') { // get all mail
            printmail(filename, 0);
        } else if (c == 'W') { // whoami
            if (COMEDIAN) writes("you're... you!");
            else { writes(user); }
            writes("\n");
        } else if (c == 'E') { // count inbox size, includes deleted mails
            int count_fd = open(count_filename, O_RDONLY);
            if (count_fd < 0) err("!");
            u64 result = 0;
            int size   = read(count_fd, &result, 8);
            if (size < 8) {
                close(count_fd);
                err("!");
            }
            close(count_fd);
            print_u64(result);
            writes("\n");
        } else if (c == 'B') { // turn on broadcast mode.
            broadcast_mode = true;
            filename       = broadcast_filename;
            count_filename = broadcast_count_filename;
            writes("O");
        } else if (c == 'Q') { // turn off broadcast mode.
            broadcast_mode = false;
            filename       = user_filename;
            count_filename = user_count_filename;
            writes("O");
        } else if (c == 'U') { // list all users
            int dir_fd = open("users/", O_RDONLY | O_DIRECTORY);
            if (dir_fd < 0) {
                writes("err:dir\n");
                continue;
            }
            list_users(dir_fd);
            close(dir_fd);
        } else if (c == 'T') { // get mail after X
            printmail(filename, scan_u64());
        } else if (c == 'S') { // send mail
            char target[ 64 + 1 ];
            read_field(target, "7", goto __WHILE_END, NULL);

            char dest_path[ 64 + 5 + 1 + 20 + 1 ];
            p = sadd(dest_path, "mail/");
            p = sadd(p, target);
            p = sadd(p, "/");
            mkdir(dest_path, 0700);

            // lock directory and calculate size to prevent issues
            int dir_fd = open(dest_path, O_RDONLY | O_DIRECTORY);
            if (dir_fd < 0) {
                writes("err:dir\n");
                continue;
            }
            slock(dir_fd);

            u64 index = create_index(dir_fd);
            if (index == -1) {
                writes("!");
                continue;
            }
            p = sadd(dest_path, "mail/");
            p = sadd(p, target);
            p = sadd(p, "/");
            p = nadd(p, index);

            int user_fd = smake(dest_path);
            if (user_fd < 0) {
                sopen(dir_fd);
                close(dir_fd);
                writes("err:smake\n");
                continue;
            }
            slock(user_fd);
            sopen(dir_fd);
            close(dir_fd);

            u64 timestamp = time(NULL);
            print_u64(timestamp);
            writes("\n");
            writes(user);
            writes("\n");

            bool hit_limit = false;

            u64  total_size = 0;
            char buf[ 256 ];
            int  read_size;
            while ((read_size = read(STDIN_FILENO, buf, 256)) != 0 && read_size > 0) {
                total_size += read_size;
                write(user_fd, buf, read_size);
                if (total_size > 65536) {
                    hit_limit = true;
                    writes(".");
                    break;
                }
            }

            if (!hit_limit) writes("O");

            sopen(user_fd);
            close(user_fd);
        } else if (c == 'D') { // delete mail
            u64 index = scan_u64();

            char dest_path[ 64 + 5 + 1 + 20 + 1 ];
            p = sadd(dest_path, filename);
            p = nadd(p, index);
            if (unlink(dest_path) < 0) err("8");
            else { err("O"); }
        } else if (c == 'C') { // change password
            char newpass[ 65 ], newpass2[ 65 ];
            u8 passlen, passlen2;
            read_field(newpass, "6", goto __WHILE_END, &passlen);
            read_field(newpass2, "7", goto __WHILE_END, &passlen2);

            // passwords don't match
            if (passlen != passlen2) err("K");
            if (safe_cmp(newpass, newpass2, passlen) != 0) err("K");

            char user_path[ 6 + 64 + 1 ] = "users/";
            memcopy(user_path + 6, user, userlen);
            user_path[ 6 + userlen ] = 0;

            int ufd = smake(user_path);
            if (ufd < 0) err("8");
            slock(ufd);
            int newpass_len = lenstr(newpass);
            write(ufd, newpass, newpass_len);
            ftruncate(ufd, newpass_len); // if newpass_len < oldpass_len
            sopen(ufd);
            close(ufd);
            writes("O");
        } else {
            writes("?");
        }
    __WHILE_END:;
    }

    return 0;
}
