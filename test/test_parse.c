#define _GNU_SOURCE

#include "specs.h"
#include "test_framework.h"
#include "test_suites.h"

#include <stdio.h>
#include <string.h>

static void expect_str(
  const c8 *name, const c8 *got, const c8 *expected
) {
  if (strcmp(got ? got : "", expected ? expected : "") != 0) { printf("  got: [%s]\n  exp: [%s]\n", got ? got : "(null)", expected ? expected : "(null)"); }
  TEST(name, got && expected && strcmp(got, expected) == 0);
}

void run_parse_tests(
  void
) {
  TEST_PRINTLN("parse");

  u8    mem[1 << 18];
  Arena arena;
  arena_init(&arena, mem, sizeof(mem));

  expect_str("size_label 512G", size_label(&arena, "512G"), "512 GB");
  expect_str("size_label 512g", size_label(&arena, "512g"), "512 GB");
  expect_str("size_label 1.8T", size_label(&arena, "1.8T"), "1.8 TB");
  expect_str("size_label spaces", size_label(&arena, "  16 G  "), "16 GB");
  expect_str("size_label 500M", size_label(&arena, "500M"), "500 MB");
  expect_str("size_label 1024K", size_label(&arena, "1024K"), "1024 KB");
  expect_str("size_label no unit", size_label(&arena, "100"), "100");
  expect_str("size_label empty", size_label(&arena, ""), "");

  const c8 *line = "NAME=\"sda\" ROTA=\"1\" SIZE=\"1.8T\" TRAN=\"sata\" MOUNTPOINT=\"/data\"";
  expect_str("lsblk NAME", lsblk_field(&arena, line, "NAME"), "sda");
  expect_str("lsblk ROTA", lsblk_field(&arena, line, "ROTA"), "1");
  expect_str("lsblk SIZE", lsblk_field(&arena, line, "SIZE"), "1.8T");
  expect_str("lsblk missing", lsblk_field(&arena, line, "MODEL"), "");

  const c8 *escaped = "NAME=\"foo\\\"bar\" MOUNTPOINT=\"a\\nb\\tc\\\\d\"";
  expect_str("lsblk escaped quote", lsblk_field(&arena, escaped, "NAME"), "foo\"bar");
  c8 *mount = lsblk_field(&arena, escaped, "MOUNTPOINT");
  TEST("lsblk escaped newline", mount[0] == 'a' && mount[1] == '\n' && mount[2] == 'b');
  TEST("lsblk escaped tab", mount[3] == '\t' && mount[4] == 'c');
  TEST("lsblk escaped other", mount[5] == '\\' && mount[6] == 'd' && mount[7] == '\0');

  const c8 *lscpu = "Model name:                              AMD Ryzen 9 7950X 16-Core Processor\n"
                    "Core(s) per socket:                      16\n";
  expect_str("line_value model", line_value(&arena, lscpu, "Model name:"), "AMD Ryzen 9 7950X 16-Core Processor");
  expect_str("line_value missing", line_value(&arena, lscpu, "Bogus:"), "");

  const c8 *inxi = "System RAM: total: 32 GiB available: 20 GiB\n  type: DDR5";
  expect_str("word_after total", word_after(&arena, inxi, "total:"), "32");
  expect_str("word_after type", word_after(&arena, strstr(inxi, "type: DDR"), "type:"), "DDR5");

  const c8 *meminfo = "MemTotal:       32842728 kB\nMemAvailable:   20123456 kB\n";
  TEST("meminfo_kib total", meminfo_kib(&arena, meminfo, "MemTotal:") == 32842728);
  TEST("meminfo_kib avail", meminfo_kib(&arena, meminfo, "MemAvailable:") == 20123456);
  TEST("meminfo_kib missing", meminfo_kib(&arena, meminfo, "Hugepages:") == 0);

  const c8  *dump  = "NAME=\"nvme0n1\" ROTA=\"0\" SIZE=\"1.8T\" TRAN=\"nvme\" TYPE=\"disk\" PKNAME=\"\" PARTTYPENAME=\"\" FSTYPE=\"\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme0n1p1\" ROTA=\"0\" SIZE=\"100M\" TRAN=\"nvme\" TYPE=\"part\" PKNAME=\"nvme0n1\" "
                     "PARTTYPENAME=\"EFI System\" FSTYPE=\"vfat\" MOUNTPOINT=\"/boot\"\n";
  LsblkTable table = lsblk_parse(&arena, dump);
  TEST("lsblk_parse count", table.count == 2);
  TEST("lsblk_parse disk name", strcmp(table.rows[0].name, "nvme0n1") == 0);
  TEST("lsblk_parse child pkname", strcmp(table.rows[1].pkname, "nvme0n1") == 0);
  TEST("row_is_whole_disk disk", row_is_whole_disk(&table.rows[0]));
  TEST("row_is_whole_disk part", !row_is_whole_disk(&table.rows[1]));
  TEST("row_is_child", row_is_child(&table.rows[1], "nvme0n1"));
  TEST("disk_name_skipped zram", disk_name_skipped("zram0"));
  TEST("disk_name_skipped loop", disk_name_skipped("loop1"));
  TEST("disk_name_skipped ram", disk_name_skipped("ram0"));
  TEST("disk_name_skipped nvme", !disk_name_skipped("nvme0n1"));

  CpuFacts cpu = parse_cpu(&arena, lscpu, "1\n", 32);
  TEST("parse_cpu suffix strip", strcmp(cpu.model, "AMD Ryzen 9 7950X") == 0);
  TEST("parse_cpu cores", cpu.cores == 16);
  TEST("parse_cpu smt file", cpu.smt_on);

  const c8 *swaps = "Filename\t\t\t\tType\t\tSize\tUsed\tPriority\n"
                    "/dev/zram0\t\t\tpartition\t8388604\t0\t100\n"
                    "/swapfile\t\t\tfile\t\t2097152\t0\t-2\n";
  SwapTable st    = swaps_parse(&arena, swaps);
  TEST("swaps_parse count", st.count == 2);
  TEST("swaps_parse zram", strcmp(st.paths[0], "/dev/zram0") == 0);
  TEST("swap_path_active yes", swap_path_active(&st, "/dev/zram0"));
  TEST("swap_path_active no", !swap_path_active(&st, "/dev/sda2"));

  expect_str("trim_copy", trim_copy(&arena, "  hi  \n"), "hi");
  expect_str("first_line_copy", first_line_copy(&arena, "one\ntwo"), "one");

  const c8 *items[] = {"a", "b", "c"};
  expect_str("join_strs", join_strs(&arena, items, 3, " + "), "a + b + c");

  TEST_SUMMARY("parse");
}
