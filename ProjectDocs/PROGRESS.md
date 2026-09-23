# PROGRESS: Alat Diagnostik CAN BMT

**Last updated:** 23 Sep 2026 WITA

Status yang dipakai: `Belum` · `Jalan` · `Selesai` · `Terhalang` · `Batal`

---

## Phase 0: Alat bisa membaca bus kendaraan

| Task | Status | Tanggal | Catatan |
|---|---|---|---|
| Firmware dasar ESP32-S3 dan transceiver | Selesai | 6 Sep 2026 | repo dimulai |
| Penegakan listen-only tiga lapis | Selesai | 6 Sep 2026 | mode controller, poison, gate build |
| Dashboard web di alat | Selesai | 8 Sep 2026 | port 80, hotspot sendiri |
| Uji di kendaraan sungguhan | Selesai | 8 Sep 2026 | Honda HR-V 2023, 500 kbps, run `hrv-001` |
| GNSS dan sensor tambahan | Selesai | 8 Sep 2026 | jalan pada perangkat keras pertama |

## Phase 1: Memetakan sinyal pada kendaraan listrik

| Task | Status | Tanggal | Catatan |
|---|---|---|---|
| Menentukan bitrate DFSK Gelora E | Selesai | 22 Sep 2026 | 250 kbps, setelah 500 kbps gagal total |
| Pemetaan saat kendaraan diam | Selesai | 22 Sep 2026 | 12 nilai, `docs/evidence/gelora-001.md` |
| Menemukan sebab kehilangan frame | Selesai | 22 Sep 2026 | interupsi CAN di flash, `docs/evidence/frame-loss.md` |
| Aliran frame lewat WiFi | Selesai | 22 Sep 2026 | 14,8 % hilang jadi 0,0 % |
| Prosedur uji jalan | Selesai | 22 Sep 2026 | `docs/PROSEDUR-TEST-JALAN.md` |
| Uji jalan dan pembuktian | Selesai | 23 Sep 2026 | 47 menit, `docs/evidence/gelora-004.md` |
| Perbaikan `ENOMEM` pada aliran frame | **Jalan** | 23 Sep 2026 | sudah di-build dan di-commit, **belum di-flash** |
| Cadangan rekaman uji jalan ke luar laptop | **Belum** | - | arsip 8 MB sudah dibuat, belum disalin |

## Phase 2: Memetakan kendaraan armada

| Task | Status | Tanggal | Catatan |
|---|---|---|---|
| Akses ke satu unit Wuling armada | **Terhalang** | - | menunggu penjadwalan operator BMT |
| Env PlatformIO untuk kendaraan armada | Belum | - | mengikuti pola `[env:gelora-e]` |
| Menentukan bitrate armada | Belum | - | tidak boleh diasumsikan |
| Pemetaan sinyal armada | Belum | - | penghalang utama bagi `project-mdt-tds` |
| Uji jalan Honda HR-V | Belum | - | kandidat 8 Sep belum pernah diuji bergerak |

## Phase 3: Perangkat keras

| Task | Status | Tanggal | Catatan |
|---|---|---|---|
| Skematik rev A dari satu sumber tunggal | Selesai | 20 Sep 2026 | `hardware/gen_schematic.py`, 61 komponen, 37 net |
| Pemeriksa skematik otomatis | Selesai | 20 Sep 2026 | 12 aturan keselamatan, diuji dengan tiga cacat suntikan |
| Putuskan rev A difabrikasi atau langsung rev B | **Belum** | - | rev B menambah slot microSD |
| Fabrikasi papan | Belum | - | menunggu keputusan di atas |

## Phase 4: Serah terima ke hilir

| Task | Status | Tanggal | Catatan |
|---|---|---|---|
| Batasan rancangan untuk perangkat armada | Selesai | 23 Sep 2026 | delapan butir, di `project-mdt-tds` berkas `08` |
| Skeleton `ProjectDocs/` | Selesai | 23 Sep 2026 | berkas ini dan `agent-documentation/` |
| Profil sinyal armada siap pakai | **Terhalang** | - | menunggu Phase 2 |
