#define _GNU_SOURCE

#include "format.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const c8 *fallback_vram(
  const c8 *name
) {
  struct Mapping {
    const c8 *needle;
    const c8 *gb;
  };
  static const struct Mapping map[] = {
    {"RTX 4090", "24"},
    {"RTX 4080", "16"},
    {"RTX 4070 Ti SUPER", "16"},
    {"RTX 4070 Ti", "12"},
    {"RTX 4070 SUPER", "12"},
    {"RTX 4070", "12"},
    {"RTX 3090", "24"},
    {"RTX 3080 Ti", "12"},
    {"RTX 3080", "10"},
  };
  if (!name) return "";
  for (usize idx = 0; idx < ARRAY_SIZE(map); idx++) {
    if (strstr(name, map[idx].needle)) return map[idx].gb;
  }
  return "";
}

c8 *format_cpu(
  Arena *arena, const c8 *lscpu, const c8 *smt, i64 nproc
) {
  CpuFacts facts = parse_cpu(arena, lscpu, smt, nproc);
  Buf      out;
  buf_init(&out, arena);
  i32 cores = facts.cores * facts.sockets;
  if (facts.has_boost) {
    buf_printf(
      &out,
      "%s %s %ld cores / %ld threads (SMT %s), boosting to ~%.1fGHz",
      facts.model,
      MDASH,
      (long)cores,
      (long)facts.threads,
      facts.smt_on ? "on" : "off",
      facts.boost_ghz
    );
  } else {
    buf_printf(&out, "%s %s %ld cores / %ld threads (SMT %s)", facts.model, MDASH, (long)cores, (long)facts.threads, facts.smt_on ? "on" : "off");
  }
  return out.data;
}

c8 *format_ram(
  Arena *arena, const c8 *meminfo, const c8 *inxi
) {
  i64 total_kib = meminfo_kib(arena, meminfo ? meminfo : "", "MemTotal:");
  i64 avail_kib = meminfo_kib(arena, meminfo ? meminfo : "", "MemAvailable:");
  i64 avail_gib = avail_kib / 1024 / 1024;
  i64 total_gib = 0;
  c8 *dtype     = arena_copy_str(arena, "");

  if (inxi && inxi[0]) {
    const c8 *system_ram = strstr(inxi, "System RAM:");
    if (system_ram) {
      const c8 *total = word_after(arena, system_ram, "total:");
      total_gib       = strtol(total, nullptr, 10);
    }
    const c8 *type = strstr(inxi, "type: DDR");
    if (type) dtype = word_after(arena, type, "type:");
  }
  if (!total_gib) total_gib = (((total_kib / 1024 / 1024) + 7) / 8) * 8;

  Buf out;
  buf_init(&out, arena);
  if (dtype[0]) {
    buf_printf(&out, "%ld GiB %s (~%ld GiB available)", (long)total_gib, dtype, (long)avail_gib);
  } else {
    buf_printf(&out, "%ld GiB (~%ld GiB available)", (long)total_gib, (long)avail_gib);
  }
  return out.data;
}

static c8 *gpu_from_smi(
  Arena *arena, const c8 *smi_line, c8 *vram, usize vram_cap
) {
  if (!smi_line || !smi_line[0]) return nullptr;
  if (strstr(smi_line, "Failed") || strstr(smi_line, "failed")) return nullptr;
  const c8 *comma = strchr(smi_line, ',');
  if (!comma) return nullptr;
  c8   *name = arena_copy_n(arena, smi_line, (usize)(comma - smi_line));
  usize len  = strlen(name);
  while (len && isspace((unsigned char)name[len - 1]))
    name[--len] = '\0';
  usize lead = 0;
  while (name[lead] && isspace((unsigned char)name[lead]))
    lead++;
  if (lead) memmove(name, name + lead, strlen(name + lead) + 1);

  c8 *memory = trim_copy(arena, comma + 1);
  i64 amount = strtol(memory, nullptr, 10);
  if (strstr(memory, "MiB")) amount /= 1024;
  snprintf(vram, vram_cap, "%ld", (long)amount);
  return name;
}

static c8 *gpu_from_lspci(
  Arena *arena, const c8 *lspci
) {
  if (!lspci || !lspci[0]) return nullptr;
  const c8 *nvidia_name = nullptr;
  const c8 *vga_name    = nullptr;
  const c8 *cursor      = lspci;
  while (*cursor) {
    const c8 *end = strchr(cursor, '\n');
    if (!end) end = cursor + strlen(cursor);
    usize line_len = (usize)(end - cursor);
    c8    line[512];
    if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
    COPY_MEMORY(line, cursor, line_len);
    line[line_len] = '\0';

    bool nvidia    = strstr(line, "VGA compatible controller: NVIDIA") || strstr(line, "3D controller: NVIDIA");
    if (nvidia && !nvidia_name) {
      c8 *left  = strchr(line, '[');
      c8 *right = left ? strchr(left + 1, ']') : nullptr;
      if (left && right) {
        *right = '\0';
        Buf built;
        buf_init(&built, arena);
        buf_printf(&built, "NVIDIA %s", left + 1);
        nvidia_name = built.data;
      } else {
        const c8 *colon = strstr(line, ": ");
        if (colon) nvidia_name = arena_copy_str(arena, colon + 2);
      }
    } else if (!vga_name && strstr(line, "VGA compatible controller:")) {
      const c8 *colon = strstr(line, ": ");
      if (colon) vga_name = arena_copy_str(arena, colon + 2);
    }
    cursor = *end ? end + 1 : end;
  }
  const c8 *chosen = nvidia_name ? nvidia_name : vga_name;
  if (!chosen) return nullptr;
  c8 *name  = arena_copy_str(arena, chosen);
  c8 *paren = strstr(name, " (");
  if (paren) *paren = '\0';
  return name;
}

c8 *format_gpu(
  Arena *arena, const c8 *nvidia_smi, const c8 *lspci
) {
  c8        vram[32] = "";
  const c8 *smi_line = first_line_copy(arena, nvidia_smi ? nvidia_smi : "");
  c8       *name     = gpu_from_smi(arena, smi_line, vram, sizeof(vram));
  if (!name) {
    name = gpu_from_lspci(arena, lspci);
    if (name) snprintf(vram, sizeof(vram), "%s", fallback_vram(name));
  }

  Buf out;
  buf_init(&out, arena);
  if (!name || !name[0]) {
    buf_append(&out, "unknown");
  } else if (vram[0]) {
    buf_printf(&out, "%s %s %s GB VRAM", name, MDASH, vram);
  } else {
    buf_append(&out, name);
  }
  return out.data;
}

c8 *format_mobo(
  Arena *arena, const c8 *vendor, const c8 *name
) {
  const c8 *vend = trim_copy(arena, first_line_copy(arena, vendor ? vendor : ""));
  const c8 *body = trim_copy(arena, first_line_copy(arena, name ? name : ""));
  Buf       out;
  buf_init(&out, arena);
  if (!vend[0] && !body[0]) {
    buf_append(&out, "unknown");
  } else {
    buf_append(&out, vend);
    if (vend[0] && body[0]) buf_append_char(&out, ' ');
    buf_append(&out, body);
  }
  return out.data;
}

c8 *format_os(
  Arena *arena, const c8 *os_release, const c8 *kernel
) {
  c8   *name = line_value(arena, os_release ? os_release : "", "NAME=");
  usize n    = strlen(name);
  if (n >= 2 && name[0] == '"' && name[n - 1] == '"') {
    memmove(name, name + 1, n - 2);
    name[n - 2] = '\0';
  }
  if (!name[0]) name = arena_copy_str(arena, "Linux");
  c8 *kver = trim_copy(arena, first_line_copy(arena, kernel ? kernel : ""));
  Buf out;
  buf_init(&out, arena);
  buf_printf(&out, "%s, kernel %s", name, kver);
  return out.data;
}

typedef enum ATTR_PACKED {
  IFACE_SKIP = 0,
  IFACE_WIFI,
  IFACE_ETH,
  IFACE_TAILSCALE,
  IFACE_WIREGUARD,
  IFACE_OTHER,
} IfaceKind;

_Static_assert(sizeof(IfaceKind) == 1, "IfaceKind packed to u8");

static ATTR_PURE ATTR_HOT IfaceKind iface_kind(
  const c8 *name
) {
  if (!name || !name[0]) return IFACE_SKIP;
  if (name[0] == 'l' && name[1] == 'o') return IFACE_SKIP;
  if (strncmp(name, "wlan", 4) == 0 || strncmp(name, "wlp", 3) == 0 || strncmp(name, "wifi", 4) == 0) return IFACE_WIFI;
  if (strncmp(name, "en", 2) == 0 || strncmp(name, "eth", 3) == 0) return IFACE_ETH;
  if (strncmp(name, "tailscale", 9) == 0) return IFACE_TAILSCALE;
  if (strncmp(name, "wg", 2) == 0) return IFACE_WIREGUARD;
  return IFACE_OTHER;
}

c8 *format_network(
  Arena *arena, const c8 *ip_br_link
) {
  IfaceTable table = iface_parse(arena, ip_br_link);

  const c8 *wifi[MAX_IFACES];
  const c8 *eth_names[MAX_IFACES];
  const c8 *eth_states[MAX_IFACES];
  const c8 *other[MAX_IFACES];
  usize     wifi_n  = 0;
  usize     eth_n   = 0;
  usize     other_n = 0;

  for (usize idx = 0; idx < table.count; idx++) {
    const IfaceRow *row  = &table.rows[idx];
    IfaceKind       kind = iface_kind(row->name);
    Buf             item;
    switch (kind) {
      case IFACE_SKIP: break;
      case IFACE_WIFI:
        buf_init(&item, arena);
        buf_printf(&item, "Wi-Fi (%s, %s)", row->name, row->state);
        wifi[wifi_n++] = item.data;
        break;
      case IFACE_ETH:
        eth_names[eth_n]  = row->name;
        eth_states[eth_n] = row->state;
        eth_n++;
        break;
      case IFACE_TAILSCALE:
        buf_init(&item, arena);
        buf_printf(&item, "Tailscale (%s)", row->name);
        other[other_n++] = item.data;
        break;
      case IFACE_WIREGUARD:
        buf_init(&item, arena);
        buf_printf(&item, "WireGuard (%s)", row->name);
        other[other_n++] = item.data;
        break;
      case IFACE_OTHER:
        buf_init(&item, arena);
        buf_printf(&item, "%s (%s)", row->name, row->state);
        other[other_n++] = item.data;
        break;
    }
  }

  const c8 *bits[8];
  usize     bit_n = 0;
  if (wifi_n) bits[bit_n++] = join_strs(arena, wifi, wifi_n, ", ");

  if (eth_n == 1) {
    Buf item;
    buf_init(&item, arena);
    buf_printf(&item, "Ethernet (%s, %s)", eth_names[0], eth_states[0]);
    bits[bit_n++] = item.data;
  } else if (eth_n > 1) {
    bool same = true;
    for (usize idx = 1; idx < eth_n; idx++) {
      if (strcmp(eth_states[0], eth_states[idx]) != 0) {
        same = false;
        break;
      }
    }
    Buf item;
    buf_init(&item, arena);
    if (same) {
      c8 *names = join_strs(arena, eth_names, eth_n, "/");
      buf_printf(&item, "%zux Ethernet (%s, %s)", eth_n, names, eth_states[0]);
    } else {
      const c8 *pairs[MAX_IFACES];
      for (usize idx = 0; idx < eth_n; idx++) {
        Buf pair;
        buf_init(&pair, arena);
        buf_printf(&pair, "%s %s", eth_names[idx], eth_states[idx]);
        pairs[idx] = pair.data;
      }
      c8 *joined = join_strs(arena, pairs, eth_n, ", ");
      buf_printf(&item, "Ethernet (%s)", joined);
    }
    bits[bit_n++] = item.data;
  }

  if (other_n) bits[bit_n++] = join_strs(arena, other, other_n, ", ");
  if (!bit_n) return arena_copy_str(arena, "none");
  return join_strs(arena, bits, bit_n, ", ");
}

static void add_swap_bit(
  Arena *arena, const c8 **bits, usize *bit_n, const c8 *size, const c8 *kind, bool active
) {
  if (*bit_n >= MAX_PARTS) return;
  c8 *label = size_label(arena, size);
  Buf bit;
  buf_init(&bit, arena);
  buf_printf(&bit, "%s %s (%s)", label, kind, active ? "active" : "unused");
  bits[(*bit_n)++] = bit.data;
}

c8 *format_swap(
  Arena *arena, const c8 *swaps, const LsblkTable *table, const SwapFileHint *files, usize file_count
) {
  SwapTable swap_tab = swaps_parse(arena, swaps);
  const c8 *bits[MAX_PARTS];
  usize     bit_n = 0;

  if (table) {
    for (usize idx = 0; idx < table->count; idx++) {
      const LsblkRow *row = &table->rows[idx];
      if (!row->name || !row->name[0]) continue;
      Buf path;
      buf_init(&path, arena);
      buf_printf(&path, "/dev/%s", row->name);
      if (strncmp(row->name, SKIP_ZRAM, SKIP_ZRAM_LEN) == 0) {
        add_swap_bit(arena, bits, &bit_n, row->size, "zram", swap_path_active(&swap_tab, path.data));
      } else if (row->fstype && strcmp(row->fstype, "swap") == 0) {
        bool active = swap_path_active(&swap_tab, path.data) || (row->mountpoint && strcmp(row->mountpoint, "[SWAP]") == 0);
        add_swap_bit(arena, bits, &bit_n, row->size, "disk swap", active);
      }
    }
  }

  for (usize idx = 0; idx < file_count; idx++) {
    if (!files[idx].size || !files[idx].size[0]) continue;
    add_swap_bit(arena, bits, &bit_n, files[idx].size, "disk swap", true);
  }

  if (!bit_n) return arena_copy_str(arena, "none");
  return join_strs(arena, bits, bit_n, ", ");
}

static c8 *format_boot_drive(
  Arena *arena, const LsblkTable *table, const LsblkRow *disk
) {
  const c8 *parts[MAX_PARTS];
  usize     part_n = 0;
  for (usize idx = 0; idx < table->count; idx++) {
    const LsblkRow *row = &table->rows[idx];
    if (!row_is_child(row, disk->name)) continue;
    if (!row->mountpoint || !row->mountpoint[0]) continue;
    if (strcmp(row->mountpoint, "[SWAP]") == 0) continue;
    c8 *label = size_label(arena, row->size);
    Buf part;
    buf_init(&part, arena);
    buf_printf(&part, "%s %s", label, row->mountpoint);
    if (part_n < MAX_PARTS) parts[part_n++] = part.data;
  }
  c8 *label = size_label(arena, disk->size);
  Buf out;
  buf_init(&out, arena);
  buf_printf(&out, "- Boot drive: %s %s (%s)", label, (disk->rota && strcmp(disk->rota, "1") == 0) ? "HDD" : "SSD", disk->name);
  if (part_n) {
    c8 *joined = join_strs(arena, parts, part_n, ", ");
    buf_printf(&out, " %s %s", MDASH, joined);
  }
  return out.data;
}

static c8 *format_nvme(
  Arena *arena, const LsblkTable *table, const LsblkRow *disk
) {
  bool efi       = false;
  bool msr       = false;
  bool main_part = false;
  bool recovery  = false;
  for (usize idx = 0; idx < table->count; idx++) {
    const LsblkRow *row  = &table->rows[idx];
    const c8       *type = row->parttypename;
    if (!row_is_child(row, disk->name) || !type) continue;
    if (strcmp(type, "EFI System") == 0)
      efi = true;
    else if (strcmp(type, "Microsoft reserved") == 0)
      msr = true;
    else if (strcmp(type, "Microsoft basic data") == 0)
      main_part = true;
    else if (strcmp(type, "Windows recovery environment") == 0)
      recovery = true;
  }
  const c8 *win[4];
  usize     win_n = 0;
  if (efi) win[win_n++] = "EFI";
  if (msr) win[win_n++] = "MSR";
  if (main_part) win[win_n++] = "main";
  if (recovery) win[win_n++] = "recovery";

  c8 *label = size_label(arena, disk->size);
  Buf out;
  buf_init(&out, arena);
  buf_printf(&out, "- NVMe: %s (%s)", label, disk->name);
  if (msr || recovery || main_part) {
    c8 *joined = join_strs(arena, win, win_n, " + ");
    buf_printf(&out, " %s Windows partition layout (%s)", MDASH, joined);
  }
  return out.data;
}

static c8 *format_storage(
  Arena *arena, const LsblkTable *table, const LsblkRow *disk
) {
  const c8 *mounts[MAX_PARTS];
  const c8 *sizes[MAX_PARTS];
  usize     mount_n = 0;
  usize     size_n  = 0;
  for (usize idx = 0; idx < table->count; idx++) {
    const LsblkRow *row = &table->rows[idx];
    if (!row_is_child(row, disk->name)) continue;
    if (size_n < MAX_PARTS) sizes[size_n++] = size_label(arena, row->size);
    if (row->mountpoint && row->mountpoint[0] && strcmp(row->mountpoint, "[SWAP]") != 0) {
      if (mount_n < MAX_PARTS) mounts[mount_n++] = row->mountpoint;
    }
  }
  c8 *label = size_label(arena, disk->size);
  Buf out;
  buf_init(&out, arena);
  buf_printf(&out, "- Storage %s: %s (%s)", (disk->rota && strcmp(disk->rota, "1") == 0) ? "HDD" : "SSD", label, disk->name);
  if (!mount_n && size_n) {
    c8 *joined = join_strs(arena, sizes, size_n, " + ");
    buf_printf(&out, " %s %s partitions, unmounted", MDASH, joined);
  } else if (mount_n == 1) {
    buf_printf(&out, " mounted at %s", mounts[0]);
  } else if (mount_n > 1) {
    c8 *joined = join_strs(arena, mounts, mount_n, ", ");
    buf_printf(&out, " mounted at %s", joined);
  }
  return out.data;
}

static bool is_nvme_disk(
  const LsblkRow *row
) {
  if (row->tran && strcmp(row->tran, "nvme") == 0) return true;
  return row->name && strncmp(row->name, "nvme", 4) == 0;
}

c8 *format_disks(
  Arena *arena, const LsblkTable *table, const c8 *boot_disk
) {
  const LsblkRow *boot[MAX_LSBLK_ROWS];
  const LsblkRow *nvme[MAX_LSBLK_ROWS];
  const LsblkRow *storage[MAX_LSBLK_ROWS];
  usize           boot_n    = 0;
  usize           nvme_n    = 0;
  usize           storage_n = 0;

  if (!table) return arena_copy_str(arena, "");

  for (usize idx = 0; idx < table->count; idx++) {
    const LsblkRow *row = &table->rows[idx];
    if (!row_is_whole_disk(row)) continue;
    if (disk_name_skipped(row->name)) continue;
    bool is_boot = boot_disk && boot_disk[0] && strcmp(row->name, boot_disk) == 0;
    if (is_boot) {
      boot[boot_n++] = row;
    } else if (is_nvme_disk(row)) {
      nvme[nvme_n++] = row;
    } else {
      storage[storage_n++] = row;
    }
  }

  Buf out;
  buf_init(&out, arena);
  const LsblkRow *const *groups[] = {boot, nvme, storage};
  const usize            counts[] = {boot_n, nvme_n, storage_n};
  for (usize group = 0; group < 3; group++) {
    for (usize idx = 0; idx < counts[group]; idx++) {
      const LsblkRow *disk = groups[group][idx];
      const c8       *line = nullptr;
      if (group == 0)
        line = format_boot_drive(arena, table, disk);
      else if (group == 1)
        line = format_nvme(arena, table, disk);
      else
        line = format_storage(arena, table, disk);
      if (out.len) buf_append_char(&out, '\n');
      buf_append(&out, line);
    }
  }
  return out.data;
}

c8 *specs_build_doc(
  Arena *arena, const c8 *cpu, const c8 *ram, const c8 *gpu, const c8 *mobo, const c8 *disks, const c8 *swap, const c8 *network, const c8 *os
) {
  Buf doc;
  buf_init(&doc, arena);
  buf_printf(
    &doc, MDASH " CPU: %s\n" MDASH " RAM: %s\n" MDASH " GPU: %s\n" MDASH " Motherboard: %s\n", cpu ? cpu : "", ram ? ram : "", gpu ? gpu : "", mobo ? mobo : ""
  );
  if (disks && disks[0]) buf_printf(&doc, "%s\n", disks);
  buf_printf(&doc, MDASH " Swap: %s\n" MDASH " Network: %s\n" MDASH " OS: %s\n", swap ? swap : "", network ? network : "", os ? os : "");
  return doc.data;
}
