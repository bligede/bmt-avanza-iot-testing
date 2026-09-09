# Rujukan CAN, dan apa yang berubah karenanya

Sumber: masukan rekan-rekan BMT, 8–9 September 2026, setelah sesi bring-up pertama.
Berkas mentahnya di `feedback/`.

Berkas ini bukan daftar tautan. Bagian yang penting adalah kolom terakhir setiap tabel:
**apa yang berubah di kode kita**. Rujukan yang tidak mengubah apa pun dicatat sebagai
rujukan saja, dan dikatakan begitu.

---

## 1. Dekode bus

| Rujukan | Isi | Untuk kita |
|---|---|---|
| [opendbc](https://github.com/commaai/opendbc) | kumpulan DBC + parser + API per merek | rujukan utama bentuk akhir; **tidak ada Wuling/SGMW** |
| [cantools](https://github.com/eerimoq/cantools) | parse DBC, decode frame jadi nilai fisik | dipakai di laptop untuk validasi sebelum logika masuk firmware |
| [awesome-automotive-can-id](https://github.com/iDoka/awesome-automotive-can-id) | indeks CAN ID + payload per merek | **Wuling, Baojun, SGMW belum ada di daftarnya** |
| [awesome-canbus](https://github.com/iDoka/awesome-canbus) | indeks tool, hardware, dokumentasi format DBC | rujukan |

**Konsekuensi yang harus disadari sejak sekarang.** Kalau armada Baswara berbasis Wuling
atau SGMW, tidak ada DBC rujukan yang bisa dipinjam. Kita membangunnya dari nol, dan
itu bukan kemunduran — hanya perlu masuk perkiraan waktu sejak awal, bukan ditemukan di
tengah jalan.

## 2. Membongkar sendiri

[can-bus-reverse-engineering-skills](https://github.com/CSS-Electronics/can-bus-reverse-engineering-skills)
(CSS Electronics, MIT) — alur kerja dari log CAN mentah ke DBC: cari CAN ID, start bit,
panjang, endianness, scale, offset, lalu verifikasi.

**Sudah dipasang sebagai skill Claude** di mesin ini, 9 Sep 2026, tiga skill:
`cansub-knowledge`, `cansub-reverse-engineering`, `combine-dbc`.

Tiga mode kerjanya, dan yang ketiga mengubah nilai prosedur run kita:

- **OFFLINE** — decode dari log yang sudah ada, memakai rujukan yang bisa didekode
  terpisah: PID OBD-II, atau kecepatan GNSS.
- **LIVE** — tangkap dari antarmuka CANsub dengan rujukan yang disuplai manusia.
- **VISION** — log CAN plus **video panel instrumen**, lalu skrip OCR lokal mengubah
  angka di layar jadi deret rujukan.

Mode VISION itu penting. `docs/RUN-PROCEDURE.md` sudah mewajibkan merekam video kluster
instrumen sebagai trek anotasi yang dibaca manusia. Ternyata video itu bisa dibaca mesin
juga, dan berubah dari catatan pendukung menjadi **sumber rujukan utama** — terutama
selagi antena GNSS mati dan kecepatan belum punya pembanding independen di perangkat.

Alurnya sejalan dengan `docs/RUN-PROCEDURE.md` dan dengan `tools/can_find_value.py`.

## 3. Kasus Wuling / SGMW

| Rujukan | Catatan |
|---|---|
| [opendbc-byd](https://github.com/BYDcar/opendbc-byd) | bukan Wuling, tapi contoh bentuk akhir yang dituju |
| [Utas XDA Wuling/Baojun/MG/SGMW](https://xdaforums.com/t/hacking-wip-wuling-baojun-mg-sgmw-vehicles.4714034/) | fokus head unit Android, bukan DBC — komunitas terdekat yang ada |

**Catatan penulis opendbc-byd yang layak dijadikan ekspektasi:** replay `candump` tidak
direspons kendaraan, kemungkinan port OBD-II difilter.

Itu sejalan dengan temuan kita sendiri pada Avanza 2009: pin 6↔14 terukur terbuka
(~22,5 kΩ, merayap), tidak ada CAN di OBD-II sama sekali. **Port OBD-II bukan jaminan
akses ke bus internal**, dan itu berlaku baik untuk membaca maupun menulis.

---

## 4. Jebakan decoding — dan status kita di masing-masing

Ini bagian yang paling berharga dari seluruh masukan. Tabelnya dibaca sebagai daftar
periksa, bukan bacaan.

| Jebakan | Isi | Status kita |
|---|---|---|
| **Byte order** | keliru Intel lawan Motorola menghasilkan angka yang **terlihat masuk akal tapi salah**, bukan crash | probe punya pilihan big/little-endian ✓ |
| **Signed value** | two's complement; dibaca unsigned, −5 jadi 251 | probe punya kotak centang `signed` ✓ |
| **Multiplexed signal** | satu CAN ID membawa isi berbeda tergantung nilai byte multiplexor | **BELUM DITANGANI** — lihat di bawah |
| **Timestamp** | ambil saat frame diterima kernel, bukan saat JSON sampai di browser | `rx_millis` diambil di task penerima ✓ |
| **Rate** | mendorong ribuan frame/detik ke WebSocket membekukan tab; agregasi di gateway, 10–30 Hz untuk tampilan, full-rate ke disk | halaman polling 2 Hz, kirim 60 frame terakhir; full-rate ke flash ✓ |
| **Keamanan** | CAN tanpa autentikasi; kalau gateway bisa menulis, siapa pun yang menembus web app bisa menulis ke bus | listen-only tiga lapis + gate build ✓, **pemutusan TX fisik belum** (F-13) |

### Byte order: kita sudah pernah kena kelas cacat ini

"Angka yang terlihat masuk akal tapi salah, bukan crash" persis menggambarkan bug DHT22
pada 9 Sep 2026: frame `04 E0 02 73 59` **lolos checksum** dan terbaca 124,0 %RH serta
62,9 °C, karena tertangkap tergeser satu bit.

Pelajarannya sama untuk CAN: **pemeriksaan struktural yang lolos tidak membuktikan
penafsirannya benar.** Kandidat sinyal butuh pemeriksaan kewajaran yang terpisah dari
pemeriksaan integritas, dan validasi terhadap pengamatan independen.

### Multiplexed signal: celah nyata di pengintai sinyal

Pengintai sinyal di dashboard mengasumsikan satu CAN ID selalu membawa tata letak yang
sama. Kalau ID yang diintai ternyata multiplexed, angkanya akan **berubah-ubah tanpa pola
yang masuk akal** — dan itu akan terbaca sebagai "byte ini bukan sinyalnya", padahal
sinyalnya ada dan cuma muncul saat byte multiplexor bernilai tertentu.

Gejala yang membedakan: nilai melompat antara beberapa rentang yang berbeda jauh, bukan
bergerak halus. Kalau itu terlihat, curigai multiplexing dan pindah ke `cantools` di
laptop — parser manual umumnya tidak menanganinya.

Belum diperbaiki. Dicatat supaya tidak salah didiagnosis saat muncul.

---

## 5. Bentuk sistem: decode sekali di gateway

Prinsip dari masukan yang sama, dan ini mengikat rancangan MDT-TDS:

> Browser tidak punya akses ke CAN, dan tidak akan pernah punya. Polanya selalu: gateway
> membaca frame, mengubahnya jadi JSON, lalu mendorongnya. Klien web dan Flutter cuma dua
> ujung yang identik.

Dan peringatan yang menyertainya: **jangan menduplikasi decoding DBC di klien.** Decode
sekali di gateway, sisakan klien sebagai penerima. Mem-parsing bit di dua bahasa berarti
dua tempat untuk salah membaca byte order, dan bugnya tidak akan muncul sampai ada sinyal
Motorola.

Yang dikirim ke klien adalah nilai yang sudah jadi:

```json
{"ts": 1757310000.412, "msg": "EngineData", "sig": {"rpm": 1520.5, "coolant": 87}}
```

Hex mentah disimpan untuk satu tampilan debug terpisah, bukan dijadikan format utama.

**Berlaku langsung untuk MDT:** panel ESP32-S3 tidak mendekode apa pun. Ia menerima nilai
jadi. Dicatat sebagai D-006 di `project-mdt-tds`.

---

## 6. Perangkat keras: Raspberry Pi + SocketCAN

Masukan menyebut Raspberry Pi plus SocketCAN sebagai yang paling umum di production
kecil, dengan `ip link set can0 up type can bitrate 500000`, lalu `candump` dan
`cansniffer`.

**Kita tidak pindah.** Alat yang ada sudah membaca 40 identifier pada 1.282 frame/detik
dengan nol dropped dan nol missed di kendaraan sungguhan. Tidak ada masalah yang
diselesaikan dengan berpindah platform, dan berpindah berarti membuang jaminan
listen-only yang sudah ditegakkan tiga lapis plus gate build.

Yang layak dipinjam bukan perangkat kerasnya melainkan perkakasnya: keluaran kita
sebaiknya bisa dikonversi ke format `candump` supaya `python-can`, `cantools`, dan
SavvyCAN bisa membacanya langsung.

Satu catatan operasional dari sana yang berlaku untuk kita juga: **bitrate salah membuat
interface masuk bus-off, dan keluarannya terlihat kosong — bukan error yang jelas.**
Persis kelas cacat yang sama dengan tabel identifier yang kehilangan 47 persen data tanpa
tanda apa pun.
