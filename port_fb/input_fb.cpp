#include "comm.h"
#include "state.h"
#include "nc2000.h"
#include "key_new.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern unsigned keypadmatrix[8][8];

struct KeyYX { int y, x; };
static KeyYX key_id_to_yx[0x40];
static bool yx_map_inited = false;

static void init_yx_map() {
    memset(key_id_to_yx, 0xFF, sizeof(key_id_to_yx));
    struct { int id; int y; int x; } map[] = {
        {0x0B, 3,1}, {0x0C, 4,1}, {0x0D, 5,1}, {0x0A, 2,1},
        {0x09, 1,1}, {0x08, 0,1}, {0x0E, 6,1}, {0x0F, 0,0},
        {0x38, 0,7}, {0x39, 1,7}, {0x3A, 2,7}, {0x3B, 3,7},
        {0x3C, 4,7}, {0x3D, 5,7}, {0x3E, 6,7}, {0x3F, 7,7},
        {0x30, 0,6}, {0x31, 1,6}, {0x32, 2,6}, {0x33, 3,6},
        {0x34, 4,6}, {0x35, 5,6}, {0x36, 6,6}, {0x37, 7,6},
        {0x28, 0,5}, {0x29, 1,5}, {0x2A, 2,5}, {0x2B, 3,5},
        {0x2C, 4,5}, {0x2D, 5,5}, {0x2E, 6,5}, {0x2F, 7,5},
        {0x20, 0,4}, {0x21, 1,4}, {0x22, 2,4}, {0x23, 3,4},
        {0x24, 4,4}, {0x25, 5,4}, {0x26, 6,4}, {0x27, 7,4},
        {0x18, 0,3}, {0x19, 1,3}, {0x1A, 2,3}, {0x1B, 3,3},
        {0x1C, 4,3}, {0x1D, 5,3}, {0x1E, 6,3}, {0x1F, 7,3},
        {0x10, 0,2}, {0x11, 1,2}, {0x12, 2,2}, {0x13, 3,2},
        {0x14, 4,2}, {0x15, 5,2},
    };
    for (auto &m : map) {
        if (m.id < 0x40) { key_id_to_yx[m.id] = {m.y, m.x}; }
    }
    yx_map_inited = true;
}
#include <errno.h>

extern nc2k_states_t nc2k_states;

// ============================================================
//  evdev keycode → WQX key_id mapping
//  (same as key.cpp's map_key, but using Linux evdev keycodes)
// ============================================================
static uint8_t evdev_to_wqx(int code) {
    switch (code) {
        case KEY_RIGHT:      return 0x1F;
        case KEY_LEFT:       return 0x3F;
        case KEY_DOWN:       return 0x1B;
        case KEY_UP:         return 0x1A;
        case KEY_ENTER:      return 0x1D;
        case KEY_SPACE:      return 0x3E;
        case KEY_DOT:        return 0x3D;
        case KEY_ESC:        return 0x3B;
        case KEY_MINUS:      return 0x0E;
        case KEY_EQUAL:      return 0x3E;
        case KEY_LEFTBRACE:  return 0x38;
        case KEY_RIGHTBRACE: return 0x39;
        case KEY_BACKSLASH:  return 0x3A;
        case KEY_COMMA:      return 0x37;
        case KEY_SLASH:      return 0x1E;
        case KEY_BACKSPACE:  return 0x3F;
        case KEY_0:          return 0x3C;
        case KEY_1:          return 0x34;
        case KEY_2:          return 0x35;
        case KEY_3:          return 0x36;
        case KEY_4:          return 0x2C;
        case KEY_5:          return 0x2D;
        case KEY_6:          return 0x2E;
        case KEY_7:          return 0x24;
        case KEY_8:          return 0x25;
        case KEY_9:          return 0x26;
        case KEY_A:          return 0x28;
        case KEY_B:          return 0x34;
        case KEY_C:          return 0x32;
        case KEY_D:          return 0x2A;
        case KEY_E:          return 0x22;
        case KEY_F:          return 0x2B;
        case KEY_G:          return 0x2C;
        case KEY_H:          return 0x2D;
        case KEY_I:          return 0x27;
        case KEY_J:          return 0x2E;
        case KEY_K:          return 0x2F;
        case KEY_L:          return 0x19;
        case KEY_M:          return 0x36;
        case KEY_N:          return 0x35;
        case KEY_O:          return 0x18;
        case KEY_P:          return 0x1C;
        case KEY_Q:          return 0x20;
        case KEY_R:          return 0x23;
        case KEY_S:          return 0x29;
        case KEY_T:          return 0x24;
        case KEY_U:          return 0x26;
        case KEY_V:          return 0x33;
        case KEY_W:          return 0x21;
        case KEY_X:          return 0x31;
        case KEY_Y:          return 0x25;
        case KEY_Z:          return 0x30;
        case KEY_F5:         return 0x0B; // 英汉
        case KEY_F6:         return 0x0C; // 名片
        case KEY_F7:         return 0x0D; // 计算
        case KEY_F8:         return 0x0A; // 行程
        case KEY_F9:         return 0x09; // 测验
        case KEY_F10:        return 0x08; // 时间
        case KEY_F11:        return 0x0E; // 网络
        case KEY_F12:        return 0x0F; // on/off
        case KEY_SEMICOLON:  return 0x15; // 发音
        case KEY_APOSTROPHE: return 0x14; // 报时
        case KEY_TAB:        return 0xFF; // special: fast-forward toggle
        default:             return 0xFF;
    }
}

// ============================================================
//  Path 1: evdev input reader (physical keyboard + uinput)
// ============================================================
static int evdev_fd = -1;
static volatile bool input_running = false;

static int find_keyboard_evdev() {
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        char name[256] = "";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (strstr(name, "tca8418")) {
            printf("evdev: found tca8418 keyboard at %s\n", path);
            closedir(dir);
            return fd;
        }
        close(fd);
    }
    closedir(dir);
    return -1;
}

static void *evdev_thread(void *arg) {
    (void)arg;
    struct input_event ev;

    while (input_running) {
        int n = read(evdev_fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(5000); // 5ms poll
                continue;
            }
            break;
        }
        if (ev.type != EV_KEY) continue;
        if (ev.value > 1) continue; // ignore repeat

        bool down = (ev.value == 1);
        if (!yx_map_inited) init_yx_map();
        fprintf(stderr, "evdev: code=%d %s\n", ev.code, down ? "DOWN" : "UP");
        uint8_t wqx_key = evdev_to_wqx(ev.code);
        if (wqx_key == 0xFF) {
            if (ev.code == KEY_TAB && down) {
                fast_forward ^= 1;
            }
            continue;
        }
        if (wqx_key < 0x40 && key_id_to_yx[wqx_key].y >= 0) {
            int y = key_id_to_yx[wqx_key].y;
            int x = key_id_to_yx[wqx_key].x;
            keypadmatrix[y][x] = down ? 1 : 0;
            fprintf(stderr, "evdev: code=%d -> wqx=0x%02x -> matrix[%d][%d]=%d\n", ev.code, wqx_key, y, x, down?1:0);
        }
    }
    return NULL;
}

// ============================================================
//  Path 2: stdin/TCP socket remote key input
//  Protocol: single byte per key event
//    Byte format: bit7 = press(1)/release(0), bit6..0 = WQX key_id
//    Example: 0x9D = press key 0x1D (ENTER), 0x1D = release key 0x1D
// ============================================================
static int tcp_server_fd = -1;
static int tcp_client_fd = -1;
static volatile bool remote_running = false;

static void process_remote_byte(uint8_t byte) {
    if (!yx_map_inited) init_yx_map();
    bool down = (byte & 0x80) != 0;
    uint8_t key_id = byte & 0x7F;
    fprintf(stderr, "TCP key: id=0x%02x %s\n", key_id, down ? "DOWN" : "UP");
    if (key_id < 0x40 && key_id_to_yx[key_id].y >= 0) {
        int y = key_id_to_yx[key_id].y;
        int x = key_id_to_yx[key_id].x;
        keypadmatrix[y][x] = down ? 1 : 0;
        fprintf(stderr, "  -> keypadmatrix[%d][%d] = %d\n", y, x, down ? 1 : 0);
    }
}

static void *stdin_thread(void *arg) {
    (void)arg;
    // Read from stdin in raw mode
    uint8_t buf[64];
    while (remote_running) {
        int n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            usleep(10000);
            continue;
        }
        for (int i = 0; i < n; i++) {
            process_remote_byte(buf[i]);
        }
    }
    return NULL;
}

static void *tcp_server_thread(void *arg) {
    int port = *(int *)arg;
    (void)arg;

    tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_server_fd < 0) { perror("socket"); return NULL; }

    int opt = 1;
    setsockopt(tcp_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(tcp_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(tcp_server_fd); return NULL;
    }
    listen(tcp_server_fd, 1);
    fprintf(stderr, "TCP key server listening on port %d\n", port);

    while (remote_running) {
        tcp_client_fd = accept(tcp_server_fd, NULL, NULL);
        if (tcp_client_fd < 0) continue;
        printf("TCP key client connected\n");

        uint8_t buf[64];
        while (remote_running) {
            int n = read(tcp_client_fd, buf, sizeof(buf));
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                process_remote_byte(buf[i]);
            }
        }
        close(tcp_client_fd);
        tcp_client_fd = -1;
        printf("TCP key client disconnected\n");
    }
    return NULL;
}

// ============================================================
//  uinput virtual keyboard (for Mac SSH remote control)
// ============================================================
static int uinput_fd = -1;

int create_uinput_device() {
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("open /dev/uinput");
        return -1;
    }

    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    // Enable all key codes we care about
    for (int i = 0; i < KEY_MAX; i++)
        ioctl(uinput_fd, UI_SET_KEYBIT, i);

    struct uinput_setup usetup = {};
    strcpy(usetup.name, "WQX Remote Keyboard");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;

    ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
    ioctl(uinput_fd, UI_DEV_CREATE);
    printf("uinput: created virtual keyboard device\n");
    return 0;
}

void inject_uinput_key(int code, int value) {
    if (uinput_fd < 0) return;
    struct input_event ev = {};
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    write(uinput_fd, &ev, sizeof(ev));

    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    write(uinput_fd, &ev, sizeof(ev));
}

void destroy_uinput_device() {
    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        uinput_fd = -1;
    }
}

// ============================================================
//  Public API
// ============================================================
static pthread_t evdev_tid, stdin_tid, tcp_tid;
static int tcp_port = 9527;

void init_keyitems() {
    // stub: key_new.cpp's init_keyitems needs SDL; we use evdev directly
}

void handle_key_wayback(signed int sym, bool key_down) {
    // stub: not used in fb port, evdev thread calls SetKey directly
}

void input_fb_init(bool enable_evdev, bool enable_stdin, bool enable_tcp, int port) {
    if (enable_evdev) {
        evdev_fd = find_keyboard_evdev();
        if (evdev_fd >= 0) {
            input_running = true;
            pthread_t t;
            pthread_create(&t, NULL, evdev_thread, NULL);
            pthread_detach(t);
            evdev_tid = t;
        } else {
            printf("Warning: no keyboard evdev found\n");
        }
    }

    if (enable_stdin) {
        remote_running = true;
        pthread_t t;
        pthread_create(&t, NULL, stdin_thread, NULL);
        pthread_detach(t);
        stdin_tid = t;
    }

    if (enable_tcp) {
        tcp_port = port;
        remote_running = true;
        pthread_t t;
        pthread_create(&t, NULL, tcp_server_thread, &tcp_port);
        pthread_detach(t);
        tcp_tid = t;
    }
}

void input_fb_shutdown() {
    input_running = false;
    remote_running = false;
    if (evdev_fd >= 0) { close(evdev_fd); evdev_fd = -1; }
    if (tcp_client_fd >= 0) { close(tcp_client_fd); tcp_client_fd = -1; }
    if (tcp_server_fd >= 0) { close(tcp_server_fd); tcp_server_fd = -1; }
    destroy_uinput_device();
}
