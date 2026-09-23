# 00: Mulai di sini

**Last updated:** 23 Sep 2026 WITA, saat `ProjectDocs/` pertama kali dibuat

## Ringkasan TL;DR

**bmt-avanza-iot-testing** adalah firmware alat uji diagnostik CAN milik BMT. ESP32-S3
plus transceiver SN65HVD230, dicolok ke OBD-II kendaraan, menyalakan hotspot dan
menyajikan dashboard web. Tugasnya **membaca** bus CAN kendaraan dan memetakan byte mana
yang berarti apa.

**Status hari ini: firmware jalan dan sudah dipakai di tiga sesi kendaraan nyata.**
Metode pemetaan sinyal sudah terbukti pada DFSK Gelora E lewat uji jalan 23 Sep 2026.
Yang belum: kendaraan armada (Wuling) belum pernah dipetakan sama sekali.

Alat ini **bukan** produk armada. Firmware armada ada di `bmt-can-bus-telemetry` dan
dibekukan. Konsumen data ini adalah `project-mdt-tds`.

## Urutan baca

| Berkas | Isi | Perkiraan waktu |
|---|---|---|
| `00-START-HERE.md` | berkas ini | 5 menit |
| `01-CONTEXT-PROJECT.md` | siapa yang memutuskan, tujuan, batas cakupan | 5 menit |
| `02-DOMAIN-KNOWLEDGE.md` | aturan domain CAN yang sudah dibayar mahal | 12 menit |
| `03-DECISIONS-LOG.md` | keputusan repo ini, dan yang diwarisi dari dua repo lain | 10 menit |
| `04-TECHNICAL-ARCHITECTURE.md` | peta environment, cara build, cara jalan, lokasi rahasia | 10 menit |
| `05-CURRENT-STATE.md` | status aktual dan penghalang aktif | 4 menit |
| `08-HANDOFF-CHECKLIST.md` | **langkah berikutnya, dan apa yang JANGAN dikerjakan** | 8 menit |
| `09-TEMUAN-EVALUASI-PROSES.md` | temuan proses selama project berjalan | 6 menit |

Total sekitar 60 menit. `06-COMMUNICATION-LOG.md` kosong dan menjelaskan sendiri kenapa.
`07-SCHEMA-MIGRATION.md` belum lahir karena project ini tidak punya basis data.

**Jangan berhenti di sini.** Isi teknisnya ada di `docs/`, dan `ProjectDocs/` sengaja
tidak menyalinnya. Yang wajib dibaca sebelum menyentuh kendaraan:

| Berkas | Kapan dibaca |
|---|---|
| `docs/RUN-PROCEDURE.md` | sebelum perekaman apa pun |
| `docs/PROSEDUR-TEST-JALAN.md` | sebelum uji sambil berjalan |
| `docs/evidence/00-umum/frame-loss.md` | sebelum memercayai kelengkapan sebuah rekaman |
| `docs/evidence/dfsk-gelora-e/gelora-004.md` | contoh pembuktian pemetaan yang lengkap |

## Kendala yang mengikat

1. **Alat ini listen-only, permanen.** Ditegakkan di tiga tempat yang saling bebas:
   mode controller, `#pragma GCC poison` pada jalur kirim sehingga pemanggilnya gagal
   dibuild, dan `tools/check_listen_only.sh` sebagai gate rilis. **Jangan pernah**
   menambahkan jalur kirim, walau untuk satu tes. Nilai alat ini justru ada pada
   jaminan itu.
2. **Rekaman mentah tidak pernah diedit setelah tes, dan kegagalan tidak pernah
   dihapus.** Interpretasi teknisi disimpan terpisah dari rekaman. Ini D-015 di repo
   armada, dan berlaku penuh di sini.
3. **Rekaman ini sampel, bukan salinan utuh bus.** Interupsi CAN berada di flash, jadi
   setiap penulisan flash mematikan cache instruksi dan frame hilang. Angkanya terukur:
   14,8 % saat menulis ke flash, 0 % saat dialirkan lewat WiFi. Analisis yang bergantung
   pada urutan antar-frame tidak boleh memakai rekaman dari alat ini. Lihat
   `docs/evidence/00-umum/frame-loss.md`.
4. **Identifier tidak pernah dipakai lintas kendaraan.** Honda HR-V, DFSK Gelora E, dan
   Wuling armada adalah platform yang berbeda sepenuhnya. Satu pun ID dari sini tidak
   boleh masuk profil sinyal armada.
5. **Sinyal yang tidak pernah bergerak tidak boleh dinyatakan valid.** Suhu controller
   Gelora E lolos sebagai kandidat saat kendaraan diam, lalu gugur setelah 47 menit
   berkendara tidak mengubahnya sedikit pun.

## Berkas dan lokasi penting

| Apa | Di mana |
|---|---|
| Repo ini | `D:\Wahyu\BMT\bmt-avanza-iot-testing`, yaitu `github.com/bligede/bmt-avanza-iot-testing` (privat) |
| Firmware armada, dibekukan | `D:\Wahyu\BMT\bmt-can-bus-telemetry` |
| Konsumen data ini | `D:\Wahyu\BMT\project-mdt-tds` |
| Decision log yang diwarisi | `bmt-can-bus-telemetry/agent-documentation/03-DECISIONS-LOG.md` |
| Kredensial WiFi | `src/Secrets.h`, di-gitignore. Templatnya `src/Secrets.h.example` |
| Rekaman mentah | `captures/<run>/`, tidak ikut repositori karena besar |

## Tindakan berikutnya

**Flash firmware yang sudah dibangun tapi belum masuk alat.** Perbaikan `FrameStream`
(commit `64cba89`) sudah lolos gate listen-only dan sudah di-build untuk env `gelora-e`,
tetapi **belum pernah masuk ke perangkat** karena USB dicabut sebelum sempat. Sampai itu
dilakukan, alat di lapangan masih membawa firmware yang memutus sambungan setiap kali
lwIP kehabisan buffer.

Sesudah itu, dua hal berurutan ada di `08-HANDOFF-CHECKLIST.md`.

## Ritme kerja

Project ini **tidak memakai sprint** (D-005 repo ini). Laporan disusun saat sebuah
milestone nyata tercapai, misalnya sebuah sinyal naik status jadi terbukti atau satu
kendaraan baru selesai dipetakan.

## Anti-pattern khas project ini

1. **Menambahkan kemampuan mengirim ke bus.** Lihat kendala nomor 1. Tidak ada
   pengecualian, termasuk "hanya untuk satu tes".
2. **Menyunting berkas rekaman.** Catatan dan tafsiran punya tempat sendiri di
   `/notes/<UNIT_ID>/`. Rekaman yang sudah disunting tidak bisa diaudit lagi.
3. **Memakai rekaman yang berlubang untuk analisis urutan frame.** Lubangnya ditandai,
   tetapi frame yang hilang di tengah tidak menyisakan jejak apa pun di berkas.
4. **Menyatakan sebuah pemetaan terbukti karena angkanya cocok satu kali.** Yang
   membuktikan adalah nilai yang **bergerak** mengikuti kendaraan.
5. **Menyalin isi `docs/` ke `ProjectDocs/`.** Dua salinan akan berselisih. `ProjectDocs/`
   merutekan, `docs/` yang memuat.
