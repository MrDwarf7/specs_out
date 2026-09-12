
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MDASH "\xE2\x80\x94"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} String;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} Strings;

static void die(const char *fmt, ...) {
    va_list ap;
    fputs("[ERR] ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void *xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size);
    if (!next) die("out of memory");
    return next;
}

static char *xstrdup(const char *s) {
    char *copy = strdup(s ? s : "");
    if (!copy) die("out of memory");
    return copy;
}

static void string_init(String *s) {
    s->data = xstrdup("");
    s->len = 0;
    s->cap = 1;
}

static void string_reserve(String *s, size_t extra) {
    size_t need = s->len + extra + 1;
    if (need <= s->cap) return;
    size_t cap = s->cap;
    while (cap < need) cap = cap < 1024 ? cap * 2 : cap + cap / 2;
    s->data = xrealloc(s->data, cap);
    s->cap = cap;
}

static void string_append_n(String *s, const char *text, size_t n) {
    string_reserve(s, n);
    memcpy(s->data + s->len, text, n);
    s->len += n;
    s->data[s->len] = '\0';
}

static void string_append(String *s, const char *text) {
    string_append_n(s, text, strlen(text));
}

static void string_appendf(String *s, const char *fmt, ...) {
    va_list ap, copy;
    va_start(ap, fmt);
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) die("could not format output");
    string_reserve(s, (size_t)n);
    vsnprintf(s->data + s->len, s->cap - s->len, fmt, ap);
    va_end(ap);
    s->len += (size_t)n;
}

static char *string_take(String *s) {
    char *data = s->data;
    s->data = NULL;
    s->len = s->cap = 0;
    return data;
}

static void strings_push(Strings *v, char *owned) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 4;
        v->items = xrealloc(v->items, v->cap * sizeof(*v->items));
    }
    v->items[v->len++] = owned;
}

static void strings_free(Strings *v) {
    for (size_t i = 0; i < v->len; i++) free(v->items[i]);
    free(v->items);
    *v = (Strings){0};
}

static char *join_strings(const Strings *v, const char *sep) {
    String out;
    string_init(&out);
    for (size_t i = 0; i < v->len; i++) {
        if (i) string_append(&out, sep);
        string_append(&out, v->items[i]);
    }
    return string_take(&out);
}

static void trim_in_place(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void first_line(char *s) {
    char *nl = strchr(s, '\n');
    if (nl) *nl = '\0';
}

static char *capture(const char *command, bool allow_failure) {
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        if (allow_failure) return xstrdup("");
        die("could not run %s: %s", command, strerror(errno));
    }

    String out;
    string_init(&out);
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) string_append(&out, buf);
    int status = pclose(pipe);
    if (status != 0 && !allow_failure) {
        free(out.data);
        die("command failed: %s", command);
    }
    return string_take(&out);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return xstrdup("");
    String out;
    string_init(&out);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) != 0) string_append_n(&out, buf, n);
    fclose(f);
    return string_take(&out);
}

static char *shell_quote(const char *s) {
    String out;
    string_init(&out);
    string_append(&out, "'");
    for (; *s; s++) {
        if (*s == '\'') string_append(&out, "'\\''");
        else string_append_n(&out, s, 1);
    }
    string_append(&out, "'");
    return string_take(&out);
}

static char *run_for_device(const char *prefix, const char *name) {
    String path, command;
    string_init(&path);
    string_appendf(&path, "/dev/%s", name);
    char *quoted = shell_quote(path.data);
    string_init(&command);
    string_appendf(&command, "%s %s 2>/dev/null", prefix, quoted);
    char *result = capture(command.data, true);
    free(path.data);
    free(quoted);
    free(command.data);
    return result;
}

/* Extract KEY="value" from one lsblk -P line, including lsblk escapes. */
static char *lsblk_field(const char *line, const char *key) {
    size_t key_len = strlen(key);
    const char *p = line;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == line || isspace((unsigned char)p[-1])) &&
            p[key_len] == '=' && p[key_len + 1] == '"') {
            p += key_len + 2;
            String out;
            string_init(&out);
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) {
                    p++;
                    switch (*p) {
                    case 'n': string_append(&out, "\n"); break;
                    case 't': string_append(&out, "\t"); break;
                    default: string_append_n(&out, p, 1); break;
                    }
                } else {
                    string_append_n(&out, p, 1);
                }
                p++;
            }
            return string_take(&out);
        }
        p += key_len;
    }
    return xstrdup("");
}

static char *line_value(const char *text, const char *label) {
    size_t label_len = strlen(label);
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        if ((size_t)(end - p) >= label_len && strncmp(p, label, label_len) == 0) {
            const char *value = p + label_len;
            while (value < end && isspace((unsigned char)*value)) value++;
            char *out = strndup(value, (size_t)(end - value));
            if (!out) die("out of memory");
            trim_in_place(out);
            return out;
        }
        p = *end ? end + 1 : end;
    }
    return xstrdup("");
}

static char *size_label(const char *input) {
    String compact;
    string_init(&compact);
    for (const char *p = input; *p; p++)
        if (!isspace((unsigned char)*p)) string_append_n(&compact, p, 1);
    if (!compact.len) return string_take(&compact);

    char unit = compact.data[compact.len - 1];
    const char *expanded = NULL;
    switch ((char)toupper((unsigned char)unit)) {
    case 'T': expanded = "TB"; break;
    case 'G': expanded = "GB"; break;
    case 'M': expanded = "MB"; break;
    case 'K': expanded = "KB"; break;
    }
    if (!expanded) return string_take(&compact);
    compact.data[--compact.len] = '\0';
    string_appendf(&compact, " %s", expanded);
    return string_take(&compact);
}

static char *gather_cpu(void) {
    char *raw = capture("lscpu", false);
    char *model = line_value(raw, "Model name:");
    char *cores_s = line_value(raw, "Core(s) per socket:");
    char *sockets_s = line_value(raw, "Socket(s):");
    char *tpc_s = line_value(raw, "Thread(s) per core:");
    char *max_mhz = line_value(raw, "CPU max MHz:");

    const char *suffixes[] = {"-Core Processor", " Processor"};
    for (size_t i = 0; i < 2; i++) {
        size_t n = strlen(model), sn = strlen(suffixes[i]);
        if (n >= sn && strcmp(model + n - sn, suffixes[i]) == 0) {
            if (i == 0) {
                char *dash = model + n - sn;
                while (dash > model && isdigit((unsigned char)dash[-1])) dash--;
                if (dash > model && dash[-1] == ' ') dash--;
                *dash = '\0';
            } else {
                model[n - sn] = '\0';
            }
            break;
        }
    }

    long cores = *cores_s ? strtol(cores_s, NULL, 10) : 1;
    long sockets = *sockets_s ? strtol(sockets_s, NULL, 10) : 1;
    long threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (threads < 1) threads = 1;
    long tpc = *tpc_s ? strtol(tpc_s, NULL, 10) : 1;
    bool smt_on = tpc != 1;
    char *smt = read_file("/sys/devices/system/cpu/smt/active");
    if (*smt) smt_on = smt[0] == '1';

    String out;
    string_init(&out);
    if (*max_mhz) {
        double ghz = strtod(max_mhz, NULL) / 1000.0;
        string_appendf(&out, "%s %s %ld cores / %ld threads (SMT %s), boosting to ~%.1fGHz",
                       model, MDASH, cores * sockets, threads, smt_on ? "on" : "off", ghz);
    } else {
        string_appendf(&out, "%s %s %ld cores / %ld threads (SMT %s)",
                       model, MDASH, cores * sockets, threads, smt_on ? "on" : "off");
    }
    free(raw); free(model); free(cores_s); free(sockets_s); free(tpc_s); free(max_mhz); free(smt);
    return string_take(&out);
}

static long meminfo_kib(const char *text, const char *key) {
    char *value = line_value(text, key);
    long result = strtol(value, NULL, 10);
    free(value);
    return result;
}

static char *word_after(const char *text, const char *marker) {
    const char *p = strstr(text, marker);
    if (!p) return xstrdup("");
    p += strlen(marker);
    while (isspace((unsigned char)*p)) p++;
    const char *end = p;
    while (*end && !isspace((unsigned char)*end)) end++;
    char *out = strndup(p, (size_t)(end - p));
    if (!out) die("out of memory");
    return out;
}

static char *gather_ram(void) {
    char *meminfo = read_file("/proc/meminfo");
    long total_kib = meminfo_kib(meminfo, "MemTotal:");
    long avail_kib = meminfo_kib(meminfo, "MemAvailable:");
    long avail_gib = avail_kib / 1024 / 1024;
    long total_gib = 0;
    char *dtype = xstrdup("");

    char *inxi = capture("command -v inxi >/dev/null 2>&1 && inxi -c 0 -m 2>/dev/null", true);
    const char *system_ram = strstr(inxi, "System RAM:");
    if (system_ram) {
        char *total = word_after(system_ram, "total:");
        total_gib = strtol(total, NULL, 10);
        free(total);
    }
    const char *type = strstr(inxi, "type: DDR");
    if (type) {
        free(dtype);
        dtype = word_after(type, "type:");
    }
    if (!total_gib) total_gib = ((total_kib / 1024 / 1024 + 7) / 8) * 8;

    String out;
    string_init(&out);
    if (*dtype) string_appendf(&out, "%ld GiB %s (~%ld GiB available)", total_gib, dtype, avail_gib);
    else string_appendf(&out, "%ld GiB (~%ld GiB available)", total_gib, avail_gib);
    free(meminfo); free(dtype); free(inxi);
    return string_take(&out);
}

static const char *fallback_vram(const char *name) {
    struct Mapping { const char *needle, *gb; } map[] = {
        {"RTX 4090", "24"}, {"RTX 4080", "16"}, {"RTX 4070 Ti SUPER", "16"},
        {"RTX 4070 Ti", "12"}, {"RTX 4070 SUPER", "12"}, {"RTX 4070", "12"},
        {"RTX 3090", "24"}, {"RTX 3080 Ti", "12"}, {"RTX 3080", "10"},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strstr(name, map[i].needle)) return map[i].gb;
    return "";
}

static char *gather_gpu(void) {
    char *smi = capture("nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null", true);
    first_line(smi);
    char *name = NULL;
    char vram[32] = "";
    char *comma = strchr(smi, ',');
    if (comma && !strstr(smi, "Failed") && !strstr(smi, "failed")) {
        *comma = '\0';
        name = xstrdup(smi);
        trim_in_place(name);
        char *memory = comma + 1;
        trim_in_place(memory);
        long amount = strtol(memory, NULL, 10);
        if (strstr(memory, "MiB")) amount /= 1024;
        snprintf(vram, sizeof(vram), "%ld", amount);
    } else {
        char *pci = capture("lspci 2>/dev/null", true);
        char *save = NULL;
        for (char *line = strtok_r(pci, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if ((strstr(line, "VGA compatible controller: NVIDIA") || strstr(line, "3D controller: NVIDIA"))) {
                char *left = strchr(line, '['), *right = left ? strchr(left + 1, ']') : NULL;
                if (left && right) {
                    *right = '\0';
                    String n; string_init(&n); string_appendf(&n, "NVIDIA %s", left + 1); name = string_take(&n);
                } else {
                    char *colon = strstr(line, ": ");
                    if (colon) name = xstrdup(colon + 2);
                }
                break;
            }
        }
        if (!name) {
            save = NULL;
            for (char *line = strtok_r(pci, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
                if (strstr(line, "VGA compatible controller:")) {
                    char *colon = strstr(line, ": ");
                    if (colon) name = xstrdup(colon + 2);
                    break;
                }
            }
        }
        if (name) {
            char *paren = strstr(name, " (");
            if (paren) *paren = '\0';
            snprintf(vram, sizeof(vram), "%s", fallback_vram(name));
        }
        free(pci);
    }

    String out;
    string_init(&out);
    if (!name || !*name) string_append(&out, "unknown");
    else if (*vram) string_appendf(&out, "%s %s %s GB VRAM", name, MDASH, vram);
    else string_append(&out, name);
    free(smi); free(name);
    return string_take(&out);
}

static char *gather_mobo(void) {
    char *vendor = read_file("/sys/devices/virtual/dmi/id/board_vendor");
    char *name = read_file("/sys/devices/virtual/dmi/id/board_name");
    first_line(vendor); first_line(name); trim_in_place(vendor); trim_in_place(name);
    String out;
    string_init(&out);
    if (!*vendor && !*name) string_append(&out, "unknown");
    else {
        string_append(&out, vendor);
        if (*vendor && *name) string_append(&out, " ");
        string_append(&out, name);
    }
    free(vendor); free(name);
    return string_take(&out);
}

static char *format_boot_drive(const char *name, const char *rota, const char *size) {
    Strings parts = {0};
    char *rows = run_for_device("lsblk -nP -o NAME,SIZE,MOUNTPOINT", name);
    char *save = NULL;
    for (char *line = strtok_r(rows, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *pname = lsblk_field(line, "NAME"), *psize = lsblk_field(line, "SIZE");
        char *mount = lsblk_field(line, "MOUNTPOINT");
        if (strcmp(pname, name) && *mount && strcmp(mount, "[SWAP]")) {
            char *label = size_label(psize);
            String part; string_init(&part); string_appendf(&part, "%s %s", label, mount);
            strings_push(&parts, string_take(&part));
            free(label);
        }
        free(pname); free(psize); free(mount);
    }
    char *label = size_label(size), *joined = join_strings(&parts, ", ");
    String out; string_init(&out);
    string_appendf(&out, "- Boot drive: %s %s (%s)", label, strcmp(rota, "1") == 0 ? "HDD" : "SSD", name);
    if (parts.len) string_appendf(&out, " %s %s", MDASH, joined);
    free(rows); free(label); free(joined); strings_free(&parts);
    return string_take(&out);
}

static char *format_nvme(const char *name, const char *size) {
    bool efi = false, msr = false, main_part = false, recovery = false;
    char *rows = run_for_device("lsblk -nP -o NAME,PARTTYPENAME", name);
    char *save = NULL;
    for (char *line = strtok_r(rows, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *type = lsblk_field(line, "PARTTYPENAME");
        if (!strcmp(type, "EFI System")) efi = true;
        else if (!strcmp(type, "Microsoft reserved")) msr = true;
        else if (!strcmp(type, "Microsoft basic data")) main_part = true;
        else if (!strcmp(type, "Windows recovery environment")) recovery = true;
        free(type);
    }
    Strings win = {0};
    if (efi) strings_push(&win, xstrdup("EFI"));
    if (msr) strings_push(&win, xstrdup("MSR"));
    if (main_part) strings_push(&win, xstrdup("main"));
    if (recovery) strings_push(&win, xstrdup("recovery"));
    char *label = size_label(size), *joined = join_strings(&win, " + ");
    String out; string_init(&out);
    string_appendf(&out, "- NVMe: %s (%s)", label, name);
    if (msr || recovery || main_part)
        string_appendf(&out, " %s Windows partition layout (%s)", MDASH, joined);
    free(rows); free(label); free(joined); strings_free(&win);
    return string_take(&out);
}

static char *format_storage(const char *name, const char *rota, const char *size) {
    Strings mounts = {0}, sizes = {0};
    char *rows = run_for_device("lsblk -nP -o NAME,SIZE,MOUNTPOINT", name);
    char *save = NULL;
    for (char *line = strtok_r(rows, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *pname = lsblk_field(line, "NAME"), *psize = lsblk_field(line, "SIZE");
        char *mount = lsblk_field(line, "MOUNTPOINT");
        if (strcmp(pname, name)) {
            strings_push(&sizes, size_label(psize));
            if (*mount && strcmp(mount, "[SWAP]")) strings_push(&mounts, xstrdup(mount));
        }
        free(pname); free(psize); free(mount);
    }
    char *label = size_label(size), *joined_sizes = join_strings(&sizes, " + ");
    char *joined_mounts = join_strings(&mounts, ", ");
    String out; string_init(&out);
    string_appendf(&out, "- Storage %s: %s (%s)", strcmp(rota, "1") == 0 ? "HDD" : "SSD", label, name);
    if (!mounts.len && sizes.len) string_appendf(&out, " %s %s partitions, unmounted", MDASH, joined_sizes);
    else if (mounts.len == 1) string_appendf(&out, " mounted at %s", mounts.items[0]);
    else if (mounts.len > 1) string_appendf(&out, " mounted at %s", joined_mounts);
    free(rows); free(label); free(joined_sizes); free(joined_mounts);
    strings_free(&mounts); strings_free(&sizes);
    return string_take(&out);
}

static char *gather_disks(void) {
    char *boot_src = capture("findmnt -no SOURCE /boot 2>/dev/null", true);
    first_line(boot_src); trim_in_place(boot_src);
    char *boot_disk = xstrdup("");
    if (*boot_src) {
        char *quoted = shell_quote(boot_src);
        String command; string_init(&command); string_appendf(&command, "lsblk -no PKNAME %s 2>/dev/null", quoted);
        free(boot_disk); boot_disk = capture(command.data, true);
        first_line(boot_disk); trim_in_place(boot_disk);
        free(quoted); free(command.data);
    }

    Strings boot = {0}, nvme = {0}, storage = {0};
    char *rows = capture("lsblk -dnP -o NAME,ROTA,SIZE,TRAN,MODEL", false);
    char *save = NULL;
    for (char *line = strtok_r(rows, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *name = lsblk_field(line, "NAME"), *rota = lsblk_field(line, "ROTA");
        char *size = lsblk_field(line, "SIZE"), *tran = lsblk_field(line, "TRAN");
        if (*name && strncmp(name, "zram", 4) && strncmp(name, "loop", 4) && strncmp(name, "ram", 3)) {
            if (*boot_disk && !strcmp(name, boot_disk)) strings_push(&boot, format_boot_drive(name, rota, size));
            else if (!strcmp(tran, "nvme") || !strncmp(name, "nvme", 4)) strings_push(&nvme, format_nvme(name, size));
            else strings_push(&storage, format_storage(name, rota, size));
        }
        free(name); free(rota); free(size); free(tran);
    }
    String out; string_init(&out);
    Strings *groups[] = {&boot, &nvme, &storage};
    for (size_t g = 0; g < 3; g++) for (size_t i = 0; i < groups[g]->len; i++) {
        if (out.len) string_append(&out, "\n");
        string_append(&out, groups[g]->items[i]);
    }
    free(boot_src); free(boot_disk); free(rows);
    strings_free(&boot); strings_free(&nvme); strings_free(&storage);
    return string_take(&out);
}

static bool swap_is_active(const char *path, const char *swaps) {
    char *copy = xstrdup(swaps), *save = NULL;
    bool found = false;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *end = line;
        while (*end && !isspace((unsigned char)*end)) end++;
        if ((size_t)(end - line) == strlen(path) && !strncmp(line, path, (size_t)(end - line))) {
            found = true;
            break;
        }
    }
    free(copy);
    return found;
}

static void add_swap_bit(Strings *bits, const char *size, const char *kind, bool active) {
    char *label = size_label(size);
    String bit; string_init(&bit); string_appendf(&bit, "%s %s (%s)", label, kind, active ? "active" : "unused");
    strings_push(bits, string_take(&bit));
    free(label);
}

static char *gather_swap(void) {
    Strings bits = {0};
    char *swaps = read_file("/proc/swaps");
    char *rows = capture("lsblk -nP -o NAME,FSTYPE,SIZE,MOUNTPOINT", false);
    char *save = NULL;
    for (char *line = strtok_r(rows, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *name = lsblk_field(line, "NAME"), *fstype = lsblk_field(line, "FSTYPE");
        char *size = lsblk_field(line, "SIZE"), *mount = lsblk_field(line, "MOUNTPOINT");
        String path; string_init(&path); string_appendf(&path, "/dev/%s", name);
        if (!strncmp(name, "zram", 4)) {
            add_swap_bit(&bits, size, "zram", swap_is_active(path.data, swaps));
        } else if (!strcmp(fstype, "swap")) {
            bool active = swap_is_active(path.data, swaps) || !strcmp(mount, "[SWAP]");
            add_swap_bit(&bits, size, "disk swap", active);
        }
        free(path.data); free(name); free(fstype); free(size); free(mount);
    }

    char *copy = xstrdup(swaps);
    save = NULL;
    bool first = true;
    for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (first) { first = false; continue; }
        char *end = line;
        while (*end && !isspace((unsigned char)*end)) end++;
        char saved = *end; *end = '\0';
        if (*line && strncmp(line, "/dev/", 5)) {
            struct stat st;
            if (stat(line, &st) == 0 && S_ISREG(st.st_mode)) {
                char *quoted = shell_quote(line);
                String command; string_init(&command); string_appendf(&command, "du -h %s 2>/dev/null", quoted);
                char *du = capture(command.data, true); first_line(du);
                char *space = du;
                while (*space && !isspace((unsigned char)*space)) space++;
                *space = '\0';
                if (*du) add_swap_bit(&bits, du, "disk swap", true);
                free(quoted); free(command.data); free(du);
            }
        }
        *end = saved;
    }
    char *result = bits.len ? join_strings(&bits, ", ") : xstrdup("none");
    free(swaps); free(rows); free(copy); strings_free(&bits);
    return result;
}

static char *gather_network(void) {
    Strings wifi = {0}, eth_names = {0}, eth_states = {0}, other = {0}, bits = {0};
    char *links = capture("ip -br link 2>/dev/null", true);
    char *save = NULL;
    for (char *line = strtok_r(links, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char name[256], state[64];
        if (sscanf(line, "%255s %63s", name, state) != 2 || !strncmp(name, "lo", 2)) continue;
        String item; string_init(&item);
        if (!strncmp(name, "wlan", 4) || !strncmp(name, "wlp", 3) || !strncmp(name, "wifi", 4)) {
            string_appendf(&item, "Wi-Fi (%s, %s)", name, state); strings_push(&wifi, string_take(&item));
        } else if (!strncmp(name, "en", 2) || !strncmp(name, "eth", 3)) {
            free(item.data); strings_push(&eth_names, xstrdup(name)); strings_push(&eth_states, xstrdup(state));
        } else if (!strncmp(name, "tailscale", 9)) {
            string_appendf(&item, "Tailscale (%s)", name); strings_push(&other, string_take(&item));
        } else if (!strncmp(name, "wg", 2)) {
            string_appendf(&item, "WireGuard (%s)", name); strings_push(&other, string_take(&item));
        } else {
            string_appendf(&item, "%s (%s)", name, state); strings_push(&other, string_take(&item));
        }
    }
    if (wifi.len) strings_push(&bits, join_strings(&wifi, ", "));
    if (eth_names.len == 1) {
        String item; string_init(&item); string_appendf(&item, "Ethernet (%s, %s)", eth_names.items[0], eth_states.items[0]);
        strings_push(&bits, string_take(&item));
    } else if (eth_names.len > 1) {
        bool same = true;
        for (size_t i = 1; i < eth_states.len; i++) if (strcmp(eth_states.items[0], eth_states.items[i])) same = false;
        String item; string_init(&item);
        if (same) {
            char *names = join_strings(&eth_names, "/");
            string_appendf(&item, "%zux Ethernet (%s, %s)", eth_names.len, names, eth_states.items[0]);
            free(names);
        } else {
            Strings mixed = {0};
            for (size_t i = 0; i < eth_names.len; i++) {
                String pair; string_init(&pair); string_appendf(&pair, "%s %s", eth_names.items[i], eth_states.items[i]);
                strings_push(&mixed, string_take(&pair));
            }
            char *joined = join_strings(&mixed, ", "); string_appendf(&item, "Ethernet (%s)", joined);
            free(joined); strings_free(&mixed);
        }
        strings_push(&bits, string_take(&item));
    }
    if (other.len) strings_push(&bits, join_strings(&other, ", "));
    char *result = bits.len ? join_strings(&bits, ", ") : xstrdup("none");
    free(links); strings_free(&wifi); strings_free(&eth_names); strings_free(&eth_states); strings_free(&other); strings_free(&bits);
    return result;
}

static char *gather_os(void) {
    char *release = read_file("/etc/os-release");
    char *name = line_value(release, "NAME=");
    size_t n = strlen(name);
    if (n >= 2 && name[0] == '"' && name[n - 1] == '"') {
        memmove(name, name + 1, n - 2);
        name[n - 2] = '\0';
    }
    if (!*name) { free(name); name = xstrdup("Linux"); }
    char *kernel = capture("uname -r", false); first_line(kernel); trim_in_place(kernel);
    String out; string_init(&out); string_appendf(&out, "%s, kernel %s", name, kernel);
    free(release); free(name); free(kernel);
    return string_take(&out);
}

static const char *default_output(void) {
    const char *home = getenv("HOME");
    if (!home || !*home) die("HOME is not set; pass -o PATH");
    static String path;
    string_init(&path);
    string_appendf(&path, "%s/.specs", home);
    return path.data;
}

static void usage(const char *program) {
    const char *base = strrchr(program, '/');
    base = base ? base + 1 : program;
    fprintf(stdout,
        "Usage: %s [options] [PATH]\n\n"
        "Gather hardware/OS facts from system commands and write a ~/.specs\n"
        "document (same bullet format as the existing file).\n\n"
        "Options:\n"
        "  -t, --truncate      Replace an existing file without asking\n"
        "  -y, --yes           Same as --truncate (assume yes)\n"
        "  -n, --dry-run       Print the document, write nothing\n"
        "  -o, --output PATH   Write to PATH (default: %s)\n"
        "  -h, --help          Show this help\n\n"
        "PATH as a positional argument is the same as -o PATH.\n\n"
        "If the destination already exists and neither -t nor -y was passed,\n"
        "you are asked before replacing it. Non-interactive stdin refuses to\n"
        "clobber without -t/-y.\n", base, default_output());
}

static bool confirm_replace(const char *dest, bool force) {
    if (access(dest, F_OK) != 0 || force) return true;
    if (!isatty(STDIN_FILENO)) die("%s exists; pass -t or -y to replace", dest);
    fprintf(stderr, "File exists: %s\nReplace with new version? [y/N] ", dest);
    fflush(stderr);
    char answer[32] = "";
    if (!fgets(answer, sizeof(answer), stdin)) return false;
    trim_in_place(answer);
    return !strcmp(answer, "y") || !strcmp(answer, "Y") || !strcmp(answer, "yes") || !strcmp(answer, "YES");
}

int main(int argc, char **argv) {
    const char *dest = default_output();
    bool force = false, dry_run = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--truncate") || !strcmp(argv[i], "-y") || !strcmp(argv[i], "--yes")) force = true;
        else if (!strcmp(argv[i], "-n") || !strcmp(argv[i], "--dry-run")) dry_run = true;
        else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if (++i >= argc) die("Option %s requires an argument", argv[i - 1]);
            dest = argv[i];
        } else if (!strcmp(argv[i], "--")) {
            if (++i < argc) dest = argv[i];
            break;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "[ERR] Unknown flag: %s\n", argv[i]);
            usage(argv[0]);
            return EXIT_FAILURE;
        } else dest = argv[i];
    }

    char *cpu = gather_cpu(), *ram = gather_ram(), *gpu = gather_gpu(), *mobo = gather_mobo();
    char *disks = gather_disks(), *swap = gather_swap(), *network = gather_network(), *os = gather_os();
    String doc; string_init(&doc);
    string_appendf(&doc, MDASH " CPU: %s\n" MDASH " RAM: %s\n" MDASH " GPU: %s\n" MDASH " Motherboard: %s\n", cpu, ram, gpu, mobo);
    if (*disks) string_appendf(&doc, "%s\n", disks);
    string_appendf(&doc, MDASH " Swap: %s\n" MDASH " Network: %s\n" MDASH " OS: %s\n", swap, network, os);

    if (dry_run) {
        fprintf(stderr, "[DRY] would write to %s\n", dest);
        fputs(doc.data, stdout);
    } else if (!confirm_replace(dest, force)) {
        fprintf(stderr, "Not writing %s\n", dest);
        fputs(doc.data, stdout);
    } else {
        FILE *out = fopen(dest, "w");
        if (!out) die("cannot open %s: %s", dest, strerror(errno));
        if (fwrite(doc.data, 1, doc.len, out) != doc.len || fclose(out) != 0)
            die("could not write %s: %s", dest, strerror(errno));
        fprintf(stderr, "Wrote %s\n", dest);
    }

    free(cpu); free(ram); free(gpu); free(mobo); free(disks); free(swap); free(network); free(os); free(doc.data);
    return 0;
}
