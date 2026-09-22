# Prosedur test jalan

Untuk perekaman sambil berjalan, dengan laptop ikut di mobil dan frame
dialirkan lewat WiFi. Dua orang: satu menyetir, satu memegang laptop dan
kamera. **Pengemudi tidak menyentuh laptop.**

Tujuannya satu: membuat nilai-nilai **bergerak**, supaya pemetaan yang sudah
ditemukan bisa naik status dari dugaan menjadi terbukti. Nilai yang diam tidak
membuktikan apa pun.

---

## A. Sebelum berangkat, kendaraan masih mati

- [ ] **Alat terpasang di OBD-II**, kunci kontak mati saat mencolok.
- [ ] **Hotspot HP menyala**, 2,4 GHz, dan laptop tersambung ke hotspot yang sama.
- [ ] **Tutup dashboard di HP.** Setiap halaman yang terbuka ikut merebut WiFi
      yang sama dengan aliran frame.
- [ ] Buka dashboard **di laptop saja**, pastikan header menyebut `GELORAE-TEST-01`,
      bitrate 250 kbps, dan tulisan LISTEN-ONLY.
- [ ] **Letakkan laptop dekat dengan alat dan dekat dengan HP.** Jarak dan bodi
      mobil adalah penyebab paling umum frame terbuang di jalur WiFi.
- [ ] **Foto layar kendaraan**, keduanya:
      panel instrumen (odometer, SOC, jam) dan layar CarInfo
      (tegangan pack, arus, SOC, SOH, suhu baterai, suhu controller).
- [ ] Catat waktu foto itu diambil.

## B. Mulai merekam

```
cd /d D:\Wahyu\BMT\bmt-avanza-iot-testing
python tools\stream_capture.py --host <ip alat> --out captures\gelora-003
```

Baris pertama harus berbunyi `device recording to flash: paused`. Itu benar:
menulis ke flash internal justru membuang sekitar 15% frame, jadi selama
mengalirkan, perekaman flash dijeda.

**Perhatikan dua angka di layar selama merekam:**

| Angka | Artinya | Harus |
|---|---|---|
| `frames` dan `/s` | yang sudah tersimpan di laptop | naik terus, sekitar 600 per detik saat diam |
| `device dropped` | frame yang dibuang alat karena WiFi tidak mengejar | **tetap 0** |

Kalau `device dropped` mulai naik dan muncul `LINK TOO SLOW`, hentikan
(Ctrl+C), perbaiki posisi laptop, lalu mulai lagi. Rekaman berlubang tidak
layak dipakai membuktikan apa pun.

## C. Urutan berjalan

Jalankan berurutan, dan **sebutkan dengan suara** setiap pindah tahap supaya
pemegang laptop bisa mencatat waktunya.

| # | Tahap | Lama | Yang dibuktikan |
|---|---|---|---|
| 1 | Kontak ON, kendaraan diam | 60 detik | garis dasar, semua nilai diam |
| 2 | Kendaraan READY, masih diam | 60 detik | sinyal yang berubah hanya karena sistem siap |
| 3 | Jalan pelan, 10–20 km/jam | 2 menit | kecepatan, dan arus keluar dari 1000 |
| 4 | Berhenti penuh sekitar 30 detik | | kecepatan harus kembali ke nol |
| 5 | Akselerasi agak kuat sekali saja | | arus melonjak, tegangan sel turun sesaat |
| 6 | Pengereman regeneratif yang jelas | | arus **berbalik arah** |
| 7 | **Jalan lurus tepat 2 km** | | odometer naik tepat 2 |
| 8 | Berhenti, kontak tetap ON | 60 detik | suhu masih naik setelah pemakaian |

Selama tahap 7, catat odometer di awal dan di akhir. Itu bukti paling kuat dan
paling mudah diperiksa.

## D. Setelah berhenti

- [ ] **Foto lagi kedua layar** seperti di bagian A, sebelum kontak dimatikan.
- [ ] **Ctrl+C** di jendela perekam. Jangan menutup jendelanya langsung: Ctrl+C
      yang menutup berkas dengan rapi dan mengembalikan alat ke mode merekam.
- [ ] Baca baris terakhir. Harus berbunyi
      `no frames dropped by the device: the recording is complete`.
      Kalau ada peringatan, catat angkanya di lembar run.
- [ ] Salin folder `captures/gelora-003` ke penyimpanan lain **hari itu juga**.
      Rekaman perjalanan tidak bisa diulang dengan kondisi yang sama.

## E. Yang dicatat di lembar run

Tulis apa adanya, termasuk yang gagal:

```
tanggal, jam mulai, jam selesai
kendaraan, kilometer awal, kilometer akhir
SOC awal, SOC akhir
cuaca, suhu luar
siapa menyetir, siapa memegang laptop
foto: nama berkas untuk sebelum dan sesudah
kejadian: setiap Ctrl+C, setiap putus WiFi, setiap tahap yang dilewati
```

## F. Batas dan keselamatan

- Alat **tidak pernah mengirim** ke bus. Itu dijamin saat firmware dibangun,
  bukan saat berjalan, dan tidak ada tombol yang bisa mengubahnya.
- **Pengemudi tidak menyentuh laptop.** Kalau ada yang perlu diperiksa, berhenti
  dulu di tempat yang aman.
- **Colok dan cabut alat hanya saat kendaraan mati.**
- Kalau dashboard menyebut `Wrong bitrate, most likely`, berarti alat memakai
  firmware kendaraan lain. Hentikan, jangan lanjutkan.

## G. Setelah di meja

```
python tools\capture_to_webcan.py captures\gelora-003\can-*.log -o captures\gelora-003.csv
python tools\match_dashboard.py captures\gelora-003 --value <odometer akhir> --scales 1 0.1
```

Yang dicari: apakah keenam kandidat di `docs/evidence/gelora-001.md` **bergerak
mengikuti** angka di foto. Kandidat yang tidak bergerak tetap dugaan, dan harus
dinyatakan begitu.
