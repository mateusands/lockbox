/* lockbox-probe: runs INSIDE the box (during --test) and checks which Wayland protocols that a sandboxed app should
 * not have are still offered to it. It reads nothing and sends no key or click: it only lists what the compositor
 * advertises.
 *   exit 0 = nothing exposed, 1 = something exposed (one line each), 2 = no Wayland
 */
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

static const char *dangerous[][2] = {
    {"ext_data_control_manager_v1", "read/replace the clipboard without focus"},
    {"zwlr_data_control_manager_v1", "read/replace the clipboard without focus"},
    {"org_kde_kwin_fake_input", "fake keyboard and mouse input"},
    {"zwp_virtual_keyboard_manager_v1", "fake keyboard input"},
    {"zwlr_virtual_pointer_manager_v1", "fake mouse input"},
    {"zkde_screencast_unstable_v1", "capture the screen"},
    {"zwlr_screencopy_manager_v1", "capture the screen"},
    {"ext_image_copy_capture_manager_v1", "capture the screen"},
    {"org_kde_plasma_window_management", "see and control other programs' windows"},
    {"zwlr_foreign_toplevel_manager_v1", "see and control other programs' windows"},
    {"ext_foreign_toplevel_list_v1", "see other programs' windows"},
    {"org_kde_kwin_remote_access_manager", "remote access to the screen"},
    {"zwp_input_method_manager_v2", "act as a keyboard (input method)"},
};
static int exposed;

static void global(void *d, struct wl_registry *r, uint32_t n, const char *iface, uint32_t v) {
    (void)d; (void)r; (void)n; (void)v;
    for (size_t i = 0; i < sizeof dangerous / sizeof *dangerous; i++)
        if (!strcmp(iface, dangerous[i][0])) {
            printf("EXPOSED: %s (%s)\n", dangerous[i][0], dangerous[i][1]);
            exposed++;
        }
}
static void global_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener listener = {global, global_remove};

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) { puts("no Wayland"); return 2; }
    wl_registry_add_listener(wl_display_get_registry(display), &listener, NULL);
    wl_display_roundtrip(display);
    if (!exposed) puts("no privileged Wayland protocols");
    wl_display_disconnect(display);
    return exposed ? 1 : 0;
}
