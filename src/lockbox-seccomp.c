/* lockbox-seccomp: writes a seccomp filter (BPF) to stdout, for `bwrap --seccomp FD`.
 *
 * The filter denies (EPERM) kernel calls that normal apps do not use and that are a frequent path to exploiting
 * kernel bugs or reaching outside the box. Everything not on the list stays allowed: games, Wine/Proton, Vulkan and
 * audio work as before. The list follows Flatpak's (common/flatpak-run.c), plus io_uring.
 *
 * Blocking new user namespaces is NOT done here: bwrap does it better with --disable-userns.
 *
 *   lockbox-seccomp [--userns] > filter.bpf        (or: lockbox-seccomp --list)
 *
 * --userns: for boxes started with --allow-userns (Proton through umu/pressure-vessel, Chromium). Inside a new user
 * namespace the app NEEDS to mount things (that is what pressure-vessel's own bwrap does), so the mount calls are
 * allowed; the rest of the list stays blocked.
 */
#include <errno.h>
#include <seccomp.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct rule { const char *name; int sys; int mount; };

int main(int argc, char **argv) {
    const struct rule deny[] = {
        /* the kernel keyring: holds secrets of other processes in the session */
        {"keyctl", SCMP_SYS(keyctl), 0}, {"add_key", SCMP_SYS(add_key), 0}, {"request_key", SCMP_SYS(request_key), 0},
        /* large kernel surfaces with a history of exploited bugs */
        {"bpf", SCMP_SYS(bpf), 0}, {"perf_event_open", SCMP_SYS(perf_event_open), 0},
        /* userfaultfd is NOT denied: emulators and recompilations use it to see which guest memory the game wrote
           (a texture changed, code was patched). Without it, TotK Recomp re-decoded every ASTC texture on every
           frame (8 FPS in the menu instead of 60). The dangerous part of it (catching faults raised inside the
           kernel) is already off for unprivileged users when vm.unprivileged_userfaultfd = 0, the default. */
        {"io_uring_setup", SCMP_SYS(io_uring_setup), 0}, {"io_uring_enter", SCMP_SYS(io_uring_enter), 0},
        {"io_uring_register", SCMP_SYS(io_uring_register), 0},
        /* system administration: without capabilities these would fail anyway, here they fail earlier */
        {"kexec_load", SCMP_SYS(kexec_load), 0}, {"kexec_file_load", SCMP_SYS(kexec_file_load), 0},
        {"init_module", SCMP_SYS(init_module), 0}, {"finit_module", SCMP_SYS(finit_module), 0},
        {"delete_module", SCMP_SYS(delete_module), 0}, {"syslog", SCMP_SYS(syslog), 0}, {"uselib", SCMP_SYS(uselib), 0},
        {"acct", SCMP_SYS(acct), 0}, {"quotactl", SCMP_SYS(quotactl), 0}, {"swapon", SCMP_SYS(swapon), 0},
        {"swapoff", SCMP_SYS(swapoff), 0}, {"reboot", SCMP_SYS(reboot), 0},
        {"mount", SCMP_SYS(mount), 1}, {"umount2", SCMP_SYS(umount2), 1}, {"pivot_root", SCMP_SYS(pivot_root), 1},
        {"open_tree", SCMP_SYS(open_tree), 1}, {"move_mount", SCMP_SYS(move_mount), 1}, {"fsopen", SCMP_SYS(fsopen), 1},
        {"fsconfig", SCMP_SYS(fsconfig), 1}, {"fsmount", SCMP_SYS(fsmount), 1}, {"fspick", SCMP_SYS(fspick), 1},
        {"mount_setattr", SCMP_SYS(mount_setattr), 1},
        /* opening a file by "handle" skips the path checks */
        {"open_by_handle_at", SCMP_SYS(open_by_handle_at), 0}, {"name_to_handle_at", SCMP_SYS(name_to_handle_at), 0},
        /* changing the system clock */
        {"settimeofday", SCMP_SYS(settimeofday), 0}, {"clock_settime", SCMP_SYS(clock_settime), 0},
        {"clock_adjtime", SCMP_SYS(clock_adjtime), 0}, {"adjtimex", SCMP_SYS(adjtimex), 0},
    };
    const size_t n = sizeof deny / sizeof *deny;

    int userns = argc > 1 && !strcmp(argv[1], "--userns");
    if (argc > 1 && !strcmp(argv[1], "--list")) {
        for (size_t i = 0; i < n; i++) printf("%s\n", deny[i].name);
        puts("ioctl TIOCSTI/TIOCLINUX (typing into the terminal outside)");
        return 0;
    }
    if (isatty(1)) { fprintf(stderr, "lockbox-seccomp: redirect the output to a file (it is binary)\n"); return 2; }

    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) return 1;
    /* filter for x86-64 and also for 32-bit programs (Wine, Steam) */
    seccomp_arch_add(ctx, SCMP_ARCH_X86);
    seccomp_arch_add(ctx, SCMP_ARCH_X32);
    for (size_t i = 0; i < n; i++)
        if (deny[i].sys >= 0 && !(userns && deny[i].mount)) seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), deny[i].sys, 0);
    /* TIOCSTI/TIOCLINUX: "typing" into the terminal that started the app (bwrap --new-session already covers it;
       this is a second lock) */
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ioctl), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, TIOCSTI));
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ioctl), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, TIOCLINUX));
    int r = seccomp_export_bpf(ctx, 1);
    seccomp_release(ctx);
    return r ? 1 : 0;
}
