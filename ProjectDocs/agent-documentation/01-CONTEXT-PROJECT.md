# 01: Konteks project

## Pemangku kepentingan

Project internal PT Bali Mikro Teknologi. **Tidak ada klien eksternal**, jadi tidak ada
persetujuan pihak ketiga yang harus dikejar dan tidak ada kanal komunikasi eksternal yang
perlu dicatat. Keputusan diambil operator BMT dan dicatat di `03-DECISIONS-LOG.md`.

| Peran | Siapa |
|---|---|
| Operator dan pengambil keputusan | BMT, `balimicrotechnology@gmail.com` |
| Akun repositori | `bligede` di GitHub |
| Pemakai alat | teknisi BMT yang melakukan bring-up di kendaraan |
| Pemilik kendaraan uji | pihak yang meminjamkan kendaraan untuk diuji |
| Penerima hasil | `project-mdt-tds` dan firmware armada `bmt-can-bus-telemetry` |

Kendaraan yang diuji **bukan milik BMT**. Itu alasan paling praktis kenapa aturan
listen-only tidak bisa ditawar: alat ini dipasang ke kendaraan orang lain, sebentar, lalu
dicabut.

## Tujuan bisnis

Armada taksi listrik Baswara akan dipasangi modul IoT yang membaca data komputer
kendaraan dan mengirimkannya ke server. Supaya modul itu bisa mengirim angka yang
**berarti**, seseorang harus lebih dulu tahu byte mana di dalam lalu lintas CAN yang
berarti kecepatan, sisa baterai, jarak tempuh, dan seterusnya.

Pengetahuan itu tidak ada di dokumen mana pun yang bisa dibeli untuk kendaraan-kendaraan
ini. Ia harus ditemukan dengan mengukur pada kendaraan yang sungguhan. Project inilah
yang mengerjakan itu.

Hasil project ini karena itu bukan perangkat, melainkan **tiga hal**: peta sinyal per
jenis kendaraan, bukti yang membuat peta itu bisa dipercaya, dan prosedur yang bisa
diulang orang lain.

## Persona pemakai

**Teknisi bring-up.** Berdiri atau duduk di kendaraan orang lain, sering di tempat
parkir, sering sambil ditunggu pemilik kendaraan. Membawa laptop dan HP. Waktunya
terbatas dan kendaraannya tidak bisa dipinjam dua kali dengan kondisi yang sama.
Konsekuensi rancangannya:

- Alat harus menyatakan sendiri apakah rekamannya sehat, tanpa teknisi perlu menganalisis
  apa pun di tempat. Dashboard memuat penghitung frame hilang justru karena ini.
- Kesalahan yang bisa dideteksi harus dideteksi **sebelum** kendaraan dinyalakan, bukan
  setelah pulang. Contohnya bitrate salah, yang dinyatakan dashboard sebagai
  "Wrong bitrate, most likely".
- Prosedur harus berbentuk daftar periksa, bukan paragraf.

**Agent atau engineer yang menganalisis di meja.** Membaca rekaman setelah kejadian,
tanpa kendaraan. Butuh rekaman yang utuh, berlabel, dan terpisah dari tafsiran, karena
tafsiran yang salah harus bisa dikoreksi tanpa menyentuh buktinya.

## Batas cakupan

**Termasuk:** firmware alat uji, dashboard web di alat, program bantu di laptop untuk
menarik dan mengolah rekaman, rancangan papan sirkuit alat, prosedur pengujian, dan
dokumen bukti hasil tiap sesi.

**Tidak termasuk:**

- **Firmware armada.** Ada di `bmt-can-bus-telemetry`, dibekukan oleh keputusan D-006 di
  repo tersebut. Alat ini bukan bagian dan bukan himpunan bagian dari produk itu.
- **Terminal dalam kendaraan dan sisi server.** Ada di `project-mdt-tds`. Project itu
  adalah **konsumen** data yang dihasilkan di sini.
- **Mengirim apa pun ke bus CAN.** Bukan sekadar di luar cakupan, melainkan dilarang
  secara mekanis.
- **Permintaan diagnostik.** Alat ini hanya mendengar siaran berkala. Nilai yang hanya
  muncul kalau diminta, misalnya resistansi isolasi pada Gelora E, tidak akan pernah
  terlihat oleh alat ini.

## Hubungan dengan project lain

```
bmt-avanza-iot-testing        alat uji, repo ini
        |
        | menghasilkan peta sinyal dan metode pembuktian
        v
bmt-can-bus-telemetry         firmware armada, dibekukan (D-006 repo tersebut)
        |
        | mengirim data terdekode dan frame mentah
        v
project-mdt-tds               pengolah di server dan terminal di kendaraan
```

Ketiganya terpisah dengan sengaja. Alasan pemisahan alat uji dari firmware armada ada di
`03-DECISIONS-LOG.md`. Alasan pemisahan terminal dari alat perekam ada di D-001 pada
`project-mdt-tds`.
