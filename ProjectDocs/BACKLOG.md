# BACKLOG: Alat Diagnostik CAN BMT

**Last updated:** 23 Sep 2026 WITA

Semua yang **belum bisa** atau **belum layak** dikerjakan sekarang, beserta apa
yang membukanya. Satu tempat, supaya tidak ada yang hilang di sela catatan.

Yang **sedang** dikerjakan dan yang **berikutnya** ada di
`agent-documentation/08-HANDOFF-CHECKLIST.md`. Pertanyaan yang menunggu jawaban
orang, bukan pekerjaan, tetap di berkas itu juga. Backlog ini isinya pekerjaan.

Status yang dipakai:

| Status | Artinya |
|---|---|
| **Siap** | tidak ada yang menghalangi, tinggal dikerjakan |
| **Menunggu akses** | butuh kendaraan, perangkat keras, atau jaringan tertentu |
| **Menunggu keputusan** | butuh jawaban operator BMT lebih dulu |
| **Ditahan sengaja** | bisa dikerjakan, tetapi sengaja tidak, dan alasannya ditulis |

---

## A. Beban perangkat dan kehilangan frame

Semuanya bersumber dari satu sebab yang sama: interupsi CAN berada di flash,
jadi setiap penulisan flash mematikan cache instruksi dan FIFO controller
meluap. Terukur 14,8 % hilang saat merekam ke flash, 0,0 % saat flash menganggur.
Rinciannya di `docs/evidence/00-umum/frame-loss.md`.

### B-01. Filter penerimaan hardware TWAI · **Ditahan sengaja**

Menyaring di controller, sebelum interupsi jalan, jadi frame yang tidak
diinginkan tidak menimbulkan kerja sama sekali.

Sudah diukur pada rekaman `gelora-004` yang sebenarnya:

| | |
|---|---|
| Lima identifier yang dipakai TDS | 26,3 % dari lalu lintas bus |
| Mask tunggal yang memuat kelimanya | meloloskan **13 dari 34** identifier |
| Beban setelah disaring | 36,4 % dari bus, jadi turun 63,6 % |

Mask TWAI memilih **pola bit**, bukan daftar, jadi tidak mungkin menyaring tepat
lima identifier. Itu sifat perangkat kerasnya, bukan kekurangan implementasi.

**Kenapa ditahan:** mematikan perekaman sudah memberi kehilangan **nol** tanpa
menyentuh kode sama sekali, sedangkan filter ini hanya memberi 63,6 % dan
membayarnya dengan rekaman yang **tidak lagi lengkap**. Satu-satunya kasus
pemakaiannya adalah sesi perekaman panjang yang memang hanya butuh sinyal TDS,
dan kasus itu belum ada.

**Yang membukanya:** kebutuhan merekam lama dengan hanya sinyal TDS, misalnya
uji ketahanan unit armada. Kalau dikerjakan, **wajib** ada cap di header berkas
rekaman yang menyatakan filter aktif dan mask-nya, supaya analisis berikutnya
tidak diam-diam mengira rekaman itu utuh.

### B-02. Format rekaman biner, bukan teks · **Siap**

Satu frame sekarang memakan sekitar 44 byte teks. Bentuk biner sekitar 17 byte,
jadi lalu lintas flash turun 2,6 kali dan jeda cache ikut turun sebanding.

Lebih baik daripada B-01 untuk kasus yang sama, karena **tidak membuang satu
frame pun**. Biayanya: format berubah, `capture_to_webcan.py` dan seluruh alat
pembaca harus menyesuaikan, dan berkas lama harus tetap terbaca.

**Kenapa belum:** belum ada sesi yang menuntutnya. Kalau pemetaan Wuling ternyata
menuntut perekaman flash yang panjang, ini yang dikerjakan lebih dulu, bukan B-01.

### B-03. Bangun ulang framework dengan `CONFIG_TWAI_ISR_IN_IRAM=y` · **Ditahan sengaja**

Perbaikan tepat di akar masalah: interupsi CAN pindah ke IRAM, dan penulisan
flash berhenti mengganggunya.

**Kenapa ditahan:** tidak bisa dengan paket Arduino siap pakai, perlu ESP-IDF
dengan Arduino sebagai komponen, dan itu mengubah seluruh cara build. Kalau arah
Orange Pi 5 (D-007) jadi, seluruh pekerjaan ini terbuang.

**Yang membukanya:** keputusan bahwa ESP32 tetap dipakai untuk jangka panjang.

### B-04. Slot microSD pada papan rev B · **Menunggu keputusan**

Menulis ke SD lewat SPI tidak mematikan cache instruksi, jadi interupsi CAN
tetap jalan. Sekaligus menghapus batas 12 MB.

**Yang membukanya:** keputusan rev A difabrikasi apa adanya atau langsung lompat
ke rev B. Rev A belum pernah difabrikasi, jadi masih sempat.

---

## B. Pemetaan kendaraan

### B-05. Pemetaan sinyal Wuling armada · **Menunggu akses**

Penghalang terbesar yang tersisa di seluruh project, dan satu-satunya yang
menghalangi `project-mdt-tds` bergerak.

Metodenya sudah terbukti, prosedurnya sudah tertulis, firmware tinggal ditambah
satu env kendaraan. Yang kurang **hanya akses ke kendaraannya**.

Persiapan ada di `08-HANDOFF-CHECKLIST.md` butir 3.

### B-06. Uji jalan Honda HR-V · **Menunggu akses**

Kandidat 8 Sep 2026 belum pernah diuji sambil berjalan, jadi statusnya sama
dengan status Gelora E sebelum 23 Sep: cocok di atas kertas, belum terbukti.

Bus HR-V sekitar 1.000 frame per detik, jadi kehilangan saat merekam ke flash
mencapai 22 sampai 26 persen. Sesi berikutnya **wajib** memakai aliran WiFi.

### B-07. Dokumen bukti untuk `gelora-002`, `gelora-003`, dan `gelora-005` · **Siap**

Tiga rekaman dari 22 September malam, 216.454 + 200.389 + 213.979 frame,
tersimpan di `captures/` tanpa dokumen bukti sama sekali. Sebagian angkanya
sudah dipakai di `frame-loss.md`, jadi rekamannya bukan sampah, hanya tidak
punya jejak.

Ketiganya tumpang tindih dan harus dibaca bersama: dua yang pertama dialirkan
ke laptop, yang ketiga ditulis ke flash alat. Itulah sisi kanan dan kiri dari
perbandingan 14,8 % lawan 0,0 %.

`gelora-005` baru ditarik 23 Sep 2026 dan **nyaris hilang**, lihat
`captures/gelora-005/RUN-SHEET.md`.

### B-08. Sinyal Gelora E yang belum selesai · **Siap**

| Yang belum selesai | Keadaannya |
|---|---|
| Nomor sel tertinggi `0x0CFF7D03` b2 | b2 = 44 sedangkan sel 3991 mV ada di posisi 46. Nilainya terbukti, penomorannya belum |
| SOH `0x0CFF7E03` b1 | tidak pernah berubah, jadi belum teruji sama sekali |
| Suhu controller `0x0CFF1601` b2 | **gugur**, tidak bergerak 47 menit termasuk saat arus 133 A. Perlu dicari ulang di identifier lain |
| Resistansi isolasi | tidak ada di 34 identifier mana pun. Kemungkinan hanya keluar lewat permintaan diagnostik, lihat B-14 |

### B-09. Rumus untuk frame multiplex · **Siap**

Tegangan 90 sel dan suhu 30 sensor berbentuk multiplex: satu byte indeks lalu
tiga nilai 16-bit. Mesin rumus di dashboard hanya bisa menyatakan satu frame
tunggal, jadi keduanya hanya bisa dinamai, tidak bisa ditampilkan sebagai angka
di panel Vehicle.

Untuk TDS ini belum penting. Untuk melihat ketidakseimbangan baterai di lapangan,
ini yang dibutuhkan.

---

## C. Dashboard

### B-10. Mode terang untuk dibaca di bawah matahari · **Siap**

Halaman sekarang gelap sepenuhnya. Di kabin terang siang hari, permukaan gelap
memantulkan lebih banyak daripada permukaan terang. Belum pernah diuji di
kendaraan pada siang hari, jadi ini dugaan, bukan keluhan terukur.

**Sebelum dikerjakan:** ukur dulu. Satu sesi siang hari sudah cukup untuk tahu
apakah ini masalah nyata.

### B-11. Panel Live Frames · **Menunggu keputusan**

Endpoint `/api/frames` ada, berjalan, dan menyajikan 60 frame terakhir, tetapi
**halaman tidak pernah memakainya**. Dua pilihan yang sama-sama sah: pakai
sebagai panel debug, atau hapus endpoint-nya karena kode yang tidak dipakai
tetap harus dirawat.

### B-12. Signal probe hanya menawarkan identifier yang tampil · **Ditahan sengaja**

Saat penyaring aktif, probe kehilangan identifier yang disembunyikan. Halaman
sudah mengatakannya dan menyarankan pindah ke All.

**Kenapa ditahan:** memberi probe daftar sendiri berarti dua sumber kebenaran di
satu halaman. Pindah ke All sudah cukup, dan lebih jujur.

---

## D. Perangkat keras

### B-13. Fabrikasi papan · **Menunggu keputusan**

Rev A sudah lengkap: skematik, netlist, dan BOM dibangkitkan dari satu berkas
sumber, diperiksa 12 aturan keselamatan oleh pemeriksa terpisah, diuji dengan
tiga cacat suntikan. Belum pernah difabrikasi. Lihat B-04 untuk rev B.

### B-14. Alat kedua yang bisa meminta data diagnostik · **Ditahan sengaja**

Nilai seperti resistansi isolasi tidak pernah disiarkan berkala, jadi alat ini
tidak akan pernah melihatnya. Menambah kemampuan meminta berarti **mengirim**,
dan itu membatalkan jaminan yang membuat alat ini boleh dipasang di kendaraan
orang lain.

**Kalau memang dibutuhkan, tempatnya alat kedua yang terpisah**, bukan alat ini.
Jaminan listen-only tidak pernah dilonggarkan untuk kenyamanan.

### B-15. Ukur suhu kotak di kabin terparkir · **Siap**

`FAN_ON_TEMP` dan `FAN_OFF_TEMP` di firmware armada masih nilai sementara
bertanda TODO VALIDASI TERMAL, karena belum ada yang mengukur kotak di mobil
terparkir di bawah matahari Bali. Alat ini punya DHT22 dan suhu die SoC, jadi ia
bisa menjawabnya sendiri dalam satu hari parkir.

---

## E. Kalau arah Orange Pi 5 jadi

Seluruh bagian ini berstatus **menunggu keputusan** D-007.

### B-16. Jaminan listen-only di SocketCAN · **Menunggu keputusan**

Yang paling penting dan paling mudah terlewat. Penegakan sekarang bersandar pada
mode controller TWAI, `#pragma GCC poison`, dan gate build. Di SocketCAN ketiganya
tidak berlaku apa adanya, dan penggantinya **harus dirancang, bukan diasumsikan**.
Tidak boleh lebih longgar daripada yang sekarang.

### B-17. Daya, kontak, dan mati mendadak · **Menunggu keputusan**

Orange Pi 5 tidak boleh mati mendadak saat kontak diputar. Butuh mematikan dengan
rapi, cadangan daya sesaat (supercapacitor atau UPS kecil), atau berkas rekaman yang tahan mati listrik.

### B-18. Waktu siap dan suhu kabin · **Menunggu keputusan**

Linux butuh puluhan detik untuk siap, sementara ESP32 ratusan milidetik. Batas
suhu kerja Orange Pi 5 juga lebih sempit, sedangkan kabin Bali bisa lewat 80 °C.

### B-19. Pindahkan alat analisis ke perangkat · **Menunggu keputusan**

Kalau jadi, Python, `cantools`, dan basis data bisa jalan di perangkat itu
sendiri, jadi pemetaan tidak lagi menuntut laptop ikut di mobil. Ini keuntungan
terbesarnya setelah hilangnya kehilangan frame.

---

## F. Proses dan kebersihan

### B-20. Rujukan `D-XXX` tanpa nama repositori · **Siap**

Rujukan di dalam kode dan `docs/` menyebut nomor telanjang, padahal `D-006`
berarti dua hal berbeda di dua repositori. Aturannya sudah ditulis di
`03-DECISIONS-LOG.md`, rujukan lamanya belum diperbaiki.

### B-21. Cadangan rekaman ke luar laptop · **SELESAI 23 Sep 2026**

Empat rekaman diarsipkan lengkap dengan sidik jari SHA-256 per berkas, lalu
diunggah operator BMT ke Google Drive: `gelora-002` (1,1 MB), `gelora-003`
(1,0 MB), `gelora-004` (8,0 MB), `gelora-005` (1,1 MB).

### B-24. Kosongkan flash alat supaya bisa merekam lagi · **SELESAI 23 Sep 2026**

Flash sempat 13,2 MB dari 14,3 MB terpakai dengan anggaran rekam **0 B**,
sehingga alat tidak bisa merekam apa pun.

Urutan yang ditempuh, dan urutan inilah yang penting: tarik seluruh isi ke
`captures/gelora-005`, bandingkan **64 sidik jari SHA-256** satu per satu dengan
isi alat sampai semuanya cocok, pastikan salinannya sudah ada di luar laptop,
baru hapus. Ukuran yang sama tidak membuktikan isi yang sama, dan yang dihapus
adalah satu-satunya pembanding yang tersisa.

Sesudahnya: 0 berkas di flash, penomoran segmen kembali ke `can-000`, dan
**delapan catatan identifier tetap utuh**, karena catatan memang disimpan
terpisah dari rekaman.

### B-22. Pastikan token GitHub yang bocor sudah dicabut · **Siap**

Token milik akun `wira97-tech` tercetak ke keluaran terminal pada 22 Sep 2026.
Jalur push sekarang sudah memakai kredensial bernama dan keluarannya disaring,
tetapi belum ada yang memastikan token lamanya benar-benar dicabut.

### B-23. Sampaikan temuan baterai ke pemilik kendaraan · **Siap**

Paket Gelora E tidak seimbang, sel terendah 200 sampai 260 mV di bawah mayoritas,
dan selisihnya melebar antara 22 dan 23 September. Tidak terlihat dari layar
mobil. Begitu disampaikan, catat di `06-COMMUNICATION-LOG.md`.
