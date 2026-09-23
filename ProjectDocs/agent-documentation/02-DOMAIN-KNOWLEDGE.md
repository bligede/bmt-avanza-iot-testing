# 02: Pengetahuan domain

Isi berkas ini adalah aturan yang **sudah dibayar** dengan waktu pengujian di kendaraan
sungguhan. Uraian panjang tiap butir ada di `docs/`, dan sengaja tidak disalin ke sini.

## 1. Listen-only, dan kenapa itu tidak bisa ditawar

Mode listen-only pada controller TWAI tidak pernah mengirim bit dominan, termasuk bit ACK
dan error frame. Akibatnya alat ini tidak bisa mengganggu kendaraan, bahkan saat
konfigurasinya salah.

Buktinya nyata: percobaan pertama pada Gelora E memakai bitrate keliru dan menghasilkan
**1.579.745 bus error dalam 378 detik**. Kendaraan tidak terpengaruh sama sekali, karena
seluruh error itu hanya tercatat di sisi alat.

Penegakannya tiga lapis, di `src/CanBusSafety.h`, `src/CanManager.cpp`, dan
`tools/check_listen_only.sh`. Lapis ketiga menggagalkan build, jadi firmware yang
melanggar tidak akan pernah jadi berkas.

## 2. Bitrate tidak boleh diasumsikan

| Kendaraan | Bitrate | Cara diketahui |
|---|---|---|
| Honda HR-V 2023 | 500 kbps | terukur, run `hrv-001` |
| DFSK Gelora E | 250 kbps | terukur, setelah 500 kbps gagal total |

Tanda bitrate salah: bus error naik ribuan per detik dan **nol frame** terdekode.
Dashboard menyatakannya sebagai "Wrong bitrate, most likely". Kalau itu muncul,
hentikan, ganti env, ulangi dengan kendaraan mati saat mencolok.

## 3. Bentuk bus berbeda antar kelas kendaraan

Gelora E memakai **29-bit extended** seluruhnya, bergaya J1939 seperti kendaraan niaga.
Byte terakhir identifier adalah alamat ECU. Honda HR-V memakai 11-bit seperti mobil
penumpang umumnya. Alat dan skema profil sinyal harus menampung keduanya.

## 4. Frame hilang, dan itu sifat perangkat kerasnya

Framework arduino-esp32 2.0.17 dikirim dengan `CONFIG_TWAI_ISR_IN_IRAM is not set`,
artinya rutin interupsi CAN berada di flash. Setiap penulisan flash mematikan cache
instruksi, interupsi tidak bisa jalan, dan FIFO penerima di controller meluap.

| Kondisi | Frame hilang |
|---|---|
| Menulis ke flash internal | 14,8 % |
| Dialirkan ke laptop lewat WiFi, penulisan flash dijeda | 0,0 % |
| Flash menganggur | nol |

Memperbesar antrean software **tidak menolong**, karena 99,7 % kehilangan berjenis
`rx_overrun`, bukan `rx_missed`. Keduanya harus dibedakan saat membaca dashboard.

Konsekuensi pemakaian: survei identifier dan pemetaan nilai tetap sah, karena nilai yang
disiarkan berkala muncul berkali-kali. Analisis yang bergantung pada urutan antar-frame,
misalnya pencacah bergulir atau protokol multi-frame, **tidak bisa dipercaya**.

Rincian dan pilihan perbaikannya di `docs/evidence/00-umum/frame-loss.md`. Rekomendasi yang
berdiri: kartu microSD pada papan rev B.

## 5. Cara membuktikan sebuah pemetaan

Ini bagian paling berharga dari project ini, dan yang paling mudah dilakukan salah.

**Yang tidak membuktikan apa pun:** nilai yang cocok dengan layar kendaraan saat
kendaraan diam. Field mana pun yang kebetulan berisi angka benar dan tidak berubah akan
terlihat sama meyakinkannya.

**Tiga cara yang terbukti bekerja**, semuanya dipakai pada `docs/evidence/dfsk-gelora-e/gelora-004.md`:

1. **Adu satu sinyal dengan sinyal lain di bus yang sama.** Kecepatan diintegrasikan
   terhadap waktu menghasilkan 4,031 km, dan odometer di bus yang sama naik 4 km.
   Pembacaan mph menuntut 6,49 km, jadi tertolak. Tidak butuh referensi manusia sama
   sekali.
2. **Pakai arah, bukan nilai.** Arus melonjak ke +133 A lalu berbalik ke -36 A dalam satu
   detik saat pedal dilepas. Tidak ada tafsiran lain untuk pembalikan arah selain
   pengereman regeneratif.
3. **Regresi terhadap sinyal yang sudah terbukti, lalu foto sebagai pemeriksa.** Offset
   putaran motor diturunkan dari 11.765 pasangan terhadap kecepatan, bukan dari
   mencocokkan satu foto. Hasilnya meleset satu rpm dari angka di panel instrumen.

**Yang tidak layak jadi bukti:** angka yang diketik manusia sambil menyetir. Dua puluh
tiga laporan kecepatan lewat pesan hanya mencapai korelasi 0,72, dan baru setelah digeser
20 detik. Jeda antara membaca panel, mengetik, dan pesan sampai tidak pernah tetap.
Kalau butuh referensi manusia, **rekam video panel instrumen**.

## 6. Bukti mentah terpisah dari tafsiran

Berkas rekaman tidak pernah diedit setelah tes, dan sesi yang gagal tidak pernah dihapus.
Catatan nama untuk tiap identifier disimpan di `/notes/<UNIT_ID>/` di alat, bukan di
dalam berkas rekaman. Tebakan yang salah dengan begitu bisa dikoreksi tanpa menyentuh
rekaman.

Ini D-015 pada repo armada, dan berlaku penuh di sini. Susunan folder bukti dan
aturannya ada di `docs/evidence/README.md`.

## 7. Identifier tidak pernah dipakai lintas kendaraan

Satu identifier yang berarti kecepatan pada DFSK tidak berarti apa-apa pada Wuling.
Memindahkan angka antar kendaraan adalah cara tercepat membuat sistem armada menampilkan
angka yang salah dengan penuh percaya diri. Yang berpindah adalah **metodenya**.

## 8. Pemetaan yang sudah ada

| Kendaraan | Berkas | Status |
|---|---|---|
| Honda HR-V 2023 | `docs/evidence/honda-hrv-2023/hrv-001.md`, `hrv-001-candidates.md` | kandidat, belum diuji jalan |
| DFSK Gelora E, diam | `docs/evidence/dfsk-gelora-e/gelora-001.md` | terbukti lewat uji jalan berikutnya |
| DFSK Gelora E, berjalan | `docs/evidence/dfsk-gelora-e/gelora-004.md` | **terbukti** |
| Wuling armada | belum ada | **belum pernah diuji** |

## 9. Temuan tentang kendaraan, bukan tentang alat

Paket baterai Gelora E tidak seimbang. Sel terendah berada sekitar 200 sampai 260 mV di
bawah mayoritas sel, dan selisihnya melebar antara 22 dan 23 September. Layar CarInfo
kendaraan hanya menampilkan enam sel pertama, semuanya sehat, jadi selisih ini tidak
terlihat dari dalam mobil.

Temuan semacam ini perlu disampaikan ke pemilik kendaraan. Ia juga menunjukkan nilai
sampingan alat ini di luar tujuan aslinya.
