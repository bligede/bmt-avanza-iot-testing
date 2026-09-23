# 04: Arsitektur teknis

## Peta environment dan cara menjalankan

Project ini tidak punya environment server. Yang setara dengan "environment" di sini
adalah **kendaraan yang sedang diuji**, karena identitas kendaraan ditetapkan saat build
(D-002).

| Env PlatformIO | Kendaraan | Bitrate | UNIT_ID |
|---|---|---|---|
| `hrv` | Honda HR-V 2023 | 500 kbps | `HRV-TEST-01` |
| `gelora-e` | DFSK Gelora E | 500 kbps | `GELORAE-TEST-01` |
| `gelora-e-250k` | DFSK Gelora E | 250 kbps, yang benar | `GELORAE-TEST-01` |

`default_envs` di `platformio.ini` menentukan mana yang dipakai kalau `-e` tidak
disebutkan. Nilainya sekarang `gelora-e`.

```
pio run -e gelora-e-250k                 build
pio run -e gelora-e-250k -t upload       flash lewat USB
pio device monitor                       serial, 115200
sh tools/check_listen_only.sh            gate rilis, jalankan sebelum ke kendaraan
```

Pembaruan firmware **hanya lewat USB**. OTA dibatalkan, lihat D-004.

### Port dan alamat saat alat menyala

| Apa | Di mana |
|---|---|
| Dashboard web | port 80, di alamat IP yang dicetak alat ke serial saat boot |
| Aliran frame langsung | port 3333, TCP, satu penerima pada satu waktu |
| Serial | 115200, tanpa filter |

Alamat IP alat **berubah tiap jaringan**. Pada sesi 22 Sep alamatnya `192.168.100.41`,
pada sesi 23 Sep lewat hotspot HP alamatnya `10.215.81.4`. Jangan menghafal alamat, baca
dari serial atau dari header dashboard.

### Endpoint dashboard

| Endpoint | Guna |
|---|---|
| `GET /api/state` | seluruh status alat, termasuk penghitung frame hilang |
| `GET /api/frames` | frame terakhir yang terlihat |
| `GET /api/captures`, `/api/capture` | daftar dan isi berkas rekaman |
| `POST /api/note`, `GET /api/notes` | nama untuk sebuah identifier, disimpan terpisah dari rekaman |
| `POST /api/mark` | menandai bahwa sesuatu terjadi pada suatu saat |
| `POST /api/sink` | menjeda atau melanjutkan perekaman ke flash |
| `POST /api/clear` | menghapus rekaman, wajib menyertakan UNIT_ID sebagai konfirmasi |

### Lokasi rahasia

Kredensial WiFi ada di **`src/Secrets.h`**, yang di-gitignore dan tidak pernah
di-commit. Templat tanpa nilai ada di `src/Secrets.h.example` dan berisi dua nama:
`WIFI_SSID` dan `WIFI_PASS`.

Jangan menulis kredensial ke `ProjectDocs/`, ke `docs/`, ke pesan commit, atau ke konsol
serial. Token GitHub pernah bocor ke keluaran terminal pada 22 Sep 2026 karena kutip
perintah yang keliru di PowerShell, dan token itu harus dicabut. Kejadian itu tercatat di
`09-TEMUAN-EVALUASI-PROSES.md`.

## Perangkat keras

| Bagian | Pilihan |
|---|---|
| Papan | ESP32-S3-DevKitC-1 N16R8 |
| Transceiver CAN | SN65HVD230 |
| Sambungan kendaraan | kabel ke konektor OBD-II, pin 6 dan 14 |
| Sensor tambahan | GNSS, DHT22, kipas, LED, tombol |
| Framework | arduino-esp32 2.0.17 lewat PlatformIO |

Rancangan papan ada di `hardware/`, dibangkitkan dari satu berkas sumber tunggal
`hardware/gen_schematic.py` dan diperiksa ulang oleh `hardware/verify_schematic.py`.

Unit armada menambahkan modem A7670C. Itu **bukan** bagian dari alat uji ini.

## Pembagian tugas di dalam firmware

| Inti | Tugas |
|---|---|
| Core 0 | pembaca CAN, satu-satunya tugas di sana |
| Core 1 | penyimpanan, WiFi, dashboard, sensor, housekeeping |

Pemisahan itu disengaja: pembaca CAN tidak boleh menunggu apa pun. Aliran frame ke
laptop dipanggil dari tugas penyimpanan saja, satu pemilik, karena versi pertama yang
memanggilnya dari dua tugas membuat alat reboot begitu bus ramai.

## Organisasi kode

| Lokasi | Isi |
|---|---|
| `src/` | firmware. `CanManager`, `RawCanLogger`, `FrameStream`, `WebDashboard`, `NotesStore`, `WifiManager` |
| `web/` | sumber dashboard, ditanam ke firmware oleh `tools/embed_web.py` saat build |
| `tools/` | program bantu di laptop, Python dan satu skrip shell |
| `hardware/` | pembangkit dan pemeriksa skematik |
| `docs/` | prosedur, laporan pengujian, dan bukti |
| `captures/` | rekaman mentah, tidak ikut repositori |
| `partitions/` | tata letak partisi flash |

### Jalur data dan berkas

```
kendaraan (OBD-II pin 6/14)
   -> transceiver -> controller TWAI, listen-only
   -> antrean di RAM
   -> salah satu dari dua tujuan:
        a. flash internal -> captures/ di laptop lewat tools/fetch_captures.py
        b. TCP port 3333 -> langsung ke captures/ lewat tools/stream_capture.py
   -> tools/capture_to_webcan.py  -> CSV untuk analisis
```

Jalur (a) kehilangan sekitar 15 % frame dan dibatasi 12 MB. Jalur (b) tidak kehilangan
apa pun di sisi alat, tetapi bergantung pada kualitas WiFi. Lihat D-003.

### Aturan penempatan berkas

```
kode->src/ dan tools/ · masukan->captures/ · hasil analisis->docs/evidence/ · build->.pio/ (gitignored)
```

Berkas rekaman tidak pernah berada satu folder dengan program yang membacanya, dan tidak
pernah disunting setelah tes.
