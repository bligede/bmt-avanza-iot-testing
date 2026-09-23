# 08: Handoff checklist

## Langkah berikutnya, terurut

### 1. Flash firmware yang sudah dibangun tapi belum masuk alat

Commit `64cba89` sudah lolos gate listen-only dan sudah di-build, tetapi belum pernah
masuk perangkat. Sampai itu dilakukan, alat masih membawa firmware yang memutus
sambungan setiap kali lwIP kehabisan buffer, dan itu yang membuat satu sesi lapangan
kehilangan sekitar 40 % bus.

```
sh tools/check_listen_only.sh
pio run -e gelora-e-250k -t upload
```

Build yang sama juga membawa penyaring identifier tiga mode dan panel Vehicle
(D-006). Keduanya tidak berguna sampai identifiernya punya nama, jadi begitu
alat menyala di jaringan, pasang nama itu sekali jalan:

```
python tools/apply_notes.py --host <ip alat> docs/evidence/dfsk-gelora-e/notes.tsv
```

Delapan identifier, lima di antaranya bertanda `#tds`. Periksa dulu dengan
`--dry-run` kalau berkasnya baru disunting.

### 2. Salin rekaman uji jalan ke luar laptop

`D:\Wahyu\BMT\gelora-004-backup-2026-09-23.zip`, 8 MB, sidik jari
`102094b5...`. Daftar SHA-256 per berkas ada di `captures/gelora-004/SHA256SUMS.txt`.
Perjalanan 47 menit itu tidak bisa diulang dengan kondisi yang sama.

### 3. Jadwalkan satu unit Wuling armada untuk satu kali uji jalan

Ini penghalang terbesar yang tersisa, dan sifatnya penjadwalan, bukan teknis. Yang perlu
disiapkan sebelum hari H:

- [ ] Satu env PlatformIO baru untuk kendaraan itu, mengikuti pola di `platformio.ini`.
- [ ] Bitrate **tidak diasumsikan**. Siapkan env 500 kbps dan 250 kbps, mulai dari 500,
      dan pindah kalau dashboard menyatakan "Wrong bitrate, most likely".
- [ ] Baca `docs/PROSEDUR-TEST-JALAN.md` sampai habis. Dua pelajaran termahal ada di
      situ: laptop harus dekat dengan alat dan dengan hotspot, dan referensi kecepatan
      harus **direkam video**, bukan diketik.
- [ ] Dua orang. Pengemudi tidak menyentuh laptop.

### 4. Uji jalan Honda HR-V, kalau kendaraannya masih bisa diakses

Kandidat dari 8 Sep 2026 di `docs/evidence/honda-hrv-2023/hrv-001-candidates.md` belum pernah diuji
sambil berjalan, jadi statusnya masih sama dengan status Gelora E sebelum 23 Sep: cocok
di atas kertas, belum terbukti. Bus HR-V jauh lebih padat, sekitar 1.000 frame per
detik, jadi kehilangan frame saat merekam ke flash mencapai 22 sampai 26 persen. Sesi
HR-V berikutnya wajib memakai aliran WiFi.

### 5. Papan rev B dengan slot microSD

Rekomendasi yang sudah berdiri di `docs/evidence/00-umum/frame-loss.md` dan
`hardware/README.md` §10. Menulis ke kartu SD lewat SPI tidak mematikan cache instruksi,
jadi interupsi CAN tetap jalan, dan batas 12 MB ikut hilang. Rev A belum pernah
difabrikasi, jadi ini bisa masuk sebelum papan pertama dibuat.

## JANGAN lakukan

1. **Jangan menambahkan jalur kirim ke bus CAN**, walau untuk satu tes, walau di balik
   flag. Tiga lapis penegakan ada supaya ini tidak pernah terjadi karena kelalaian.
   Alat ini dipasang di kendaraan milik orang lain.
2. **Jangan menyunting berkas rekaman.** Nama dan tafsiran punya tempat sendiri di
   `/notes/<UNIT_ID>/`. Rekaman yang sudah disunting tidak bisa diaudit.
3. **Jangan menghapus sesi yang gagal.** Riwayat diagnostik hilang bersamanya. Berkas
   rekaman yang rusak karena transfer serial pada 22 Sep sengaja disimpan sebagai bukti.
4. **Jangan memakai identifier dari satu kendaraan untuk kendaraan lain.** Honda HR-V,
   DFSK Gelora E, dan Wuling armada adalah platform yang berbeda sepenuhnya.
5. **Jangan menyatakan sebuah sinyal terbukti karena nilainya cocok saat kendaraan
   diam.** Yang membuktikan adalah nilai yang bergerak. Suhu controller Gelora E lolos
   dengan cara itu lalu gugur.
6. **Jangan memakai angka yang diketik manusia sebagai kriteria penerimaan.** Dua puluh
   tiga laporan kecepatan lewat pesan hanya mencapai korelasi 0,72. Kalau butuh
   referensi manusia, rekam video panel instrumen.
7. **Jangan mengunduh berkas rekaman sambil merekam.** Keduanya menyentuh flash yang
   sama dengan tempat kode berada, dan frame ikut hilang.
8. **Jangan menulis kredensial ke `ProjectDocs/`, ke `docs/`, ke pesan commit, atau ke
   konsol serial.** Rujuk lokasinya saja. Pola yang sudah bekerja: `Secrets.h`
   di-gitignore, `Secrets.h.example` di-commit.
9. **Jangan merujuk keputusan hanya dengan nomornya.** `D-006` berarti dua hal berbeda
   di dua repositori. Sebut repo asalnya.
10. **Jangan membuka ulang OTA** tanpa keputusan baru yang menggantikan D-004. Kodenya
    sudah di-revert seutuhnya, bukan dinonaktifkan.

## Pertanyaan terbuka

| Pertanyaan | Siapa yang menjawab | Menghalangi |
|---|---|---|
| **Kapan satu unit Wuling armada bisa dipakai satu kali uji jalan?** | operator BMT | seluruh pemetaan sinyal armada, dan lewat itu `project-mdt-tds` |
| Apakah temuan ketidakseimbangan baterai Gelora E sudah disampaikan ke pemilik kendaraan? | operator BMT | tanggung jawab ke pemilik kendaraan uji, isi `06` |
| Papan rev A difabrikasi apa adanya, atau langsung lompat ke rev B dengan slot microSD? | operator BMT | pengadaan perangkat keras |
| Apakah Honda HR-V masih bisa diakses untuk satu kali uji jalan? | operator BMT | status kandidat `hrv-001` |
| Apakah alat ini perlu mendukung permintaan diagnostik, bukan hanya mendengar siaran? | operator BMT | nilai seperti resistansi isolasi tidak akan pernah terlihat tanpa itu, dan itu berarti **mengirim**, yang bertabrakan langsung dengan D-001 repo ini |

Pertanyaan terakhir perlu dibaca hati-hati. Menambah permintaan diagnostik berarti
membatalkan jaminan listen-only, dan jaminan itu adalah alasan alat ini boleh dipasang
di kendaraan orang lain. Kalau kemampuan itu memang dibutuhkan, tempatnya adalah alat
**kedua** yang terpisah, bukan alat ini.
