# 05: Status saat ini

**Last sync:** 23 Sep 2026 WITA, setelah firmware di-flash dan flash alat dikosongkan

## TL;DR

| Bagian | Status | Catatan |
|---|---|---|
| Firmware alat uji | **Jalan** | dipakai di tiga sesi kendaraan nyata sejak 8 Sep 2026 |
| Penegakan listen-only | **Selesai** | tiga lapis, gate build lolos |
| Dashboard web | **Jalan** | termasuk kolom nama identifier dan catatan berisi hasil hitung langsung |
| Aliran frame lewat WiFi | **Jalan** | menghapus kehilangan 14,8 % akibat penulisan flash |
| Pemetaan Honda HR-V | **Ada kandidat** | 8 Sep 2026, belum pernah diuji sambil berjalan |
| Pemetaan DFSK Gelora E | **Terbukti** | 23 Sep 2026, uji jalan 47 menit |
| Pemetaan Wuling armada | **Belum** | belum pernah ada sesi sama sekali |
| Rancangan papan sirkuit | **Ada, rev A** | `hardware/`, diperiksa mesin, belum pernah difabrikasi |
| Perbaikan FrameStream terakhir | **Terpasang** | di-flash 23 Sep 2026, hash terverifikasi |
| Penyaring identifier dan panel Vehicle | **Terpasang, belum teruji** | D-006. Butuh kendaraan: di meja `rx = 0`, tidak ada yang bisa disaring |
| Catatan identifier Gelora E di alat | **Terpasang** | 8 catatan, 5 bertanda `#tds` |
| Flash alat | **Kosong dan siap** | 13,2 MB ditarik jadi `gelora-005`, diverifikasi, dicadangkan, lalu dihapus |
| Arah perangkat: Orange Pi 5 | **Arah, belum keputusan** | D-007, empat hal belum dijawab |
| Cadangan rekaman uji jalan | **Belum** | ada arsip 8 MB di laptop, belum disalin keluar |

## Penghalang aktif

### 1. Penyaring dan panel Vehicle belum pernah melihat bus sungguhan

Keduanya sudah terpasang di alat, tetapi di meja `rx = 0` karena tidak ada bus CAN
yang tersambung, jadi tidak ada satu pun identifier untuk disaring atau ditampilkan.
Keduanya baru terbukti bekerja saat alat menyentuh kendaraan.

Kalau ternyata ada yang salah di sana, gejalanya akan seperti ini: tabel kosong
padahal bus ramai, atau panel Vehicle tidak muncul padahal catatan sudah bertanda.
Obatnya sementara: pindah penyaring ke **All**.

### 2. Kendaraan armada belum pernah dipetakan

Metode pemetaan sudah terbukti, tetapi terbukti pada **DFSK**. Armada memakai Wuling, dan
tidak satu pun identifier DFSK boleh dipakai untuk armada. Yang dibutuhkan adalah satu
unit Wuling armada dan satu kali sesi uji jalan.

Ini bukan lagi pertanyaan teknis. Metodenya siap, prosedurnya tertulis di
`docs/PROSEDUR-TEST-JALAN.md`, dan firmware tinggal ditambah satu env kendaraan. Yang
kurang adalah akses ke kendaraannya.

### 3. Tiga rekaman 22 September belum punya dokumen bukti

`gelora-002`, `gelora-003`, dan `gelora-005`, seluruhnya 630 ribu frame, tersimpan
dan sudah dicadangkan tetapi belum ada satu pun dokumen yang menelusurinya. Ketiganya
tumpang tindih dan merupakan bahan mentah di balik angka 14,8 % lawan 0,0 % di
`frame-loss.md`. Rinciannya di B-07 pada `../BACKLOG.md`.

## Yang sedang berjalan

Tidak ada plan aktif di `ProjectDocs/plans/`. Folder itu belum lahir, dan itu keadaan
yang sah: pekerjaan sejauh ini berbentuk sesi pengujian, bukan rangkaian tugas
terencana.

## Backlog

Semua yang tertunda, beserta apa yang membukanya, ada di satu tempat:
[`../BACKLOG.md`](../BACKLOG.md). Dua puluh tiga butir, dikelompokkan per sebab,
masing-masing dengan status Siap, Menunggu akses, Menunggu keputusan, atau
Ditahan sengaja.

## Hutang yang sudah diketahui

| Hutang | Akibat kalau dibiarkan |
|---|---|
| Rujukan `D-XXX` di kode dan `docs/` belum menyebut repositori asalnya | `D-006` berarti dua hal berbeda di dua repo, dan pembaca berikutnya akan menebak |
| Suhu controller `0x0CFF1601` b2 turun jadi dugaan | kalau dipakai, sistem armada menampilkan angka yang tidak pernah terbukti |
| Tautan WiFi pada sesi uji jalan putus 30 kali, 14,9 % waktu tidak terekam | rekaman berlubang tidak layak untuk analisis urutan frame |
| Papan rev A belum punya slot microSD | kehilangan frame saat merekam ke flash tetap ada sampai rev B |
| `gelora-002` dan `gelora-003` tersimpan tanpa dokumen bukti | 417 ribu frame yang tidak bisa ditelusuri siapa pun selain yang merekamnya |
