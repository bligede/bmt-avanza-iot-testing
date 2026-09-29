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

**Source:** `docs/evidence/00-umum/frame-loss.md`, diukur pada DFSK Gelora E

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

---

## D-006: Dashboard menyaring apa yang ditampilkan, bukan apa yang direkam (23 Sep 2026 WITA, sesi Claude Code)

**Source:** operator BMT, sesi Claude Code

> "sebelum flash, jangan tampilkan frame yang tidak digunakan atau tidak diperlukan dalam aplikasi taxi dispatch system (TDS) dan frame yang tidak berhasil kamu petakan. dengan harapan agar tidak membebani esp32 (overwork)"

**Konteks:** dashboard menampilkan seluruh identifier di bus. Pada Gelora E itu
34 baris hex, dan hampir semuanya tidak berarti apa pun bagi orang yang membaca
layar di dalam mobil.

Satu hal dalam alasan permintaan ini perlu diluruskan, dan diluruskan di dokumen
juga: **beban dashboard bukan penyebab status Overworked.** Frame hilang karena
interupsi CAN berada di flash, sehingga setiap penulisan flash mematikan cache
instruksi. Terukur 14,8 % melawan 0,0 %. Menyembunyikan baris tidak menyentuh
sebab itu sedikit pun.

**Decision:** penyaring dipasang pada **apa yang dikirim dan digambar**, tidak
pernah pada apa yang diterima atau direkam. Tiga mode: TDS, Named, All. Alat
tetap menerima, menghitung, dan merekam seluruh frame di bus, karena identifier
yang belum dinamai siapa pun justru itu yang dibutuhkan sesi pemetaan
berikutnya.

Penandanya awalan `#tds` di teks catatan identifier, bukan kolom baru dan bukan
berkas baru: tidak menambah penyimpanan, tidak mengubah format, dan operator
memasang atau mencabutnya dengan mengetik.

**Implementasi:** `StateJson::IdFilter` dan parameter `/api/state?ids=`,
`NotesStore::noteFor()` dan `taggedTds()`, penyaring tiga tombol di halaman,
panel Vehicle yang membangun kartunya dari catatan bertanda yang memuat rumus,
dan `tools/apply_notes.py` untuk memasang satu berkas catatan per kendaraan.
Halaman selalu menyebutkan berapa yang disembunyikan, dan turun sendiri ke mode
yang berisi selama pemakainya belum pernah memilih. Konvensinya di
`docs/DASHBOARD-TDS.md`.

Yang memang dihemat, dan jujur kelas dua: dokumen `/api/state` menyusut sekitar
tiga perempat pada bus 34 identifier dengan 5 bertanda.

---

## D-007: Arah perangkat berpindah ke Orange Pi 5 (23 Sep 2026 WITA, sesi Claude Code), **PROVISIONAL**

**Source:** operator BMT, sesi Claude Code

> "sebagai informasi tambahan, kedepan kami akan menggunakan orange pi 5 sebagai pengganti esp32"

**Konteks:** disampaikan sebagai informasi, bukan perintah kerja, jadi dicatat
sebagai arah dan bukan keputusan terkunci.

**Decision (arah):** perangkat perekam berpindah dari ESP32-S3 ke Orange Pi 5.

**Yang selesai dengan sendirinya kalau ini jadi:**

- **Frame hilang.** SocketCAN di Linux tidak punya masalah interupsi yang
  terhenti oleh penulisan flash. Seluruh isi `00-umum/frame-loss.md` menjadi
  sejarah, termasuk rekomendasi microSD untuk rev B.
- **Batas 12 MB.** Berganti jadi kapasitas kartu atau SSD.
- **Alat analisis.** Python, `cantools`, dan basis data bisa jalan di perangkat
  itu sendiri, jadi pemetaan tidak lagi menuntut laptop ikut di mobil.
- **Dashboard.** Halamannya HTML biasa plus satu endpoint JSON, jadi berpindah
  apa adanya.

**Yang justru menjadi lebih sulit, dan belum dijawab:**

- **Daya dan kontak.** Orange Pi 5 menarik arus jauh lebih besar dan **tidak
  boleh mati mendadak** saat kontak diputar. Butuh mematikan dengan rapi,
  cadangan daya sesaat (supercapacitor atau UPS kecil), atau berkas yang tahan mati listrik.
- **Waktu siap.** ESP32 siap dalam hitungan ratusan milidetik; Linux beberapa
  puluh detik. Untuk alat uji itu tidak masalah, untuk unit armada itu
  menentukan.
- **Suhu kabin.** Kabin terparkir di Bali bisa lewat 80 derajat. Batas kerja
  Orange Pi 5 lebih sempit daripada ESP32.
- **Jaminan listen-only.** Penegakan tiga lapis yang ada sekarang bersandar pada
  mode controller TWAI, `#pragma GCC poison`, dan gate build. Pada SocketCAN
  jaminan setara harus dirancang ulang, dan **tidak boleh diturunkan**.

**Implementasi:** belum ada yang dikerjakan, dan belum ada yang boleh dikerjakan
atas dasar arah ini. Yang mengunci atau membatalkannya: keputusan operator BMT
setelah keempat hal di atas dijawab. Sampai itu terjadi, pekerjaan ESP32 tetap
berjalan, karena pemetaan kendaraan armada tidak boleh menunggu perangkat baru.

---

## D-008: Dua dashboard dari satu alat, lokal dulu, dan argo tidak disimpan (28 Sep 2026 WITA, sesi Claude Code)

**Source:** pesan Direktur BMT, diteruskan operator lewat sesi Claude Code

> "esp32 menampilkan 2 dashboard secara bersamaan dan secara lokal dulu: 1. dashboard TDS (selaride yang kita buat terakhir) 2. dashboard tampilan semua data yang berhasil kamu petakan pada testing dengan dfsk gelora e) seteah berhasil, baru nanti esp32 kita coba mengirim data ke server BMT"

**Konteks:** Sampai 27 September alat hanya punya satu halaman, dashboard teknis.
Layar SELARIDE hidup di `project-mdt-tds` sebagai halaman terpisah dengan data contoh,
dan tidak pernah tersambung ke bus. Lebih penting lagi: firmware **tidak punya decoder
sinyal sama sekali**. Yang bernama `speed` di `src/` adalah kecepatan GPS dari NMEA,
bukan kecepatan kendaraan dari CAN. Jadi kedua dashboard yang diminta bergantung pada
satu komponen yang belum ada.

**Decision:**

1. Alat menyajikan **dua alamat**: `/` dashboard teknis, `/argo` layar SELARIDE.
   Keduanya boleh dibuka bersamaan dari dua gawai.
2. Nilai ter-decode datang dari tabel di firmware (`src/SignalDecoder.cpp`), berisi
   delapan pemetaan DFSK Gelora E yang sudah berstatus **terbukti**. Yang belum terbukti
   tidak masuk tabel.
3. Argometer menghitung **kuantitas saja**: kilometer isi dan menit tunggu. Rupiah
   dihitung oleh halaman, dari `tariff.js`, satu-satunya berkas di seluruh sistem tempat
   harga ditulis. Dua tempat yang sama-sama tahu harga adalah dua tempat yang bisa
   berselisih soal uang penumpang.
4. Jarak diambil dari **selisih odometer**, bukan dari integral kecepatan. Odometer
   adalah hitungan kendaraan sendiri: tidak melenceng mengikuti seberapa sering frame
   datang, tidak menumpuk galat sepanjang shift, dan selamat dari putusnya penerimaan.
5. **Argo tidak disimpan.** Alat reboot di tengah trip berarti tarifnya hilang dan harus
   dipencet `Mulai` lagi. Ini disengaja: alternatifnya menulis ke filesystem yang
   menyimpan rekaman CAN, dan tidak ada fitur tampilan yang sepadan dengan risiko itu.
6. Pengiriman ke server BMT **belum dikerjakan sama sekali**, sesuai urutan yang diminta.

**Implementasi:** commit `3302eaf`, 28 September 2026. `SignalDecoder`, `ArgoMeter`,
`web-argo/`, rute baru di `WebDashboard.cpp`, dan `tools/embed_web.py` yang menyusun
kedua halaman. Biaya flash 219 KB, seluruhnya aset. Partisi LittleFS **tidak disentuh**,
karena mengunggah image filesystem akan menghapus rekaman CAN yang ada di sana.
Prosedur ujinya di `docs/PROSEDUR-TEST-2-DASHBOARD.md`.

**Yang membatalkan atau mengubahnya:** hasil uji jalan 29 September. Kalau dua gawai
membuka dua halaman sekaligus ternyata membuat `rx_overrun` naik, urutan
"lokal dulu, server kemudian" tetap berlaku tetapi jumlah klien yang dilayani serentak
harus dibatasi.

---

## D-009: Penempatan di panel Vehicle ditentukan satuan bacaan, bukan identifier (28 Sep 2026 WITA, sesi Claude Code)

**Source:** operator BMT lewat sesi Claude Code, disertai dua gambar acuan dari Direktur

> "bagian ini buat visualisasi lebih menarik dan dinamis, buat seperti contoh gauge, grafik realtime, atau icon, atur agar terlihat menarik dan profesional"

> "tampilan kamu kurang menarik, perlebar tampilan, buat mirip seperti di contoh"

**Konteks:** Panel Vehicle dibangun dari catatan identifier, dan catatan ditulis per
kendaraan dengan kata-kata si pemeta. Acuan yang diberikan Direktur berbentuk cluster
kendaraan listrik dengan tiga zona tetap. Menyusun panel mengikuti gambar itu berarti
memilih: mematok identifier mana yang masuk zona mana, atau mencari sesuatu yang berlaku
di semua kendaraan.

**Decision:** yang menentukan sebuah bacaan masuk zona mana adalah **satuannya**.
`%` mengambil cincin di tengah, `km/jam` mengambil angka besar di kanan, `km` mengambil
blok kiri, sisanya tetap jadi kartu. "Motor", "Kecepatan" dan "SOC" adalah teks bebas
yang diketik seseorang; `rpm`, `km/jam` dan `%` berarti sama di setiap kendaraan.
Karena itu pula **ikon diambil dari satuan**, dan satuan yang tidak dikenali dapat ikon
netral, bukan ikon yang salah.

Turunannya, yang sama mengikatnya:

- **Skala grafik menyesuaikan diri** terhadap apa yang sudah terlihat, karena tidak ada
  rentang yang diketahui dan mengarangnya berarti mengaku tahu sesuatu tentang kendaraan
  itu. Karena skalanya nisbi, grafiknya sengaja tidak bersumbu.
- **Tidak ada angka yang diwarnai karena nilainya.** Ambang adalah keputusan tentang
  kendaraan, dan alat ini tidak berhak mengambilnya. Warna hanya ada pada skala.
- Tanpa persen maupun kecepatan, cluster tidak dibangun sama sekali dan panel kembali
  jadi kisi kartu. Kendaraan yang belum dipetakan tidak pernah melihat zona kosong.

**Implementasi:** commit `12280c0` (riwayat 40 detik di tiap kartu) dan `2559986`
(susunan cluster). Rincian di `docs/DASHBOARD-TDS.md`.

**Yang sempat dicoba lalu ditarik:** commit `82d603c` menyusun ulang panel mengikuti
acuan kedua, dengan dial kecepatan besar di tengah dan bacaan baterai terpecah jadi
kartu sendiri-sendiri. Operator menolaknya:

> "saya tidak suka hasil pekerjaanmu yang terbaru ini, kembalikan ke desain sebelum ini"

Dikembalikan lewat `git revert` pada commit `9c8296e`, bukan dengan menghapus commit-nya,
karena riwayat yang sudah terdorong ke remote tidak ditulis ulang. Yang ikut kembali
termasuk `notes.tsv`: catatan pack kembali ke bentuk semula, jadi **tidak ada catatan
yang harus dipasang ulang** karena percobaan ini.

---

## D-010: Firmware dua dashboard flash ke alat, argometer bilangan bulat (28-29 Sep 2026 WITA, sesi Claude Code)

**Source:** operator BMT lewat sesi Claude Code, sesi flashing langsung sebelum uji jalan 29 September

> "siap-siap sebentar lagi akan saya flash firmware yang baru ke alat, nanti akan coba melakukan test jalan lagi"

**Konteks:** Firmware dua dashboard (D-008) dan susunan cluster panel Vehicle (D-009)
sudah dibangun dan lolos gate sejak 28 September, tetapi belum pernah menyentuh alat.
Sore harinya operator juga meminta empat perbaikan di layar argo setelah membukanya di
tablet sepuluh inci: format bilangan bulat untuk argometer dan jarak, stopwatch untuk
waktu tunggu, tarif tunggu Rp 1.000 per 60 detik, dan tata letak yang utuh di layar
potret lebar. Keputusan tarifnya sendiri (kenapa bilangan bulat, kenapa Rp 1.000) ada
di `project-mdt-tds` D-014 dan D-015; catatan ini hanya sisi firmware dan flash.

**Decision:**

1. `ArgoMeter` mengikuti keputusan `mdt-ui`: `KM_STEP` naik dari 0,1 menjadi 1,0, dan
   `MIN_STEP` untuk waktu tunggu **dihapus**: waktu tunggu tidak lagi ditik, dikirim
   mentah dalam milidetik (`wait_ms`, mengganti `wait_min`) supaya halaman bisa
   menjalankannya sebagai stopwatch dan menagihnya dari angka yang sama yang dipajang.
2. `tools/fake_device.py` diikutkan mengirim bentuk JSON yang sama, karena alat tiruan
   yang mengirim bentuk lama adalah alat tiruan yang menguji halaman yang salah.
3. Firmware di-build untuk `gelora-e-250k`, lolos `check_listen_only.sh`, dan **di-flash
   ke alat sungguhan** via USB (COM6) menggunakan `pio run -t upload`. Bukan
   `uploadfs`. Partisi LittleFS tidak disentuh, karena berisi rekaman CAN.

**Temuan operasional selama sesi ini, dicatat karena tidak jelas dari kode:**

- **Alat memegang 13,3 MB rekaman (60 berkas) yang belum pernah ditarik**, sisa flash
  saat ditemukan hanya 421 KB. Empat berkas terakhir 0 byte: sudah ada sesi perekaman
  yang gagal karena kehabisan tempat. `clearcaptures` **tidak dijalankan**; menghapus
  rekaman yang belum dicadangkan bukan keputusan yang boleh diambil otomatis. Lihat
  B-28 di `../BACKLOG.md`.
- **Perekaman menyala otomatis setiap kali alat boot.** Dimatikan manual dua kali
  lewat konsol serial selama sesi ini (`capture off`), dan akan menyala lagi begitu
  alat direstart. Ini perilaku lama, bukan regresi, tetapi belum pernah tertulis
  sebagai sesuatu yang perlu diingat operator.
- **Alat tidak punya IP tetap dan hanya mengenal satu SSID**, dikompilasi ke
  `src/Secrets.h` (`WiFi.begin(WIFI_SSID, WIFI_PASS)`, tanpa daftar cadangan). Saat
  hotspot berganti nama atau alat reboot, IP lama berhenti menjawab tanpa peringatan.
  Cara paling pasti mendapat IP baru: colok USB, ketik `wifi` di konsol.
- **Membuka port serial dengan DTR/RTS default me-reset ESP32.** `pio device monitor`
  dan pembacaan naif lewat `pyserial` keduanya memicu reboot, yang di tengah sesi
  flashing berarti kehilangan koneksi WiFi yang baru saja terbentuk. Skrip yang benar
  membuka port dengan `dtr=False, rts=False` sebelum `open()`.

**Implementasi:** commit `7ec6b09` di repo ini, `d3bfecb` di `project-mdt-tds`. Hash
`firmware.bin` setelah flash: `fa6d3ec348e56e3e...` (28 byte pertama, lengkap di log
sesi). Flash 1.067.753 B, 50,9 %. `unit=GELORAE-TEST-01`.

**Yang membatalkan atau mengubahnya:** hasil uji jalan 29 September. Kalau dua
dashboard yang hidup bersamaan membuat `rx_overrun` naik, D-008 perlu direvisi
sebelum firmware ini dianggap siap untuk armada.
