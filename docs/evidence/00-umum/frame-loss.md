# Kenapa frame hilang saat merekam, dan apa yang bisa dilakukan

Temuan 22 September 2026, dari panel Health yang menyatakan **Overworked** pada
pengujian DFSK Gelora E.

## Angka yang terukur

Pada satu sesi berdurasi 948 detik di bus 250 kbps yang mengalir sekitar 644
frame per detik:

| | |
|---|---|
| Frame diterima | 578.035 |
| **Hilang: `rx_overrun`** (FIFO hardware meluap) | **31.548** |
| Hilang: `rx_missed` (antrean driver penuh) | 95 |
| Total hilang | 4,9 % dari seluruh frame di bus |
| Kehilangan saat flash menganggur, diukur 25 detik | **nol** |

Perbandingan penting: **99,7 % kehilangan berjenis `rx_overrun`**, bukan
`rx_missed`. Keduanya berbeda asal:

- `rx_missed` berarti tugas pembaca terlambat mengambil dari antrean driver.
  Diperbaiki dengan memperbesar antrean.
- `rx_overrun` berarti **rutin interupsi tidak sempat jalan sama sekali**,
  sehingga FIFO di dalam controller CAN meluap. Memperbesar antrean software
  tidak menolong sedikit pun.

## Penyebabnya

Framework yang dipakai, arduino-esp32 2.0.17, dikirim dengan sdkconfig yang
memuat baris berikut:

```
# CONFIG_TWAI_ISR_IN_IRAM is not set
```

Artinya rutin interupsi TWAI berada di flash. Pada ESP32, setiap kali flash
ditulis atau dihapus, cache instruksi dimatikan, dan kode yang berada di flash
**tidak bisa dieksekusi** sampai operasi itu selesai. Rutin interupsi CAN ikut
berhenti. FIFO penerima di controller hanya menampung beberapa frame, jadi pada
644 frame per detik ia penuh dalam hitungan milidetik.

Jadi perekam ini melewatkan frame **justru ketika sedang merekam**, dan juga
saat berkas capture diunduh lewat WiFi, karena keduanya menyentuh flash yang
sama dengan tempat kode berada.

Bukti yang menguatkan: penghitung `loop ever` pernah mencatat jeda **1.727 ms**
pada tugas housekeeping, yang hanya masuk akal kalau seluruh inti tertahan oleh
operasi flash.

## Yang sudah dikerjakan

`CAN_RX_QUEUE_LEN` dinaikkan dari 64 ke 256. Ini hanya menghapus jenis
kehilangan yang kecil (95 frame), dan sengaja dilakukan karena murah: sekitar
8 KB heap dari 176 KB yang menganggur. **Ini bukan perbaikan untuk masalah
utamanya**, dan komentarnya di `Config.h` menyatakan hal itu.

## Terbukti: mengalirkan lewat WiFi menghapus kehilangan itu

Diuji 22 Sep 2026 pada kendaraan yang sama, bus mengalir sekitar 640 frame per
detik, dua fase berturut-turut masing-masing 30 detik:

| Fase | Frame diterima | Frame hilang | Rasio |
|---|---|---|---|
| Perangkat menulis ke flash sendiri | 16.627 | 2.898 | **14,8 %** |
| Dialirkan ke laptop lewat WiFi, penulisan flash dijeda | 19.337 | **0** | **0,0 %** |

Di laptop tersimpan 19.303 frame dari 19.332 yang dikirim perangkat. Sembilan
dibuang oleh perangkat karena buffer soket sempat penuh, dan sisanya masih di
jalan saat soket ditutup. Keduanya terhitung, bukan hilang diam-diam.

Ini juga menjelaskan kenapa angka 4,9 % sebelumnya lebih kecil: laju bus saat
itu lebih rendah. Semakin ramai bus, semakin besar bagian yang hilang.

**Karena penulisan flash itu sendiri penyebabnya, perekaman flash harus dijeda
selama mengalirkan.** `tools/stream_capture.py` melakukannya sendiri di awal
dan mengembalikannya di akhir, termasuk saat prosesnya dihentikan paksa.

## Diukur ulang di kendaraan yang hidup, 23 Sep 2026

Pengukuran sebelumnya membandingkan dua fase berturut-turut. Yang ini lebih
langsung: satu alat, satu kendaraan hidup, satu saklar ditekan, dan penghitung
yang sama dibaca sebelum dan sesudah.

| | Frame diterima | Overrun | Hilang |
|---|---|---|---|
| Merekam ke flash, 123 detik sejak boot | 67.828 | 11.573 | **14,6 %** |
| Perekaman dimatikan, 12 detik sesudahnya | 7.843 | 16 | **0,20 %** |

Tujuh puluh tiga kali lebih baik, dan yang berubah hanya satu saklar.

Sisa 0,20 % itu **belum dijelaskan**. Jumlahnya kecil dan tidak mengubah
kesimpulan, tetapi ia bukan nol, jadi jangan ditulis sebagai nol. Dugaan yang
belum diuji: metadata LittleFS, jurnal catatan, atau permintaan dashboard yang
kebetulan menyentuh flash.

### Kenapa pengukuran ini baru muncul sekarang

Karena cacatnya baru terlihat sekarang. Pilihan perekaman **tidak bertahan
melewati reboot**: perangkat selalu menyala dengan perekaman hidup, dan di
kendaraan reboot terjadi setiap kali kontak diputar. Saklar di dashboard
dibatalkan lebih cepat daripada sempat dipakai.

Operator BMT menemukannya dengan cara paling sederhana: melihat status masih
Overworked setelah saklar itu dimatikan. Diperbaiki dengan menyimpan pilihan
terakhir ke berkas dan membacanya saat boot.

## Pilihan perbaikan yang sebenarnya

| Pilihan | Efek | Biaya |
|---|---|---|
| **Kartu microSD** untuk capture | Menulis ke SD lewat SPI **tidak mematikan cache**, jadi interupsi CAN tetap jalan. Sekaligus menghapus batas 12 MB | Perlu slot SD di PCB. Masuk ke rev B |
| **Bangun ulang framework** dengan `CONFIG_TWAI_ISR_IN_IRAM=y` | Perbaikan tepat sasaran di akar masalah | Tidak bisa dengan paket Arduino siap pakai; perlu ESP-IDF dengan Arduino sebagai komponen |
| **Rekam dalam format biner**, bukan teks | Satu frame kini memakan sekitar 44 byte teks. Bentuk biner sekitar 17 byte, jadi lalu lintas flash turun 2,6 kali, dan jeda cache ikut turun sebanding | Perubahan format, konverter harus menyesuaikan |
| **Jangan mengunduh saat merekam** | Menghilangkan satu sumber jeda | Gratis, tinggal disiplin kerja |

Rekomendasi: **kartu microSD di PCB rev B**. Itu menyelesaikan kehilangan frame
dan batas kapasitas sekaligus, tanpa menyentuh framework.

## Sampai itu ada, ini yang berlaku

Rekaman dari alat ini adalah **sampel, bukan salinan lengkap bus**. Pada Gelora E
sekitar 5 % frame tidak tercatat; pada Honda HR-V yang busnya jauh lebih padat
(sekitar 1.000 frame per detik) kehilangannya 22 sampai 26 %.

Konsekuensinya berbeda menurut pemakaian:

- **Survei identifier, periode, dan pemetaan nilai**: tetap sah. Nilai yang
  disiarkan berkala akan muncul berkali-kali, jadi kehilangan sebagian tidak
  mengubah kesimpulan. Seluruh pemetaan `gelora-001` berdiri di atas ini.
- **Analisis yang bergantung pada urutan antar-frame**, misalnya pencacah
  bergulir, checksum, atau protokol multi-frame: **tidak bisa dipercaya**. Frame
  yang hilang di tengah tidak ditandai apa pun di berkas.

Panel Health menyatakan ini apa adanya: angka **Frames lost by driver** berisi
`rx_missed + rx_overrun` dalam satuan frame. Kalau ia tidak nol, rekaman yang
sedang berjalan sedang berlubang, dan itu harus dicatat di lembar run.
