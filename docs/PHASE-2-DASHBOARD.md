# Fase 2 — dashboard web untuk memecahkan ID ECU

Rencana dua minggu dan catatan apa yang sudah dibangun. Mulai 13 September 2026.

## Masukan CEO BMT (dicatat apa adanya)

> tambahkan satu kolom input sebagai catatan saat nanti testing (diisi pada saat testing
> sebagai note dalam mengenali ID yang dikeluarkan oleh ECU)
> apa butuh library tinygsm? buat kode yang clean architectur, maintenable, modular
> tampilkan penggunaan memory internal di esp32 (mengetahui apa esp32 dalam performa yang
> baik ataukan overwork?)
> interface lcd apa bisa mengoutputkan html?
> apa bisa esp32 berkomunikasi dengan payment gateway?
> cari hmi android yang kompatibel dengan esp32
> fase 2 minngu kedepan, fokus di aplikasi via dashboard web, tampilkan dari data hex ke
> desimal, tabel jangan discroll agar mudah memantau data keseluruhan dalam 1 tampilan
> gunakan reverse engineering skill untuk ememcahkan kode id dan nilali hes dari ecu

---

## 1. Yang sudah dibangun

### Tabel identifier — satu tampilan, tanpa scroll

- Setiap ID di bus satu baris, **urut ID** — tidak berpindah-pindah saat hitungan berubah,
  jadi mata hafal letaknya.
- Byte 0–7 ditampilkan **hex di atas, desimal di bawah**.
- Byte yang berubah dalam 2 detik terakhir **menyala hijau**. Byte pencacah bergulir
  (biasanya byte terakhir) akan menyala terus — itu justru cara mengenalinya.
- ID yang diam lebih dari 2 detik **diredupkan**.
- Laju per ID dalam Hz, dihitung di jendela ±3 detik supaya ID 1 Hz tidak meloncat 0–2.
- Di layar ≥1280 px tabel dibagi **dua kolom berdampingan**: 40 ID muat dalam satu layar
  1440 px. Di HP tabel bisa digeser ke samping di dalam panelnya, tidak pernah atas-bawah.

### Kolom nama per ID, tepat di sebelah ID

- Kolom **Name** kosong dan siap diisi, letaknya persis di kanan kolom ID, sebelum Hz dan
  byte, sehingga terbaca berpasangan dengan ID-nya.
- Ketik di baris ID-nya, tekan Enter. Tersimpan di perangkat, bingkai hijau = tersimpan,
  merah = gagal (arahkan kursor untuk alasannya).
- Perangkat mengembalikan teks yang **benar-benar disimpan** (dipangkas, maks 60 byte
  UTF-8), dan kolom menampilkan itu — catatan tidak pernah tampak tersimpan dalam bentuk
  yang tidak disimpan.
- Disimpan di `/notes/ids.tsv`, dan setiap perubahan dicatat di `/notes/journal.log`
  beserta waktunya.
- **Sengaja tidak ditulis ke berkas capture.** Catatan adalah tafsiran, bukan bukti;
  keduanya dipisah (D-015). Tebakan yang salah bisa dikoreksi tanpa menyentuh rekaman run.

### Panel Health — ESP32 sehat atau kewalahan?

| Meter | Artinya |
|---|---|
| CAN driver queue | antrean frame di driver TWAI, puncak sejak boot |
| Capture queue | antrean frame menunggu penulis flash, puncak sejak boot |
| Frames lost by driver | `rx_missed_count` + `rx_overrun_count` — **frame**, bukan kejadian alert |
| Task stacks | tiap task: seberapa dekat pernah ke ujung stack-nya |
| Heap in use / low-water | pemakaian heap sekarang, dan titik terendahnya sejak boot |
| Loop lag | housekeeping tidur 50 ms; berapa lama jeda nyatanya |
| Core 0 / Core 1 | beban CPU per core, **perkiraan** |
| Flash filesystem, firmware image | ruang tersisa |

Verdict **Healthy / Tight / Overworked** hanya memakai angka yang bergerak *sebelum* data
hilang. Beban CPU ditampilkan tapi tidak dipakai untuk verdict: framework ini
(arduino-esp32 2.0.17) tidak menyertakan FreeRTOS run-time stats di varian sdkconfig mana
pun, jadi beban diukur lewat idle hook per core — dan cara itu **meremehkan** beban.

Kerugian yang terjadi *sekarang* dibedakan dari yang terjadi *tadi*: satu kejadian di
awal boot tidak membuat verdict merah selamanya.

### Berkas capture bisa diunduh lewat WiFi

Panel Capture menampilkan semua berkas di perangkat; klik untuk mengunduh. Tidak perlu
lagi `cat` lewat konsol serial di port FTDI.

### Arsitektur yang lebih rapi

`WebDashboard.cpp` dulu 1.084 baris: HTML, CSS, JavaScript, semua penyusun JSON, dan rute
dalam satu berkas. Mengubah satu kolom tabel berarti mengedit HTML di dalam string C++.

| Modul | Satu tanggung jawab |
|---|---|
| `web/index.html`, `app.css`, `app.js` | halaman, sebagai berkas web biasa |
| `tools/embed_web.py` | web/ → header gzip, otomatis sebelum setiap build |
| `WebDashboard` | rute HTTP saja |
| `StateJson` | bentuk dokumen JSON |
| `JsonWriter` | penulis JSON berbatas, nol dependensi |
| `NotesStore` | catatan per ID |
| `SystemHealth` | memori, stack, antrean, CPU |
| `FrameRing` | jejak frame mentah untuk `/api/frames` |

Halaman kini 57,8 KB → **19,5 KB gzip** di flash. Build tanpa warning pada `-Wall -Wextra`.

---

## 2. Dua koreksi yang ikut ditemukan

**"missed" menghitung kejadian, bukan frame** (review F-02). Angka lama menghitung alert
`RX_QUEUE_FULL`; satu alert bisa berarti banyak frame hilang. Driver TWAI punya penghitung
frame sendiri (`rx_missed_count`, `rx_overrun_count`), dan itulah yang sekarang ditampilkan
sebagai *missed*.

**Klaim "disalin tanpa perubahan" di README sudah tidak benar.** `RawCanLogger` diubah di
`fd386ba`, `CanManager` di fase ini. Repo armada sedang dibekukan, jadi keduanya tidak bisa
"diperbaiki di sana lalu disalin ulang". README sekarang menyatakan divergensinya dan apa
yang harus dipindahkan balik.

---

## 3. Alur memecahkan ID dengan skill reverse engineering

Skill `cansub-reverse-engineering` membaca **webCAN CSV** dan menyelaraskan rujukan lewat
epoch timestamp.

```
1. Unduh segmen capture yang sudah selesai dari panel Capture
2. python tools/capture_to_webcan.py can-003.log can-004.log -o captures/run-002.csv
3. Serahkan run-002.csv ke skill:
     survey     -> ID mana yang bergerak, byte mana pencacah/checksum
     correlate  -> dibutuhkan RUJUKAN (lihat di bawah)
     bitsearch  -> start bit, panjang, endianness
     build_dbc  -> skala, offset, jadi DBC
     verify
```

**Survei bisa langsung jalan** pada rekaman apa pun. **Korelasi butuh rujukan** — deret
nilai fisik yang diketahui pada waktu yang sama:

| Rujukan | Status |
|---|---|
| Kecepatan GNSS | antena mati |
| PID OBD-II | tidak mungkin — butuh mengirim request, melanggar listen-only |
| **Video panel instrumen** (mode VISION skill) | **jalur utama** — wajib direkam di run berikutnya |

### Soal foto panel instrumen HR-V (8 Sep): angka mana yang terekam?

**Terjawab pada 20 Sep 2026.** Rekaman penuh ditarik dari flash perangkat lewat WiFi
sebelum perangkat di-flash ulang: 34 berkas, 8,8 MB, satu run utuh 3 menit 23 detik.

| Angka di panel | Hasil |
|---|---|
| Odometer **069620 km** | `0x294`, byte 3-5 big-endian, di 100% frame ID itu |
| Suhu luar **29 °C** | `0x324`, byte 0, dengan rumus `°C = nilai - 40` |
| Trip A **1459,6 km** | tidak ada di bus ini |

Perkiraan lama bahwa rekaman hanya memuat "sekitar separuh frame" ternyata terlalu
pesimistis: ID berperiode 10 ms tercatat 78 Hz, bukan 100 Hz, jadi kehilangannya sekitar
22 sampai 26 persen dan merata di semua ID.

Rinciannya: [`docs/evidence/honda-hrv-2023/hrv-001.md`](evidence/honda-hrv-2023/hrv-001.md) untuk asal-usul dan
keutuhan data, [`hrv-001-candidates.md`](evidence/honda-hrv-2023/hrv-001-candidates.md) untuk kandidat
sinyal dan cara membuktikannya. Keduanya masih hipotesis sampai diuji sambil kendaraan
bergerak.

---

## 4. Jawaban untuk pertanyaan CEO

### Perlu TinyGSM?

**Tidak.** Modul A7670C punya stack MQTT, HTTP, dan TLS sendiri; firmware armada memakainya
lewat perintah AT, tanpa pustaka. Dukungan A76xx di TinyGSM upstream juga tidak sematang
SIM7600. Arsitektur berlapis yang diminta sudah dipakai di sana — transport AT terpisah dari
protokol — dan kini diterapkan di lapisan web firmware uji.

### Bisakah LCD menampilkan HTML?

**Panel ESP32-S3 + LVGL: tidak.** Tidak ada mesin browser; LVGL menggambar widget sendiri,
jadi seluruh tampilan harus ditulis ulang dalam C.

**HMI Android: ya.** WebView menampilkan dashboard yang sudah ada **tanpa ditulis ulang**.
Itu argumen terkuat untuk jalur Android.

### Bisakah ESP32 berkomunikasi dengan payment gateway?

**Bisa secara teknis, dan jangan dilakukan.** API payment gateway diautentikasi dengan
*server key*. Kunci yang disimpan di perangkat dalam mobil bisa dibaca dari flash, dan siapa
pun yang memilikinya bisa membuat transaksi atau refund atas nama BMT.

Susunan yang benar: **perangkat → server TDS → payment gateway.** Server memegang kunci dan
membuat QRIS dinamis; perangkat hanya menampilkan QR dan menerima status "sudah dibayar" dari
server. Webhook dari gateway juga harus diterima server publik — perangkat di kendaraan tidak
bisa menerimanya. Pembayaran kartu butuh perangkat bersertifikasi, bukan rakitan.

### HMI Android yang kompatibel dengan ESP32

Ada kategori produk yang namanya persis sama dengan proyek ini: **MDT Android untuk armada**.

| Produk | Yang relevan |
|---|---|
| [Waysion V7S](https://www.waysion.com/v7s-rugged-android-tablet/) | Android 14, **9–36 V DC**, **sensor ACC kontak**, RS232, RS485, dual CAN, IP65 |
| [Topicon MDT765](https://www.topicon.hk/products-detail/en/mobile-data-terminal-mdt765.html) | MDT 7" untuk bus/truk, RS232, IP67 |
| [PaceBlade MDT-770](https://www.paceblade.eu/paceblade-mdt-770) | 7", input 12 V, NFC, WiFi ac |
| [K71 (Kcosit)](https://www.kcosit.com/In-Vehicle-Tablet-Fleet-Management-p4243217.html) | i.MX 8M Mini, 1280×800, RS232, CAN, IP67 |
| [DWIN DMG12800T070_34WTC](https://www.dwin-global.com/7-inch-android-intelligent-display-model-dmg12800t070-34wtc-product/) | layar pintar Android 7" industri, dari produsen layar serial |

Panel industri umum seperti [Geekland 7" Android](https://geekland.co/product/7-android-poe-industrial-panel-pc-hmi-with-rs232-485/)
juga ada, tapi dirancang untuk dinding dengan PoE — bukan untuk catu daya kendaraan.

**Cara menyambungkannya ke ESP32:**

- **WiFi** — paling sederhana. MDT membuka dashboard perangkat di WebView. Nol kode baru.
- **RS232** — **jangan disambung langsung ke GPIO ESP32.** RS232 berayun ±12 V; ESP32 hanya
  tahan 3,3 V. Wajib lewat transceiver seperti MAX3232.
- **RS485** — lewat transceiver 3,3 V seperti SP3485.

Sensor ACC kontak di kelas produk ini sekaligus menjawab temuan F-15 (kuras aki saat
parkir): layar ikut mati bersama kunci kontak.

**Ini membuka ulang D-005** di `project-mdt-tds`, yang mengunci panel ESP32-S3 + LVGL.
Keputusannya ada di CEO. Rekomendasi saya: arah CEO — HTML, HMI Android, pembayaran —
lebih cocok dengan MDT Android daripada panel LVGL. Harganya lebih mahal per unit, tetapi
memberi tampilan HTML tanpa tulis ulang, catu 9–36 V, sensor kontak, IP65, dan 4G/GPS/NFC
bawaan. Satu syarat tetap berlaku: **perekam CAN tetap perangkat terpisah** (D-001),
walaupun MDT-nya punya port CAN sendiri — jaminan listen-only hidup di perekam.

---

## 5. Rencana dua minggu

| Minggu | Pekerjaan | Selesai kalau |
|---|---|---|
| 1 | Flash firmware fase 2; verifikasi di bench | Health = Healthy dengan dashboard terbuka; `Frames lost by driver` tetap 0 |
| 1 | Tarik capture HR-V lama dari flash, konversi, jalankan survei skill | daftar ID dengan pencacah/checksum teridentifikasi |
| 1 | Cari odometer 69620 km di capture penuh | ketemu, atau tercatat "tidak ada di bus pin 6/14" |
| 2 | Run terkendali HR-V sesuai `docs/RUN-PROCEDURE.md`, **dengan video kluster** | capture + video + catatan per ID terkumpul |
| 2 | Korelasi kecepatan dan RPM lewat mode VISION skill | kandidat sinyal dengan start bit, panjang, skala, offset |
| 2 | Keputusan CEO: MDT Android atau panel ESP32-S3 | dicatat sebagai keputusan di `project-mdt-tds` |

Yang **tidak** ada di rencana ini: memasukkan ID apa pun ke `signals.cfg` armada. Kandidat
dari HR-V adalah hipotesis tentang Honda sampai divalidasi terhadap rujukan independen, dan
armada bukan Honda.
