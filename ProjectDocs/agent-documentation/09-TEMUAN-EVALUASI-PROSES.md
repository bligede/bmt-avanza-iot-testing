# 09: Temuan dan evaluasi proses

Diisi **saat temuan muncul**, bukan di akhir project.

**Aturan penanda panen.** Tiap entri baru diakhiri `[belum dipanen]`. Penanda itu antrean,
bukan hiasan: panen mengubahnya jadi `[dipanen -> <tujuan>, YYYY-MM-DD]` setelah rutenya
selesai. Isi entri **tidak pernah disunting** saat ditandai, penanda hanya ditambahkan.

---

## A. Temuan proses

**A-1. Bug yang hanya muncul di kendaraan sungguhan.** Aliran frame lewat WiFi lolos
seluruh pengujian di meja, lalu membuat alat reboot detik itu juga saat bus kendaraan
ramai. Penyebabnya dua tugas memindahkan buffer yang sama. Di meja bus terlalu sepi untuk
memunculkannya. Pelajarannya: untuk kode yang menyentuh jalur akuisisi, pengujian di meja
adalah penyaring, bukan bukti. `[belum dipanen]`

**A-2. Penghitung yang tidak bisa menunjukkan kegagalan yang seharusnya ia tangkap.**
Layar perekam di laptop menampilkan "drops 0" sepanjang satu sesi yang sebenarnya
kehilangan 3.090 frame, karena penghitung itu hanya melihat putus sambungan di sisi
laptop. Pelajarannya: setiap penghitung harus diuji dengan pertanyaan "kalau hal yang
ingin kutangkap terjadi, apakah angka ini bergerak?" `[belum dipanen]`

**A-3. Bukti lebih kuat kalau tidak melibatkan manusia.** Satuan kecepatan diputuskan
dengan mengadu kecepatan terhadap odometer di bus yang sama, dan itu selesai dalam satu
perhitungan. Dua puluh tiga laporan kecepatan yang diketik pengemudi sambil menyetir
hanya mencapai korelasi 0,72 dan tidak memutuskan apa pun. `[belum dipanen]`

**A-4. Rujukan keputusan tanpa nama repositori.** Tiga repo BMT masing-masing punya deret
`D-XXX`, dan `D-006` sudah berarti dua hal berbeda. Rujukan di dalam kode repo ini
sebagian besar menyebut nomor telanjang. Aturannya sudah ditulis di `03-DECISIONS-LOG.md`,
tetapi rujukan lama belum diperbaiki. `[belum dipanen]`

**A-5. Prosedur yang lahir dari kesalahan lebih berguna daripada prosedur yang lahir dari
bayangan.** `docs/PROSEDUR-TEST-JALAN.md` ditulis sebelum uji jalan, lalu dua butir
termahalnya justru ditambahkan **setelah** uji jalan, yaitu jarak laptop dan cara merekam
referensi kecepatan. Keduanya tidak terbayangkan sebelum dialami. `[belum dipanen]`

**A-6. Rekaman yang ada tetapi tidak pernah didokumentasikan.** `gelora-002` dan
`gelora-003`, masing-masing 216.454 dan 200.389 frame dari 22 September malam, tersimpan
di `captures/` tanpa satu pun dokumen bukti. Sebagian angkanya sudah dipakai di
`frame-loss.md`, jadi rekaman itu **bukan sampah**, hanya tidak punya jejak. Baru
ketahuan saat mengarsipkan dokumentasi per kendaraan. Pelajarannya: menghitung berkas di
`captures/` lawan dokumen di `docs/evidence/` adalah pemeriksaan murah yang menangkap
hutang semacam ini. `[belum dipanen]`

## B. Temuan teknis

**B-1. Interupsi CAN di flash.** `CONFIG_TWAI_ISR_IN_IRAM is not set` pada arduino-esp32
2.0.17 membuat setiap penulisan flash mematikan cache instruksi dan controller kehilangan
frame. Terukur 14,8 % melawan 0,0 %. Ini sifat framework, bukan bug kode, dan berlaku
untuk project ESP32 mana pun yang menulis flash sambil menerima CAN. `[belum dipanen]`

**B-2. `WiFiClient::availableForWrite()` tidak pernah diimplementasikan di ESP32.** Ia
mewarisi nilai 0 dari kelas `Print`, sehingga kode yang menunggunya tidak akan pernah
mengirim apa pun. Jalan keluarnya `::send(fd, ..., MSG_DONTWAIT)` langsung.
`[belum dipanen]`

**B-3. `ENOMEM` dari lwIP berarti "coba lagi", bukan "sambungan mati".** Memperlakukannya
sebagai fatal membuat alat menyambung ulang 338 kali dalam 30 detik dan kehilangan
sekitar 40 % bus, sementara penghitung drop ikut nol setiap sambungan baru. Hanya `EPIPE`,
`ECONNRESET`, `ENOTCONN`, dan `EBADF` yang benar-benar mengakhiri sesi. `[belum dipanen]`

**B-4. Transfer berkas lewat serial merusak data secara diam-diam.** Percobaan menarik
rekaman lewat `cat` di serial menghasilkan berkas yang lebih besar dari aslinya, berisi
deretan NUL dan baris yang terduplikasi separuh. HTTP lewat WiFi tidak bermasalah.
Berkas rusak itu sengaja disimpan sebagai bukti. `[belum dipanen]`

**B-5. `.gitignore` yang menyebut pola berkas, bukan folder.** `captures/*.log` membuat
8,8 MB bukti nyaris ikut ter-commit karena berkas lain di folder itu tidak tercakup.
`captures/**` dengan pengecualian eksplisit untuk lembar run adalah bentuk yang benar.
`[belum dipanen]`

## C. Cara melapor

**C-1. Menyatakan lubang di muka membuat sisanya bisa dipercaya.**
`docs/evidence/dfsk-gelora-e/gelora-004.md` menempatkan "14,9 % waktu tidak terekam" di bagian atas,
sebelum satu pun temuan disajikan, dan menjelaskan kenapa tiap bukti tetap tahan terhadap
lubang itu. Laporan yang menyembunyikan kelemahannya sendiri tidak bernilai.
`[belum dipanen]`

**C-2. Menurunkan status sebuah temuan adalah bagian dari laporan, bukan aib.** Suhu
controller naik jadi kandidat saat kendaraan diam, lalu diturunkan kembali jadi dugaan
setelah 47 menit berkendara tidak mengubahnya. Penurunan itu ditulis sejelas
kenaikannya. `[belum dipanen]`

## D. Yang sudah bekerja baik

**D-1. Penegakan aturan oleh mesin, bukan oleh prosa.** Gate listen-only menggagalkan
build, bukan menegur. Dalam tiga sesi kendaraan, aturan itu tidak pernah sekali pun
terlanggar. `[belum dipanen]`

**D-2. Satu sumber kebenaran untuk skematik.** `hardware/gen_schematic.py` membangkitkan
skematik, netlist, dan BOM dari satu berkas data, lalu `verify_schematic.py` membaca
hasilnya kembali dengan parser terpisah dan memeriksa 12 aturan keselamatan. Diuji dengan
tiga cacat yang sengaja disuntikkan, ketiganya tertangkap. `[belum dipanen]`

**D-3. Bukti terpisah dari tafsiran.** Aturan D-015 repo armada terbukti berharga saat
sebuah tafsiran harus dikoreksi: rekaman tidak perlu disentuh sama sekali.
`[belum dipanen]`

## E. Usulan untuk project berikutnya

**E-1. Slot microSD sejak papan pertama.** Menulis ke SD lewat SPI tidak mematikan cache
instruksi. Itu menyelesaikan kehilangan frame dan batas 12 MB sekaligus, tanpa menyentuh
framework. Rev A belum difabrikasi, jadi masih sempat. `[belum dipanen]`

**E-2. Rekam video panel instrumen sebagai referensi standar.** Sekali di awal, tunjukkan
jam laptop ke kamera supaya video dan rekaman bisa disejajarkan. Lebih murah daripada
orang kedua yang mengetik angka, dan jauh lebih akurat. `[belum dipanen]`

**E-3. Jadikan "adu dua sinyal di bus yang sama" sebagai kriteria penerimaan baku.**
Kecepatan melawan odometer memutuskan satuan tanpa referensi eksternal. Pola yang sama
berlaku di kendaraan lain: cari dua besaran yang secara fisika harus konsisten.
`[belum dipanen]`

## F. Catatan terbuka

**F-1. Token GitHub bocor ke keluaran terminal, 22 Sep 2026.** Kutip perintah yang keliru
di PowerShell membuat kredensial tercetak apa adanya. Token itu milik akun `wira97-tech`
dan harus dicabut di pengaturan akun. Jalur push sekarang memakai kredensial bernama dan
keluarannya disaring. Perlu dipastikan token lama benar-benar sudah dicabut.
`[belum dipanen]`

**F-2. Apakah alat ini perlu mendukung permintaan diagnostik?** Nilai seperti resistansi
isolasi tidak pernah disiarkan berkala, jadi alat ini tidak akan pernah melihatnya.
Menambah kemampuan meminta berarti **mengirim**, dan itu membatalkan jaminan yang membuat
alat ini boleh dipasang di kendaraan orang lain. Kalau memang dibutuhkan, tempatnya alat
kedua yang terpisah. `[belum dipanen]`
