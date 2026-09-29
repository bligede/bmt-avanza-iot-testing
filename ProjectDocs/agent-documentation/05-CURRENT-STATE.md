# 05: Status saat ini

**Last sync:** 29 Sep 2026 WITA, setelah firmware masuk ke alat dan tepat sebelum uji jalan

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
| Decoder sinyal di firmware | **Di alat** | 8 pemetaan terbukti, D-008, di-flash 28 Sep |
| Argometer di alat | **Di alat** | bilangan bulat, tunggu stopwatch, D-010 |
| Layar SELARIDE di `/argo` | **Di alat** | tombol trip dipencet manual, teruji lewat WiFi |
| Panel Vehicle susunan cluster | **Di alat** | D-009, penempatan dari satuan |
| Alat tiruan `tools/fake_device.py` | **Jalan** | kedua halaman terbukti di meja, tanpa alat |
| Rekaman lama di flash alat | **13,3 MB, belum ditarik** | B-28, 60 berkas, 421 KB tersisa saat ditemukan |
| Kirim data ke server BMT | **Belum dimulai** | sengaja, urutannya setelah uji jalan (D-008) |

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

### 3. Firmware sudah di alat, tetapi belum pernah menyentuh bus sungguhan

Di-flash 28 September (D-010), diperiksa lewat WiFi dari meja: kedua halaman menjawab,
`unit=GELORAE-TEST-01`, hash `firmware.bin` terverifikasi saat upload. Aritmetika tarif
dan pemecahan rumus di panel Vehicle sebelumnya sudah terbukti di meja lawan
`tools/fake_device.py`, yang mengkodekan nilai tiruannya kembali menjadi byte CAN
persis seperti Gelora E mengirimnya.

Yang belum terbukti sama sekali: beban CPU alat, `rx_overrun` saat dua gawai memoll dua
halaman serentak, dan bagaimana ponsel berlaku di kendaraan bergerak. Tidak ada satu pun
dari itu yang bisa diukur tanpa bus sungguhan, dan itulah pokok uji jalan hari ini.

**IP alat tidak tetap.** Ia menerima alamat dari DHCP hotspot dan hanya mengenal satu
SSID, dikompilasi ke `src/Secrets.h`. Kalau hotspot berganti atau alat reboot di
tengah uji, alamat lama berhenti menjawab tanpa peringatan. Cara paling pasti mendapat
alamat baru: colok USB, ketik `wifi` di konsol. **Jangan** buka port serial dengan
`pyserial` default; DTR/RTS bawaannya me-reset ESP32.

### 4. 13,3 MB rekaman lama masih di flash alat

Ditemukan saat sesi flashing 28 September: 60 berkas, sisa ruang hanya 421 KB, empat
berkas terakhir 0 byte karena sudah pernah kehabisan tempat di tengah perekaman.
`clearcaptures` **tidak dijalankan**. Rinciannya di B-28 pada `../BACKLOG.md`.

Ini tidak menghalangi uji jalan hari ini kalau perekaman dimatikan dulu (`capture off`
di konsol, atau lewat dashboard teknis), tetapi menghalangi **total** sesi pemetaan
berikutnya begitu ruang yang tersisa habis.

### 5. Dua sumber rumus yang harus diawasi di jalan

Nilai di **panel Vehicle** dihitung dari rumus di catatan identifier, di dalam alat, dan
bisa diubah siapa pun dari halaman itu. Nilai di **layar argo** dihitung tabel di
`src/SignalDecoder.cpp`, yang hanya berubah lewat build baru. Keduanya berasal dari bukti
yang sama, jadi seharusnya selalu sama. Kalau berbeda di jalan: catat keduanya, jangan
diperbaiki di tempat.

### 5. Tiga rekaman 22 September belum punya dokumen bukti

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
