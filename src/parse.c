#define _GNU_SOURCE

#include "parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

ATTR_CONSTFN ATTR_ALWAYS_INLINE bool ascii_space(
  unsigned char code
) {
  return isspace(code) != 0;
}

c8 *trim_copy(
  Arena *arena, const c8 *text
) {
  if (!text) return arena_copy_str(arena, "");
  const c8 *start = text;
  while (*start && ascii_space((unsigned char)*start))
    start++;
  const c8 *end = start + strlen(start);
  while (end > start && ascii_space((unsigned char)end[-1]))
    end--;
  return arena_copy_n(arena, start, (usize)(end - start));
}

c8 *first_line_copy(
  Arena *arena, const c8 *text
) {
  if (!text) return arena_copy_str(arena, "");
  const c8 *nl = strchr(text, '\n');
  if (!nl) return arena_copy_str(arena, text);
  return arena_copy_n(arena, text, (usize)(nl - text));
}

c8 *size_label(
  Arena *arena, const c8 *input
) {
  Buf compact;
  buf_init(&compact, arena);
  if (input) {
    for (const c8 *cursor = input; *cursor; cursor++) {
      if (!ascii_space((unsigned char)*cursor)) buf_append_char(&compact, *cursor);
    }
  }
  if (compact.len == 0) return compact.data;

  c8        unit     = compact.data[compact.len - 1];
  const c8 *expanded = nullptr;
  switch ((c8)ascii_to_upper((unsigned char)unit)) {
    case 'T': expanded = "TB"; break;
    case 'G': expanded = "GB"; break;
    case 'M': expanded = "MB"; break;
    case 'K': expanded = "KB"; break;
    default : break;
  }
  if (!expanded) return compact.data;
  compact.data[--compact.len] = '\0';
  buf_printf(&compact, " %s", expanded);
  return compact.data;
}

static c8 g_empty[] = "";

// cppcheck-suppress staticFunction
c8 *lsblk_field(
  Arena *arena, const c8 *line, const c8 *key
) {
  if (UNLIKELY(!line || !key || !key[0])) return g_empty;
  usize     key_len = strlen(key);
  const c8 *cursor  = line;
  while ((cursor = strstr(cursor, key)) != nullptr) {
    bool at_start = (cursor == line) || ascii_space((unsigned char)cursor[-1]);
    if (at_start && cursor[key_len] == '=' && cursor[key_len + 1] == '"') {
      cursor += key_len + 2;
      Buf out;
      buf_init(&out, arena);
      while (*cursor && *cursor != '"') {
        if (*cursor == '\\' && cursor[1]) {
          cursor++;
          switch (*cursor) {
            case 'n': buf_append_char(&out, '\n'); break;
            case 't': buf_append_char(&out, '\t'); break;
            default : buf_append_char(&out, *cursor); break;
          }
        } else {
          buf_append_char(&out, *cursor);
        }
        cursor++;
      }
      return out.data;
    }
    cursor += key_len;
  }
  return g_empty;
}

c8 *line_value(
  Arena *arena, const c8 *text, const c8 *label
) {
  if (UNLIKELY(!text || !label)) return arena_copy_str(arena, "");
  usize     label_len = strlen(label);
  const c8 *cursor    = text;
  while (*cursor) {
    const c8 *end = strchr(cursor, '\n');
    if (!end) end = cursor + strlen(cursor);
    if ((usize)(end - cursor) >= label_len && strncmp(cursor, label, label_len) == 0) {
      const c8 *value = cursor + label_len;
      while (value < end && ascii_space((unsigned char)*value))
        value++;
      c8   *out = arena_copy_n(arena, value, (usize)(end - value));
      usize len = strlen(out);
      while (len && ascii_space((unsigned char)out[len - 1]))
        out[--len] = '\0';
      return out;
    }
    cursor = *end ? end + 1 : end;
  }
  return arena_copy_str(arena, "");
}

c8 *word_after(
  Arena *arena, const c8 *text, const c8 *marker
) {
  if (UNLIKELY(!text || !marker)) return arena_copy_str(arena, "");
  const c8 *found = strstr(text, marker);
  if (!found) return arena_copy_str(arena, "");
  found += strlen(marker);
  while (*found && ascii_space((unsigned char)*found))
    found++;
  const c8 *end = found;
  while (*end && !ascii_space((unsigned char)*end))
    end++;
  return arena_copy_n(arena, found, (usize)(end - found));
}

i64 meminfo_kib(
  Arena *arena, const c8 *text, const c8 *key
) {
  const c8 *value = line_value(arena, text, key);
  if (!value[0]) return 0;
  return strtol(value, nullptr, 10);
}

static void parse_lsblk_row(
  Arena *arena, const c8 *line, LsblkRow *row
) {
  row->name         = lsblk_field(arena, line, "NAME");
  row->type         = lsblk_field(arena, line, "TYPE");
  row->pkname       = lsblk_field(arena, line, "PKNAME");
  row->tran         = lsblk_field(arena, line, "TRAN");
  row->size         = lsblk_field(arena, line, "SIZE");
  row->rota         = lsblk_field(arena, line, "ROTA");
  row->parttypename = lsblk_field(arena, line, "PARTTYPENAME");
  row->fstype       = lsblk_field(arena, line, "FSTYPE");
  row->mountpoint   = lsblk_field(arena, line, "MOUNTPOINT");
}

LsblkTable lsblk_parse(
  Arena *arena, const c8 *text
) {
  LsblkTable table = {};
  if (!text || !text[0]) return table;
  usize guess = 0;
  for (const c8 *scan = text; *scan; scan++) {
    if (*scan == '\n') guess++;
  }
  if (text[strlen(text) - 1] != '\n') guess++;
  if (guess > MAX_LSBLK_ROWS) guess = MAX_LSBLK_ROWS;
  if (guess == 0) return table;
  table.rows = arena_alloc(arena, guess * sizeof(LsblkRow), ARENA_ALIGN);
  ZERO_MEMORY(table.rows, guess * sizeof(LsblkRow));

  PREFETCH(text);
  const c8 *cursor = text;
  while (*cursor && table.count < MAX_LSBLK_ROWS) {
    const c8 *end = strchr(cursor, '\n');
    if (!end) end = cursor + strlen(cursor);
    usize line_len = (usize)(end - cursor);
    if (line_len) {
      const c8 *line = arena_copy_n(arena, cursor, line_len);
      if (table.count >= guess) break;
      parse_lsblk_row(arena, line, &table.rows[table.count]);
      table.count++;
    }
    cursor = *end ? end + 1 : end;
  }
  return table;
}

static void skip_spaces(
  const c8 **cursor
) {
  while (**cursor && ascii_space((unsigned char)**cursor))
    (*cursor)++;
}

static const c8 *take_token(
  Arena *arena, const c8 **cursor
) {
  skip_spaces(cursor);
  const c8 *start = *cursor;
  while (**cursor && !ascii_space((unsigned char)**cursor))
    (*cursor)++;
  return arena_copy_n(arena, start, (usize)(*cursor - start));
}

IfaceTable iface_parse(
  Arena *arena, const c8 *text
) {
  IfaceTable table = {};
  table.rows       = arena_alloc(arena, MAX_IFACES * sizeof(IfaceRow), ARENA_ALIGN);
  ZERO_MEMORY(table.rows, MAX_IFACES * sizeof(IfaceRow));
  if (!text || !text[0]) return table;

  const c8 *cursor = text;
  while (*cursor && table.count < MAX_IFACES) {
    const c8 *end = strchr(cursor, '\n');
    if (!end) end = cursor + strlen(cursor);
    c8    line_buf[512];
    usize line_len = (usize)(end - cursor);
    if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
    COPY_MEMORY(line_buf, cursor, line_len);
    line_buf[line_len] = '\0';
    const c8 *scan     = line_buf;
    const c8 *name     = take_token(arena, &scan);
    const c8 *state    = take_token(arena, &scan);
    if (name[0] && state[0]) {
      table.rows[table.count].name  = name;
      table.rows[table.count].state = state;
      table.count++;
    }
    cursor = *end ? end + 1 : end;
  }
  return table;
}

SwapTable swaps_parse(
  Arena *arena, const c8 *text
) {
  SwapTable table = {};
  void     *block = arena_alloc(arena, MAX_LSBLK_ROWS * sizeof(*table.paths), ARENA_ALIGN);
  ZERO_MEMORY(block, MAX_LSBLK_ROWS * sizeof(*table.paths));
  table.paths = (const c8 **)block;
  if (!text || !text[0]) return table;

  const c8 *cursor = text;
  bool      header = true;
  while (*cursor && table.count < MAX_LSBLK_ROWS) {
    const c8 *end = strchr(cursor, '\n');
    if (!end) end = cursor + strlen(cursor);
    if (header) {
      header = false;
      cursor = *end ? end + 1 : end;
      continue;
    }
    const c8 *scan = cursor;
    while (scan < end && !ascii_space((unsigned char)*scan))
      scan++;
    if (scan > cursor) {
      table.paths[table.count] = arena_copy_n(arena, cursor, (usize)(scan - cursor));
      table.count++;
    }
    cursor = *end ? end + 1 : end;
  }
  return table;
}

static void strip_cpu_suffix(
  c8 *model
) {
  static const c8 *const suffixes[] = {"-Core Processor", " Processor"};
  usize                  model_len  = strlen(model);
  for (usize idx = 0; idx < ARRAY_SIZE(suffixes); idx++) {
    usize suffix_len = strlen(suffixes[idx]);
    if (model_len < suffix_len) continue;
    if (strcmp(model + model_len - suffix_len, suffixes[idx]) != 0) continue;
    if (idx == 0) {
      c8 *dash = model + model_len - suffix_len;
      while (dash > model && isdigit((unsigned char)dash[-1]))
        dash--;
      if (dash > model && dash[-1] == ' ') dash--;
      *dash = '\0';
    } else {
      model[model_len - suffix_len] = '\0';
    }
    return;
  }
}

CpuFacts parse_cpu(
  Arena *arena, const c8 *lscpu, const c8 *smt, i64 nproc
) {
  CpuFacts facts = {};
  c8      *model = line_value(arena, lscpu ? lscpu : "", "Model name:");
  strip_cpu_suffix(model);
  facts.model         = model;

  const c8 *cores_s   = line_value(arena, lscpu ? lscpu : "", "Core(s) per socket:");
  const c8 *sockets_s = line_value(arena, lscpu ? lscpu : "", "Socket(s):");
  const c8 *tpc_s     = line_value(arena, lscpu ? lscpu : "", "Thread(s) per core:");
  const c8 *max_mhz   = line_value(arena, lscpu ? lscpu : "", "CPU max MHz:");

  facts.cores         = cores_s[0] ? (i32)strtol(cores_s, nullptr, 10) : 1;
  facts.sockets       = sockets_s[0] ? (i32)strtol(sockets_s, nullptr, 10) : 1;
  facts.threads       = nproc < 1 ? 1 : (i32)nproc;
  i32 tpc             = tpc_s[0] ? (i32)strtol(tpc_s, nullptr, 10) : 1;
  facts.smt_on        = tpc != 1 ? 1 : 0;
  if (smt && smt[0]) facts.smt_on = smt[0] == '1' ? 1 : 0;

  if (max_mhz[0]) {
    facts.has_boost = 1;
    facts.boost_ghz = (f32)(strtod(max_mhz, nullptr) / 1000.0);
  }
  return facts;
}

bool disk_name_skipped(
  const c8 *name
) {
  if (!name || !name[0]) return true;
  if (name[0] == 'z' && strncmp(name, SKIP_ZRAM, SKIP_ZRAM_LEN) == 0) return true;
  if (name[0] == 'l' && strncmp(name, SKIP_LOOP, SKIP_LOOP_LEN) == 0) return true;
  if (name[0] == 'r' && strncmp(name, SKIP_RAM, SKIP_RAM_LEN) == 0) return true;
  return false;
}

bool row_is_whole_disk(
  const LsblkRow *row
) {
  if (!row || !row->name || !row->name[0]) return false;
  if (row->type && strcmp(row->type, "disk") == 0) return true;
  if (row->type && strcmp(row->type, "part") == 0) return false;
  if (row->pkname && row->pkname[0]) return false;
  return true;
}

bool row_is_child(
  const LsblkRow *row, const c8 *disk_name
) {
  if (!row || !disk_name || !row->name) return false;
  if (strcmp(row->name, disk_name) == 0) return false;
  if (row->pkname && row->pkname[0]) return strcmp(row->pkname, disk_name) == 0;
  usize disk_len = strlen(disk_name);
  if (strncmp(row->name, disk_name, disk_len) != 0) return false;
  c8 next = row->name[disk_len];
  return next == 'p' || (next >= '0' && next <= '9');
}

bool swap_path_active(
  const SwapTable *table, const c8 *path
) {
  if (!table || !path) return false;
  usize path_len = strlen(path);
  for (usize idx = 0; idx < table->count; idx++) {
    const c8 *entry = table->paths[idx];
    if (entry && strlen(entry) == path_len && strcmp(entry, path) == 0) return true;
  }
  return false;
}

c8 *join_strs(
  Arena *arena, const c8 *const *items, usize count, const c8 *sep
) {
  Buf out;
  buf_init(&out, arena);
  for (usize idx = 0; idx < count; idx++) {
    if (idx) buf_append(&out, sep);
    buf_append(&out, items[idx] ? items[idx] : "");
  }
  return out.data;
}
