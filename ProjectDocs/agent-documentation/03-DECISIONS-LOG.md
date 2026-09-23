# 03: Decisions Log

Keputusan yang mengikat repo ini. Format:

## D-XXX: &lt;Judul&gt; (&lt;DD MMM YYYY HH:MM WITA&gt;)
**Source:** &lt;kanal dan pengirim, atau berkas yang memuatnya&gt;
> "&lt;kutipan literal, pertahankan typo, emoji, kapitalisasi&gt;"

**Konteks:** … / **Decision:** … / **Implementasi:** …

Nomor urut D-001 ke atas, tidak pernah dipakai ulang. Konvensi lengkapnya di skill
`bmt-decision-tracking`.

---

## PERINGATAN: nomor keputusan bertabrakan antar repositori

Tiga repositori BMT masing-masing punya deret `D-XXX` sendiri, dan nomornya **sudah
bertabrakan hari ini**. `D-006` berarti "feature freeze" di repo armada, dan berarti
"decode sekali di gateway" di `project-mdt-tds`. Keduanya dirujuk dari repo ini.

**Aturan: setiap rujukan keputusan wajib menyebut repositorinya.** Tulis
`D-015 (bmt-can-bus-telemetry)`, jangan nomor telanjang. Rujukan lama di dalam kode dan
`docs/` sebagian besar belum menyebut repo, dan itu dicatat sebagai hutang di
`09-TEMUAN-EVALUASI-PROSES.md`.

### Keputusan yang diwarisi, bukan milik repo ini

| Nomor | Repo asal | Judul | Kenapa mengikat di sini |
|---|---|---|---|
| D-003 | `bmt-can-bus-telemetry` | Milestone 1 dipecah M1A / M1B / M1C dengan acceptance berbasis evidence | bentuk bukti yang harus dihasilkan tiap sesi |
| D-006 | `bmt-can-bus-telemetry` | Feature freeze sampai M1A dan M1B PASS | alasan alat uji ini dibangun terpisah, bukan ditambahkan ke firmware armada |
| D-015 | `bmt-can-bus-telemetry` | Raw evidence immutable, terpisah dari interpretasi teknisi | aturan bukti yang paling sering dirujuk di repo ini, 13 kali |
| D-016 | `bmt-can-bus-telemetry` | Unfreeze sebagian D-006: WiFi dan dashboard diagnostik untuk tes Avanza | izin keberadaan WiFi dan dashboard di alat ini |
| D-001 | `project-mdt-tds` | MDT dan unit perekam adalah dua perangkat terpisah | alasan alat ini tidak pernah diberi layar |
| D-005 | `project-mdt-tds` | Survei armada dibatalkan, MDT punya antarmuka sendiri | dirujuk di `docs/PHASE-2-DASHBOARD.md` |
| D-007 | `project-mdt-tds` | Sistem ini untuk banyak jenis kendaraan | alasan ada satu env PlatformIO per kendaraan |
| D-008 | `project-mdt-tds` | Data ECU ke server: dua aliran yang tidak pernah dicampur | koreksi atas D-006 `project-mdt-tds` |

---

## D-001: Alat ini listen-only secara permanen, ditegakkan tiga lapis (6 Sep 2026 WITA)

**Source:** `README.md` repo ini, ditegakkan oleh `src/CanBusSafety.h` dan
`tools/check_listen_only.sh`

> "**The device is listen-only, permanently.** It reads the vehicle CAN bus and
> never writes to it"

Kalimat itu berlanjut dengan daftar yang membuatnya tidak bisa ditafsir longgar: tidak
ada data frame, tidak ada remote frame, tidak ada ACK, dan tidak ada error frame.

**Konteks:** alat ini dipasang ke kendaraan milik pihak lain, sebentar, lalu dicabut.
Satu firmware yang keliru mengirim ke bus bisa mengganggu kendaraan orang. Aturan yang
hanya ditulis di dokumen akan dilanggar cepat atau lambat oleh orang yang tidak membaca
dokumen itu.

**Decision:** penegakannya dipindah ke mesin, di tiga tempat yang saling bebas:
mode controller, `#pragma GCC poison` pada jalur kirim sehingga pemanggilnya gagal
dibuild, dan sebuah gate rilis yang dijalankan sebelum build menuju kendaraan.

**Implementasi:** `CanManager` menolak start pada mode selain `TWAI_MODE_LISTEN_ONLY`.
`tools/check_listen_only.sh` memeriksa ketiganya dan mengembalikan status gagal.
Jalankan gate itu sebelum setiap build yang akan mendekati kendaraan.

---

## D-002: Satu env PlatformIO per kendaraan, UNIT_ID tidak pernah diedit tangan (8 Sep 2026 WITA)

**Source:** `platformio.ini` repo ini

> "The vehicle about to be tested. Change this line, or pass -e on the command
> line; never edit UNIT_ID in Config.h by hand."

**Konteks:** `UNIT_ID` masuk ke header setiap berkas rekaman dan memilih folder catatan
di alat, jadi ia harus benar **sebelum** sebuah run dimulai, tidak bisa dibetulkan
sesudahnya (D-015 di `bmt-can-bus-telemetry`). Mengeditnya dengan tangan di `Config.h`
berarti satu langkah manual yang mudah terlupa di tempat parkir.

**Decision:** identitas kendaraan ditetapkan saat build lewat env PlatformIO, bukan lewat
suntingan berkas. Bitrate yang berbeda juga jadi env tersendiri.

**Implementasi:** `[env:hrv]`, `[env:gelora-e]`, `[env:gelora-e-250k]`. `Config.h`
menolak build kalau tidak ada kendaraan yang dipilih. Konvensi ini mengikuti D-007 di
`project-mdt-tds`.

---

## D-003: Frame dialirkan lewat WiFi, perekaman flash dijeda selama mengalirkan (22 Sep 2026 WITA, sesi Claude Code)

**Source:** `docs/evidence/frame-loss.md`, diukur pada DFSK Gelora E

> "**Karena penulisan flash itu sendiri penyebabnya, perekaman flash harus dijeda
> selama mengalirkan.**"

**Konteks:** panel Health menyatakan Overworked, dan pengukuran menunjukkan 14,8 % frame
hilang saat alat menulis ke flash sendiri melawan 0,0 % saat frame dialirkan ke laptop.
Penyebabnya interupsi CAN yang berada di flash. Membiarkan perekaman flash menyala
selama mengalirkan akan mengembalikan kehilangan yang justru ingin dihindari.

**Decision:** untuk sesi yang menuntut kelengkapan, frame dialirkan lewat WiFi ke laptop
yang ikut di kendaraan, dan perekaman flash dijeda selama itu.

**Implementasi:** `src/FrameStream.cpp` menyajikan aliran di port 3333.
`tools/stream_capture.py` menjeda perekaman flash di awal dan mengembalikannya di akhir,
termasuk saat prosesnya dihentikan paksa. Biayanya: sesi jadi bergantung pada kualitas
tautan WiFi, dan setiap putus ditandai `# LINK` di dalam berkas, tidak pernah ditutup
diam-diam.

---

## D-004: OTA dibatalkan, flashing lewat USB (23 Sep 2026 WITA, sesi Claude Code)

**Source:** operator BMT, sesi Claude Code

> "tidak jadi dengan ota, flash dengan usb saja, skenario dengan OTA batal, sudah saya colok usb"

**Konteks:** pembaruan lewat kabel menuntut alat dicabut dari kendaraan tiap kali
firmware berubah, dan itu merepotkan saat pengujian berlangsung. OTA sempat dikerjakan
sampai selesai untuk menghapus kerepotan itu.

**Decision:** OTA dibatalkan sepenuhnya. Pembaruan firmware dilakukan lewat USB.

**Implementasi:** commit `281e585` di-revert seutuhnya, bukan dinonaktifkan lewat flag.
Fitur yang tidak jadi dipakai tetapi tetap ada di kode adalah beban yang harus dirawat
tanpa memberi manfaat. Kata sandi OTA yang sempat dibuat berada di `src/Secrets.h` yang
di-gitignore dan tidak pernah dicetak.

---

## D-005: Tanpa sprint, laporan disusun per milestone (23 Sep 2026 WITA, sesi Claude Code)

**Source:** operator BMT, sesi Claude Code, saat menjalankan `bmt-project-onboarding`

> "Tanpa sprint, lapor per milestone"

**Konteks:** `bmt-project-onboarding` menuntut ritme laporan disepakati dan dicatat,
supaya `bmt-laporan-klien` tahu kapan harus jalan dan `bmt-sprint-close` tahu apakah ia
berlaku. Sampai 23 Sep 2026 project ini berjalan tanpa ritme yang pernah dinyatakan.

**Decision:** project ini tidak memakai sprint. Laporan disusun saat sebuah milestone
nyata tercapai, misalnya sebuah sinyal naik status jadi terbukti, satu kendaraan baru
selesai dipetakan, atau satu tahap perangkat keras selesai diukur.

**Implementasi:** `bmt-sprint-close` tidak berlaku di repo ini. Panen pelajaran dari
`09-TEMUAN-EVALUASI-PROSES.md` lewat `bmt-skill-evolution` dilakukan pada milestone.
Karena project ini internal BMT tanpa klien eksternal, laporan berarti ringkasan ke
operator BMT. Keputusan yang sama dicatat sebagai D-009 di `project-mdt-tds`.
