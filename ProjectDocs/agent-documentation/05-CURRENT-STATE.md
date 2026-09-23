# 05: Status saat ini

**Last sync:** 23 Sep 2026 WITA, setelah uji jalan DFSK Gelora E dan pembuatan `ProjectDocs/`

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
| Perbaikan FrameStream terakhir | **Belum di-flash** | sudah di-build dan di-commit, belum masuk alat |
| Penyaring identifier dan panel Vehicle | **Belum di-flash** | sudah di-build, D-006, `docs/DASHBOARD-TDS.md` |
| Arah perangkat: Orange Pi 5 | **Arah, belum keputusan** | D-007, empat hal belum dijawab |
| Cadangan rekaman uji jalan | **Belum** | ada arsip 8 MB di laptop, belum disalin keluar |

## Penghalang aktif

### 1. Firmware terbaru belum masuk alat

Commit `64cba89` memperbaiki penyebab paling mahal di sesi lapangan: aliran frame
memperlakukan `ENOMEM` dari lwIP sebagai sambungan mati, lalu menyambung ulang 338 kali
dalam 30 detik dan kehilangan sekitar 40 % bus. Perbaikannya sudah di-build untuk env
`gelora-e` dan gate listen-only lolos, tetapi **belum pernah di-flash** karena USB
dicabut sebelum sempat.

Selama itu belum dilakukan, setiap sesi mengalirkan frame masih membawa risiko yang sama.
Butuh satu sambungan USB.

Dalam build yang sama ikut menunggu: penyaring identifier tiga mode dan panel
Vehicle (D-006). Setelah di-flash, pasang nama identifiernya sekali jalan:

```
python tools/apply_notes.py --host <ip alat> docs/evidence/dfsk-gelora-e/notes.tsv
```

### 2. Kendaraan armada belum pernah dipetakan

Metode pemetaan sudah terbukti, tetapi terbukti pada **DFSK**. Armada memakai Wuling, dan
tidak satu pun identifier DFSK boleh dipakai untuk armada. Yang dibutuhkan adalah satu
unit Wuling armada dan satu kali sesi uji jalan.

Ini bukan lagi pertanyaan teknis. Metodenya siap, prosedurnya tertulis di
`docs/PROSEDUR-TEST-JALAN.md`, dan firmware tinggal ditambah satu env kendaraan. Yang
kurang adalah akses ke kendaraannya.

### 3. Rekaman uji jalan belum punya salinan di luar laptop

`captures/gelora-004` berisi 1,5 juta frame dari perjalanan 47 menit yang tidak bisa
diulang dengan kondisi yang sama. Arsip terkompresi 8 MB sudah dibuat di
`D:\Wahyu\BMT\gelora-004-backup-2026-09-23.zip` beserta daftar sidik jari SHA-256 per
berkas, tetapi belum disalin ke tempat lain.

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
