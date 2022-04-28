#define _GNU_SOURCE // for memfd_create

#include "assert.h"
#include "types.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h> // struct winsize, TIOCSWINSZ, ioctl,
#include <sys/mman.h> // memfd_create
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, FD_ISSET, select
#include <termios.h> // struct termios, TCSANOW, tcgetattr, tcsetattr
#include <unistd.h> // ftruncate

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>


#define DEFAULT_SHELL "/bin/sh"

#define UNUSED(name) __attribute__((__unused__)) name ## __UNUSED


typedef struct RawDataBuffer
{
    size_t size;
    size_t bytes_read;

    char *base;
    char *wrap;

    char *read;
    char *write;
} RawDataBuffer;


#define LINE_BUFFER_COUNT 80

typedef struct TerminalLine
{
    size_t first_byte;
    size_t one_past_last_byte;
} TerminalLine;


typedef struct TerminalLineBuffer
{
    RawDataBuffer *data;
    size_t total_line_count;

    size_t current_line;
    TerminalLine lines[LINE_BUFFER_COUNT];
} TerminalLineBuffer;


typedef struct Terminal
{
    TerminalLineBuffer *buffer;
    size_t current_line;

    unsigned cols;
    unsigned rows;

    unsigned cursor_x;
    unsigned cursor_y;
} Terminal;



static size_t
copy_string(char *dst, const char *src, size_t len)
{
    size_t count = 0;

    for (; src[count] && (count < len); ++count)
    {
        dst[count] = src[count];
    }
    if (count < len)
    {
        dst[count] = 0;
    }

    return count;
}


static int
is_printable(char c)
{
    int result = (c >= 32) && (c <= 126);
    return result;
}


#if 0
static unsigned
minu(unsigned a, unsigned b)
{
    unsigned result = a < b ? a : b;
    return result;
}
#endif


static unsigned long long
minull(unsigned long long a, unsigned long long b)
{
    unsigned long long result = a < b ? a : b;
    return result;
}


#if 0
static int
power_of_2s(long long v)
{
    int result = (v > 0) && (0 == (v & (v - 1)));

    return result;
}
#endif


static int
power_of_2u(unsigned long long v)
{
    int result = v && (0 == (v & (v - 1)));

    return result;
}


static unsigned
alignu_down(unsigned value, unsigned align)
{
    unsigned result = value / align * align;

    return result;
}


static unsigned long long
alignull_up2(unsigned long long value, unsigned long long align)
{
    ASSERT(power_of_2u(align));

    unsigned long long mask = align - 1;

    unsigned long long result = (value + (mask - 1)) & ~mask;

    return result;
}


static _Noreturn void
error_exit(const char *message)
{
    fputs(message, stderr);
    exit(EXIT_FAILURE);
}


#if 0
static _Noreturn void
errorf_exit(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}
#endif


static _Noreturn void
errno_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


static ssize_t
pty_read(int pty_fd, RawDataBuffer *buffer)
{
    size_t used = CAST(size_t, buffer->write - buffer->read);

    size_t avail = buffer->size - used;

    ssize_t bytes_read = read(pty_fd, buffer->write, minull(avail, EXPR_MAX(bytes_read)));
    if (bytes_read < 0)
    {
        perror("pty_read");
    }
    else
    {
#if 0
        printf("Read %ld bytes\n", bytes_read);
        for (ssize_t i = 0; i < bytes_read; ++i)
        {
            if (i)
            {
                printf(", ");
            }
            char c = buffer->write[i];
            if (isprint(c))
            {
                printf("'%c'", c);
            }
            else
            {
                printf("<%d>", c);
            }
        }
        fputs("\n", stdout);
#endif
        buffer->bytes_read += CAST(size_t, bytes_read);
        buffer->write += bytes_read;
        if (buffer->write >= buffer->wrap)
        {
            buffer->read -= buffer->size;
            buffer->write -= buffer->size;
        }
    }
    return bytes_read;
}


static int
pty_open(char *name, size_t len)
{
    int pty_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty_fd == -1)
    {
        errno_exit("pty_open:posix_openpt");
    }

    if(grantpt(pty_fd) == -1)
    {
        errno_exit("pty_open:grantpt");
    }

    if (unlockpt(pty_fd) == -1)
    {
        errno_exit("pty_open:unlockpt");
    }

    char *pty_name = ptsname(pty_fd);
    if (!pty_name)
    {
        errno_exit("pty_open:ptsname");
    }

    size_t copied = copy_string(name, pty_name, len);
    if (copied == len)
    {
        errno = EOVERFLOW;
        errno_exit("pty_open:copy_string");
    }

    return pty_fd;
}


static pid_t
pty_spawn(int *fd, struct winsize *winsize)
{
    pid_t pid = -1;

    size_t pty_name_len = PATH_MAX;
    char *pty_name = malloc(pty_name_len);
    if (!pty_name)
    {
        errno_exit("pty_spawn:malloc");
    }

    int parent_fd = pty_open(pty_name, pty_name_len);
    if (parent_fd >= 0)
    {
        pid = fork();
        switch (pid)
        {
            // fork failed
            case -1:
            {
                errno_exit("pty_spawn:fork");
            } break;

            // we're the child process
            case 0:
            {
                if (setsid() == -1)
                {
                    errno_exit("pty_spawn:setsid");
                }

                close(parent_fd);
                int child_fd = open(pty_name, O_RDWR);
                if (child_fd == -1)
                {
                    errno_exit("pty_spawn:open");
                }

                if (winsize)
                {
                    if (ioctl(child_fd, TIOCSWINSZ, winsize) == -1)
                    {
                        errno_exit("pty_spawn:ioctl TIOCSWINSZ");
                    }
                }

                if (dup2(child_fd, STDIN_FILENO) != STDIN_FILENO)
                {
                    errno_exit("pty_spawn:dup2 stdin");
                }
                if (dup2(child_fd, STDOUT_FILENO) != STDOUT_FILENO)
                {
                    errno_exit("pty_spawn:dup2 stdout");
                }
                if (dup2(child_fd, STDERR_FILENO) != STDERR_FILENO)
                {
                    errno_exit("pty_spawn:dup2 stderr");
                }

                if (child_fd > STDERR_FILENO)
                {
                    close(child_fd);
                }
            } break;

            // we're the parent process. pid = the process id of the child
            default:
            {
                *fd = parent_fd;
            } break;
        }
    }

    free(pty_name);
    return pid;
}


typedef struct XlibConnection
{
    Display *display;
    Window window;
    int fd;

    XftDraw *draw;
    XftFont *font;
    XftColor color;

    unsigned short width;
    unsigned short height;

    unsigned short cursor_x;
    unsigned short cursor_y;
} XlibConnection;


static Atom WM_PROTOCOLS;
static Atom WM_DELETE_WINDOW;


static void
xlib_process_key_press(XKeyEvent *event, int pty_fd)
{
    char buf[32];
    KeySym keysym;
    int bytes = XLookupString(event, buf, sizeof(buf), &keysym, 0);
#if 0
    printf("KeySym: %lu (%s) -> %d bytes: ", keysym, XKeysymToString(keysym), bytes);
    for (int i = 0; i < bytes; ++i)
    {
        if (i)
        {
            printf(", ");
        }
        char c = buf[i];
        if (is_printable(c))
        {
            printf("%d (%c)", c, c);
        }
        else
        {
            printf("%d", c);
        }
    }
    fputs("\n", stdout);
#endif
    write(pty_fd, buf, CAST(size_t, bytes));
}


static void
draw_buffer(XlibConnection *x_connection, Terminal *terminal)
{
    printf("%s: width = %u, height = %u\n", __func__, x_connection->width, x_connection->height);
    XClearWindow(x_connection->display, x_connection->window);

    TerminalLineBuffer *lines = terminal->buffer;
    RawDataBuffer *data = lines->data;

    // @todo Handle jumps in the cursor position
    unsigned cursor_x = 0;
    unsigned cursor_y = 0;
    if (terminal->rows > lines->total_line_count)
    {
        cursor_y = CAST(unsigned, lines->total_line_count);
    }
    else
    {
        cursor_y = terminal->rows;
    }

    size_t current_line = lines->current_line;
    int x_pos = 0;
    int y_pos = CAST(int, cursor_y) * x_connection->font->height - x_connection->font->descent;
    for (size_t lines_to_draw = 0; lines_to_draw < cursor_y; ++lines_to_draw)
    {
        TerminalLine *line = lines->lines + current_line;

        // @todo Handle long lines whose start may no longer be in the buffer
        // In this case we probably want to display the portion of the line
        // which may still be in the buffer
        if (data->bytes_read - line->one_past_last_byte >= data->size)
        {
            printf("skipping line %zu: last_byte = %zu but first available is %zu\n",
                current_line, line->one_past_last_byte, data->bytes_read - data->size);
            break;
        }

        size_t first_byte = line->first_byte;
        size_t offset = data->bytes_read - first_byte;

        if (offset > data->size)
        {
            offset = data->size;
            first_byte = data->bytes_read - offset;
        }

        char *byte_to_read = data->write - offset;
        while ((first_byte++ < line->one_past_last_byte) && (x_pos < x_connection->width))
        {
            char c = *byte_to_read++;
            if (is_printable(c))
            {
                XftDrawStringUtf8(
                    x_connection->draw, &x_connection->color, x_connection->font,
                    x_pos, y_pos, CAST(FcChar8 *, &c), 1);
                x_pos += x_connection->font->max_advance_width;
            }
            else
            {
                switch (c)
                {
                    case '\r':
                    {
                        x_pos = 0;
                    } break;

                    case '\n':
                    {
                        // Don't have to do anything, because this triggers an end-of-line
                    } break;

                    default:
                    {
                        printf("unrecognized control character: %d\n", c);
                    }
                }
            }
        }

        if (current_line)
        {
            --current_line;
        }
        else
        {
            current_line = ARRAY_COUNT(lines->lines) - 1;
        }
        x_pos = 0;
        y_pos -= x_connection->font->height;
    }

    cursor_x = CAST(unsigned, x_pos / x_connection->font->max_advance_width);
    cursor_y = CAST(unsigned, y_pos / x_connection->font->height);

    terminal->cursor_x = cursor_x;
    terminal->cursor_y = cursor_y;
}


static void
terminal_resize(Terminal *terminal, XlibConnection *x_connection, int pty_fd)
{
    terminal->cols = CAST(unsigned, x_connection->width / x_connection->font->max_advance_width);
    terminal->rows = CAST(unsigned, x_connection->height / x_connection->font->height);
    struct winsize terminal_size = {
        .ws_row = CAST(unsigned short, terminal->rows),
        .ws_col = CAST(unsigned short, terminal->cols),
    };

    if (ioctl(pty_fd, TIOCSWINSZ, &terminal_size) == -1)
        errno_exit("terminal_resize: ioctl");
}


static int
xlib_process_events(XlibConnection *x_connection, int pty_fd, Terminal *terminal)
{
    int running = 1;

    int event_count = XPending(x_connection->display);
    while (event_count)
    {
        for (int i = 0; i < event_count; ++i)
        {
            XEvent event;
            XNextEvent(x_connection->display, &event);
            switch (event.type)
            {
                case ConfigureNotify:
                {
                    if ((event.xconfigure.width != x_connection->width)
                        || (event.xconfigure.height != x_connection->height))
                    {
                        puts("Window resized");
                        x_connection->width = CAST(unsigned short, event.xconfigure.width);
                        x_connection->height = CAST(unsigned short, event.xconfigure.height);
                        terminal_resize(terminal, x_connection, pty_fd);
                    }
                } break;

                case Expose:
                {
                    puts("Expose");
                    draw_buffer(x_connection, terminal);
                } break;

                case KeyPress:
                {
                    xlib_process_key_press(&event.xkey, pty_fd);
                } break;

                case ClientMessage:
                {
                    if ((event.xclient.message_type == WM_PROTOCOLS)
                        && (CAST(Atom, event.xclient.data.l[0]) == WM_DELETE_WINDOW))
                    {
                        running = 0;
                    }
                } break;
            }
        }
        event_count = XPending(x_connection->display);
    }

    return running;
}


#if 0
static XRenderColor
xlib_rgba_bytes(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    XRenderColor result = {
        .red = r * EXPR_MAX(result.red) / EXPR_MAX(r),
        .green = g * EXPR_MAX(result.green) / EXPR_MAX(g),
        .blue = b * EXPR_MAX(result.blue) / EXPR_MAX(b),
        .alpha = a * EXPR_MAX(result.alpha) / EXPR_MAX(a),
    };

    _Static_assert(EXPR_MAX(result.red) <= (INT_MAX / EXPR_MAX(r)), "Integer overflow");
    _Static_assert(EXPR_MAX(result.blue) <= (INT_MAX / EXPR_MAX(g)), "Integer overflow");
    _Static_assert(EXPR_MAX(result.green) <= (INT_MAX / EXPR_MAX(b)), "Integer overflow");
    _Static_assert(EXPR_MAX(result.alpha) <= (INT_MAX / EXPR_MAX(a)), "Integer overflow");

    return result;
}
#endif


static XRenderColor
xlib_rgba_floats(float r, float g, float b, float a)
{
    ASSERT(r >= 0 && r <= 1);
    ASSERT(g >= 0 && g <= 1);
    ASSERT(b >= 0 && b <= 1);
    ASSERT(a >= 0 && a <= 1);

    XRenderColor result = {
        .red = CAST(unsigned short, (r * EXPR_MAX(result.red)) + .5),
        .green = CAST(unsigned short, (g * EXPR_MAX(result.blue)) + .5),
        .blue = CAST(unsigned short, (b * EXPR_MAX(result.green)) + .5),
        .alpha = CAST(unsigned short, (a * EXPR_MAX(result.alpha)) + .5),
    };

    return result;
}


static void
xlib_window_create(XlibConnection *connection)
{
    Display *display = XOpenDisplay(0);
    if (!display)
    {
        error_exit("xlib_window_create:XOpenDisplay");
    }

    int screen = DefaultScreen(display);
    unsigned screen_width = CAST(unsigned, DisplayWidth(display, screen));
    unsigned screen_height = CAST(unsigned, DisplayHeight(display, screen));

    XftFont *font = XftFontOpen(
        display, screen,
        XFT_FAMILY, XftTypeString, "mono",
        nullptr);
    if (!font)
    {
        error_exit("XftFontOpen");
    }
    printf("Font: width: %d, height: %d, ascent: %d, descent: %d\n",
        font->max_advance_width, font->height, font->ascent, font->descent);

    unsigned font_width = CAST(unsigned, font->max_advance_width);
    unsigned font_height = CAST(unsigned, font->height);

    unsigned cols = 80;
    unsigned rows = 25;

    unsigned window_width = font_width * cols;
    if (window_width > screen_width)
    {
        window_width = alignu_down(screen_width, font_width);
        cols = window_width / font_width;
    }

    unsigned window_height = font_height * rows;
    if (window_height > screen_height)
    {
        window_height = alignu_down(screen_height, font_height);
        rows = window_height / font_height;
    }

    Window parent = RootWindow(display, screen);
    int window_x = CAST(int, (screen_width - window_width) / 2);
    int window_y = CAST(int, (screen_height - window_height) / 2);
    unsigned border_width = 0;

    int color_depth = CopyFromParent;
    unsigned int window_class = InputOutput;
    Visual *visual = DefaultVisual(display, screen);

    unsigned long attribute_mask = CWBackPixel | CWEventMask;
    XSetWindowAttributes attributes = {
        .background_pixel = BlackPixel(display, screen),
        .event_mask = ExposureMask | KeyPressMask | StructureNotifyMask,
    };

    Window window = XCreateWindow(
            display, parent,
            window_x, window_y, window_width, window_height, border_width,
            color_depth, window_class, visual,
            attribute_mask, &attributes);

    char *window_name = "Terminal";
    XTextProperty wm_name;
    Xutf8TextListToTextProperty(display, &window_name, 1, XUTF8StringStyle, &wm_name);
    XSetWMName(display, window, &wm_name);
    XFree(wm_name.value);

    XWMHints *wm_hints = XAllocWMHints();
    if (wm_hints)
    {
        wm_hints->flags = InputHint | StateHint;
        wm_hints->input = True;
        wm_hints->initial_state = NormalState;
        XSetWMHints(display, window, wm_hints);
        XFree(wm_hints);
    }
    else
    {
        error_exit("xlib_window_create:XAllocWMHints");
    }

    XSizeHints *wm_normal_hints = XAllocSizeHints();
    if (wm_normal_hints)
    {
        wm_normal_hints->flags = PPosition | PSize;
        XSetWMNormalHints(display, window, wm_normal_hints);
        XFree(wm_normal_hints);
    }
    else
    {
        error_exit("xlib_window_create:XAllocSizeHints");
    }

    XClassHint *wm_class = XAllocClassHint();
    if (wm_class)
    {
        wm_class->res_name = "terminal";
        wm_class->res_class = "Terminal Emulator";
        XSetClassHint(display, window, wm_class);
        XFree(wm_class);
    }
    else
    {
        error_exit("xlib_window_create:XAllocClassHint");
    }

    WM_PROTOCOLS = XInternAtom(display, "WM_PROTOCOLS", False);
    if (!WM_PROTOCOLS)
    {
        error_exit("xlib_window_create:XInternAtom WM_PROTOCOLS");
    }
    WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if (!WM_DELETE_WINDOW)
    {
        error_exit("xlib_window_create:XInternAtom WM_DELETE_WINDOW");
    }
    if (!XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1))
    {
        error_exit("xlib_window_create:XSetWmProtocols");
    }

    XMapWindow(display, window);

    Colormap colormap = DefaultColormap(display, screen);
    //XRenderColor value = xlib_rgba_bytes(255, 255 / 2, 0, 255);
    XRenderColor value = xlib_rgba_floats(1, 1, 1, 1);
    XftColor color;
    XftColorAllocValue(display, visual, colormap, &value, &color);

    XftDraw *draw = XftDrawCreate(display, window, visual, DefaultColormap(display, screen));

    connection->display = display;
    connection->window = window;
    connection->fd = ConnectionNumber(display);
    connection->draw = draw;
    connection->font = font;
    connection->color = color;
    connection->width = 0;
    connection->height = 0;
}


static void
parse_lines(TerminalLineBuffer *buffer)
{
    // @todo Handle jumps in the cursor position

    // IMPORTANT! This function needs to see all data in order to properly
    // parse whatever the end state of the terminal is, even if the terminal
    // ends up not displaying it all (e.g., a block of data is received that
    // exceeds a full screen of data)
    RawDataBuffer *data = buffer->data;
    size_t bytes_to_read = CAST(size_t, data->write - data->read);
    size_t current_byte = data->bytes_read - bytes_to_read;

    TerminalLine *line = buffer->lines + buffer->current_line;
    while (data->read < data->write)
    {
        line->one_past_last_byte = ++current_byte;

        if ('\n' == *data->read++)
        {
            ++buffer->total_line_count;
            if (++buffer->current_line == ARRAY_COUNT(buffer->lines))
            {
                buffer->current_line = 0;
            }

            line = buffer->lines + buffer->current_line;
            line->first_byte = line->one_past_last_byte = current_byte;
        }
    }
}


static void
data_buffer_create(RawDataBuffer *buffer, size_t size)
{
    long page_size = sysconf(_SC_PAGESIZE);

    size_t aligned_size = alignull_up2(size, CAST(size_t, page_size));
    if ((aligned_size < size) || (aligned_size > EXPR_MAX(aligned_size) / 3))
    {
        errno = EOVERFLOW;
        errno_exit("data_buffer_create: buffer size");
    }

    int fd = memfd_create("data buffer", 0);
    if (fd == -1)
    {
        errno_exit("data_buffer_create: memfd_create");
    }

    ASSERT(aligned_size < TYPE_MAX(off_t));
    if (ftruncate(fd, CAST(off_t, aligned_size)) == -1)
    {
        errno_exit("data_buffer_create: ftruncate");
    }

    char *start = mmap(nullptr, 3 * aligned_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == start)
    {
        errno_exit("data_buffer_create: mmap");
    }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_FIXED;
    if (mmap(start, aligned_size, prot, flags, fd, 0) == MAP_FAILED)
    {
        errno_exit("map first buffer");
    }
    if (mmap(start + aligned_size, aligned_size, prot, flags, fd, 0) == MAP_FAILED)
    {
        errno_exit("map second buffer");
    }
    if (mmap(start + 2 * aligned_size, aligned_size, prot, flags, fd, 0) == MAP_FAILED)
    {
        errno_exit("map third buffer");
    }
    close(fd);

    buffer->size = aligned_size;
    buffer->bytes_read = 0;
    buffer->base = start;
    buffer->wrap = start + 2 * aligned_size;
    buffer->read = start;
    buffer->write = start;
}


static void
run_terminal(int pty_fd)
{
    RawDataBuffer data_buffer;
    data_buffer_create(&data_buffer, 4000);

    TerminalLineBuffer *line_buffer = mmap(nullptr, sizeof(*line_buffer),
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == line_buffer)
    {
        errno_exit("mmap line_buffer");
    }
    line_buffer->data = &data_buffer;
    line_buffer->total_line_count = 1;

    XlibConnection x_connection;
    xlib_window_create(&x_connection);

    Terminal terminal = { .buffer = line_buffer };

    enum ClientFds {
        X_FD,
        PTY_FD,

        FD_COUNT,
    };
    struct epoll_event epoll_events[] = {
        [X_FD] = { .events = EPOLLIN, .data = {.fd = x_connection.fd} },
        [PTY_FD] = { .events = EPOLLIN, .data = {.fd = pty_fd} },
    };

    int epollfd = epoll_create(FD_COUNT);
    if (epollfd == -1)
    {
        errno_exit("epoll_create");
    }

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, x_connection.fd, epoll_events + X_FD) == -1)
    {
        errno_exit("epoll_ctl X window");
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pty_fd, epoll_events + PTY_FD) == -1)
    {
        errno_exit("epoll_ctl pty");
    }

    int running = xlib_process_events(&x_connection, pty_fd, &terminal);
    while (running)
    {
        int xevents = XPending(x_connection.display);
        printf("Getting ready to epoll. %d xevents in queue\n", xevents);
        int nfds = epoll_wait(epollfd, epoll_events, ARRAY_COUNT(epoll_events), -1);
        if (nfds == -1)
        {
            errno_exit("epoll_wait");
        }

        for (int i = 0; i < nfds; ++i)
        {
            struct epoll_event *epoll_event = epoll_events + i;
            if (pty_fd == epoll_event->data.fd)
            {
                ssize_t result = pty_read(pty_fd, &data_buffer);
                if (result > 0)
                {
                    printf("Read %ld from pty\n", result);
                    parse_lines(line_buffer);
                    draw_buffer(&x_connection, &terminal);
                }
                else
                {
                    printf("pty returned %ld, quitting...\n", result);
                    running = 0;
                }
            }
            else
            {
                ASSERT(x_connection.fd == epoll_event->data.fd);
            }
        }
        running = xlib_process_events(&x_connection, pty_fd, &terminal);
    }
}


static void
execute_shell(void)
{
    // Update the environment with our terminal information
    setenv("TERM", "nullrefterm", 1);

    char *shell = getenv("SHELL");
    if (!shell || !*shell)
    {
        shell = DEFAULT_SHELL;
    }

    execl(shell, shell, nullptr);
    // execl only returns on error
    errno_exit("execl");
}


int
main(void)
{
    int pty_fd;
    if (pty_spawn(&pty_fd, 0))
    {
        // we're the parent process
        run_terminal(pty_fd);
    }
    else
    {
        // we're the child process
        execute_shell();
    }

    return EXIT_SUCCESS;
}
