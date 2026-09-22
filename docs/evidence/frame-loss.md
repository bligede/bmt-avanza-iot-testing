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
