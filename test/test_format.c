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

void run_format_tests(
  void
) {
  TEST_PRINTLN("format");

  u8    mem[1 << 18];
  Arena arena;
  arena_init(&arena, mem, sizeof(mem));

  const c8 *lscpu = "Model name:                              AMD Ryzen 9 7950X 16-Core Processor\n"
                    "Core(s) per socket:                      16\n"
                    "Socket(s):                               1\n"
                    "Thread(s) per core:                      2\n"
                    "CPU max MHz:                             5759.0000\n";
  expect_str("cpu boost smt on", format_cpu(&arena, lscpu, "1\n", 32), "AMD Ryzen 9 7950X " MDASH " 16 cores / 32 threads (SMT on), boosting to ~5.8GHz");

  const c8 *lscpu_intel = "Model name: Intel(R) Core(TM) i7-14700K Processor\n"
                          "Core(s) per socket: 8\n"
                          "Socket(s): 1\n"
                          "Thread(s) per core: 1\n";
  expect_str("cpu no boost smt off", format_cpu(&arena, lscpu_intel, "", 8), "Intel(R) Core(TM) i7-14700K " MDASH " 8 cores / 8 threads (SMT off)");

  TEST("cpu smt file overrides tpc", strstr(format_cpu(&arena, lscpu_intel, "1", 8), "SMT on") != nullptr);

  const c8 *meminfo = "MemTotal:       32842728 kB\nMemAvailable:   20123456 kB\n";
  expect_str("ram rounded no inxi", format_ram(&arena, meminfo, ""), "32 GiB (~19 GiB available)");
  expect_str("ram with ddr", format_ram(&arena, meminfo, "System RAM: total: 32 GiB available: 20 GiB\n  type: DDR5"), "32 GiB DDR5 (~19 GiB available)");

  expect_str("gpu nvidia-smi", format_gpu(&arena, "NVIDIA GeForce RTX 4090, 24564 MiB\n", ""), "NVIDIA GeForce RTX 4090 " MDASH " 23 GB VRAM");

  const c8 *lspci = "00:00.0 Host bridge: Intel Corporation Device\n"
                    "01:00.0 VGA compatible controller: NVIDIA Corporation Device [GeForce RTX 4090] (rev a1)\n";
  expect_str("gpu lspci nvidia fallback", format_gpu(&arena, "Failed to initialize NVML\n", lspci), "NVIDIA GeForce RTX 4090 " MDASH " 24 GB VRAM");

  const c8 *lspci_intel = "00:00.0 Host bridge: Intel Corporation Device\n"
                          "00:02.0 VGA compatible controller: Intel Corporation Alder Lake-S GT1 (rev 0c)\n";
  expect_str("gpu lspci vga not first line", format_gpu(&arena, "", lspci_intel), "Intel Corporation Alder Lake-S GT1");
  expect_str("gpu unknown", format_gpu(&arena, "", ""), "unknown");

  expect_str("mobo both", format_mobo(&arena, "ASUSTeK COMPUTER INC.\n", "ROG STRIX\n"), "ASUSTeK COMPUTER INC. ROG STRIX");
  expect_str("mobo unknown", format_mobo(&arena, "\n", ""), "unknown");

  expect_str("os quoted", format_os(&arena, "NAME=\"Arch Linux\"\n", "6.8.1-arch1-1\n"), "Arch Linux, kernel 6.8.1-arch1-1");
  expect_str("os empty name", format_os(&arena, "ID=arch\n", "6.8.1"), "Linux, kernel 6.8.1");

  const c8 *ip_mixed = "lo               UNKNOWN        00:00:00:00:00:00\n"
                       "wlan0            UP             aa:aa:aa:aa:aa:aa\n"
                       "eth0             UP             bb:bb:bb:bb:bb:bb\n"
                       "enp2s0           DOWN           cc:cc:cc:cc:cc:cc\n"
                       "tailscale0       UNKNOWN        dd:dd:dd:dd:dd:dd\n"
                       "wg0              UNKNOWN        ee:ee:ee:ee:ee:ee\n";
  expect_str("network mixed", format_network(&arena, ip_mixed), "Wi-Fi (wlan0, UP), Ethernet (eth0 UP, enp2s0 DOWN), Tailscale (tailscale0), WireGuard (wg0)");

  const c8 *ip_same = "eth0 UP\nenp2s0 UP\n";
  expect_str("network Nx ethernet", format_network(&arena, ip_same), "2x Ethernet (eth0/enp2s0, UP)");
  expect_str("network one eth", format_network(&arena, "eth0 UP\n"), "Ethernet (eth0, UP)");
  expect_str("network none", format_network(&arena, "lo DOWN\n"), "none");

  const c8 *lsblk  = "NAME=\"nvme0n1\" ROTA=\"0\" SIZE=\"1.8T\" TRAN=\"nvme\" TYPE=\"disk\" PKNAME=\"\" PARTTYPENAME=\"\" FSTYPE=\"\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme0n1p1\" ROTA=\"0\" SIZE=\"100M\" TRAN=\"nvme\" TYPE=\"part\" PKNAME=\"nvme0n1\" PARTTYPENAME=\"EFI "
                     "System\" FSTYPE=\"vfat\" MOUNTPOINT=\"/boot\"\n"
                     "NAME=\"nvme0n1p2\" ROTA=\"0\" SIZE=\"16G\" TRAN=\"nvme\" TYPE=\"part\" PKNAME=\"nvme0n1\" PARTTYPENAME=\"\" "
                     "FSTYPE=\"swap\" MOUNTPOINT=\"[SWAP]\"\n"
                     "NAME=\"nvme0n1p3\" ROTA=\"0\" SIZE=\"1.7T\" TRAN=\"nvme\" TYPE=\"part\" PKNAME=\"nvme0n1\" PARTTYPENAME=\"\" "
                     "FSTYPE=\"ext4\" MOUNTPOINT=\"/\"\n"
                     "NAME=\"nvme1n1\" ROTA=\"0\" SIZE=\"512G\" TRAN=\"nvme\" TYPE=\"disk\" PKNAME=\"\" PARTTYPENAME=\"\" FSTYPE=\"\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme1n1p1\" ROTA=\"0\" SIZE=\"100M\" TYPE=\"part\" PKNAME=\"nvme1n1\" PARTTYPENAME=\"EFI System\" "
                     "FSTYPE=\"\" MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme1n1p2\" ROTA=\"0\" SIZE=\"16M\" TYPE=\"part\" PKNAME=\"nvme1n1\" PARTTYPENAME=\"Microsoft reserved\" "
                     "FSTYPE=\"\" MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme1n1p3\" ROTA=\"0\" SIZE=\"400G\" TYPE=\"part\" PKNAME=\"nvme1n1\" PARTTYPENAME=\"Microsoft basic data\" "
                     "FSTYPE=\"\" MOUNTPOINT=\"\"\n"
                     "NAME=\"nvme1n1p4\" ROTA=\"0\" SIZE=\"1G\" TYPE=\"part\" PKNAME=\"nvme1n1\" PARTTYPENAME=\"Windows recovery "
                     "environment\" FSTYPE=\"\" MOUNTPOINT=\"\"\n"
                     "NAME=\"sda\" ROTA=\"1\" SIZE=\"4T\" TRAN=\"sata\" TYPE=\"disk\" PKNAME=\"\" PARTTYPENAME=\"\" FSTYPE=\"\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"sda1\" ROTA=\"1\" SIZE=\"4T\" TYPE=\"part\" PKNAME=\"sda\" PARTTYPENAME=\"\" FSTYPE=\"ext4\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"sdb\" ROTA=\"0\" SIZE=\"512G\" TRAN=\"sata\" TYPE=\"disk\" PKNAME=\"\" PARTTYPENAME=\"\" FSTYPE=\"\" "
                     "MOUNTPOINT=\"\"\n"
                     "NAME=\"sdb1\" ROTA=\"0\" SIZE=\"512G\" TYPE=\"part\" PKNAME=\"sdb\" PARTTYPENAME=\"\" FSTYPE=\"ext4\" "
                     "MOUNTPOINT=\"/data\"\n"
                     "NAME=\"zram0\" ROTA=\"0\" SIZE=\"8G\" TYPE=\"disk\" PKNAME=\"\" FSTYPE=\"swap\" MOUNTPOINT=\"[SWAP]\"\n";

  LsblkTable table = lsblk_parse(&arena, lsblk);
  c8        *disks = format_disks(&arena, &table, "nvme0n1");
  TEST("disks boot line", strstr(disks, "- Boot drive: 1.8 TB SSD (nvme0n1)") != nullptr);
  TEST("disks boot mounts exclude swap", strstr(disks, "100 MB /boot, 1.7 TB /") != nullptr);
  TEST("disks boot no swap mount", strstr(disks, "[SWAP]") == nullptr);
  TEST("disks nvme windows", strstr(disks, "- NVMe: 512 GB (nvme1n1) " MDASH " Windows partition layout (EFI + MSR + main + recovery)") != nullptr);
  TEST("disks storage hdd unmounted", strstr(disks, "- Storage HDD: 4 TB (sda) " MDASH " 4 TB partitions, unmounted") != nullptr);
  TEST("disks storage ssd mounted", strstr(disks, "- Storage SSD: 512 GB (sdb) mounted at /data") != nullptr);
  TEST("disks skip zram", strstr(disks, "zram") == nullptr);
  TEST("disks order boot before nvme", strstr(disks, "Boot drive") < strstr(disks, "- NVMe:"));
  TEST("disks order nvme before storage", strstr(disks, "- NVMe:") < strstr(disks, "- Storage HDD:"));

  const c8 *swaps = "Filename Type Size Used Priority\n/dev/zram0 partition 1 0 100\n";
  expect_str("swap zram active", format_swap(&arena, swaps, &table, nullptr, 0), "16 GB disk swap (active), 8 GB zram (active)");
  expect_str("swap none", format_swap(&arena, "Filename\n", nullptr, nullptr, 0), "none");

  SwapFileHint hint = {.path = "/swapfile", .size = "2.0G"};
  TEST("swap file hint", strstr(format_swap(&arena, "Filename\n", nullptr, &hint, 1), "2.0 GB disk swap (active)") != nullptr);

  c8 *doc = specs_build_doc(&arena, "cpu-frag", "ram-frag", "gpu-frag", "mobo-frag", "- Boot drive: x", "none", "Ethernet (eth0, UP)", "Arch Linux, kernel 1");
  TEST("doc CPU header", strstr(doc, MDASH " CPU: cpu-frag\n") != nullptr);
  TEST("doc RAM header", strstr(doc, MDASH " RAM: ram-frag\n") != nullptr);
  TEST("doc GPU header", strstr(doc, MDASH " GPU: gpu-frag\n") != nullptr);
  TEST("doc mobo header", strstr(doc, MDASH " Motherboard: mobo-frag\n") != nullptr);
  TEST("doc disk line", strstr(doc, "- Boot drive: x\n") != nullptr);
  TEST("doc Swap header", strstr(doc, MDASH " Swap: none\n") != nullptr);
  TEST("doc Network header", strstr(doc, MDASH " Network: Ethernet (eth0, UP)\n") != nullptr);
  TEST("doc OS header", strstr(doc, MDASH " OS: Arch Linux, kernel 1\n") != nullptr);

  const c8 *cpu_at = strstr(doc, MDASH " CPU:");
  const c8 *ram_at = strstr(doc, MDASH " RAM:");
  const c8 *gpu_at = strstr(doc, MDASH " GPU:");
  const c8 *mb_at  = strstr(doc, MDASH " Motherboard:");
  const c8 *dk_at  = strstr(doc, "- Boot drive:");
  const c8 *sw_at  = strstr(doc, MDASH " Swap:");
  const c8 *nt_at  = strstr(doc, MDASH " Network:");
  const c8 *os_at  = strstr(doc, MDASH " OS:");
  TEST(
    "doc section order",
    cpu_at
      && ram_at
      && gpu_at
      && mb_at
      && dk_at
      && sw_at
      && nt_at
      && os_at
      && cpu_at < ram_at
      && ram_at < gpu_at
      && gpu_at < mb_at
      && mb_at < dk_at
      && dk_at < sw_at
      && sw_at < nt_at
      && nt_at < os_at
  );

  TEST_SUMMARY("format");
}
