/* lockbox-wayland <socket> <app_id>
 *
 * Creates a Wayland socket "tagged" with the wp_security_context_v1 protocol (the same one Flatpak uses). The
 * compositor (KWin, Mutter, Sway...) treats everything that connects through it as a sandboxed app and withholds
 * the privileged protocols: reading the clipboard without focus (data-control), faking keyboard/mouse input
 * (fake-input), capturing the screen (screencast) and listing/controlling other programs' windows.
 *
 * Prints "ready" once the socket exists. Stays alive until stdin closes; then the compositor closes the context
 * (disconnecting its clients) and the socket is removed. lockbox ties stdin to a pipe that closes when the box exits.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client.h>
#include "security-context-v1-client.h"

static struct wp_security_context_manager_v1 *manager;

static void global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version) {
    (void)d; (void)version;
    if (!strcmp(iface, wp_security_context_manager_v1_interface.name))
        manager = wl_registry_bind(r, name, &wp_security_context_manager_v1_interface, 1);
}
static void global_remove(void *d, struct wl_registry *r, uint32_t name) { (void)d; (void)r; (void)name; }
static const struct wl_registry_listener listener = {global, global_remove};

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: lockbox-wayland <socket> <app_id>\n"); return 2; }
    const char *path = argv[1], *app_id = argv[2];

    struct wl_display *display = wl_display_connect(NULL);
    if (!display) { fprintf(stderr, "lockbox-wayland: no Wayland session\n"); return 1; }
    wl_registry_add_listener(wl_display_get_registry(display), &listener, NULL);
    wl_display_roundtrip(display);
    if (!manager) { fprintf(stderr, "lockbox-wayland: the compositor has no wp_security_context_v1\n"); return 3; }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    if (strlen(path) >= sizeof addr.sun_path) { fprintf(stderr, "lockbox-wayland: path too long\n"); return 2; }
    strcpy(addr.sun_path, path);
    int listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unlink(path);
    if (listen_fd < 0 || bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) || listen(listen_fd, 64)) {
        perror("lockbox-wayland"); return 1;
    }
    int close_pipe[2];
    if (pipe2(close_pipe, O_CLOEXEC)) { perror("lockbox-wayland"); unlink(path); return 1; }

    struct wp_security_context_v1 *ctx = wp_security_context_manager_v1_create_listener(manager, listen_fd, close_pipe[1]);
    wp_security_context_v1_set_sandbox_engine(ctx, "lockbox");
    wp_security_context_v1_set_app_id(ctx, app_id);
    wp_security_context_v1_commit(ctx);
    wp_security_context_v1_destroy(ctx);
    if (wl_display_roundtrip(display) < 0) { fprintf(stderr, "lockbox-wayland: the compositor refused\n"); unlink(path); return 1; }
    close(listen_fd);
    close(close_pipe[1]);   /* the compositor holds the other end: when we close close_pipe[0], it ends the context */

    puts("ready");
    fflush(stdout);
    char c;
    while (read(0, &c, 1) > 0) {}
    unlink(path);
    return 0;
}
