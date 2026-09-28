# Dashboard: menyaring identifier dan panel nilai kendaraan

Sejak 23 September 2026 dashboard tidak lagi menampilkan seluruh identifier
begitu saja. Ada tiga mode, dan satu panel di atas yang menampilkan nilai
kendaraan sebagai angka, bukan hex.

## Yang perlu diluruskan lebih dulu

**Penyaring ini tidak memperbaiki frame yang hilang.** Frame hilang karena
interupsi CAN berada di flash, sehingga setiap penulisan flash mematikan cache
instruksi dan FIFO controller meluap. Terukur 14,8 % saat menulis ke flash
melawan 0,0 % saat dialirkan lewat WiFi. Rinciannya di
[evidence/00-umum/frame-loss.md](evidence/00-umum/frame-loss.md).

Menyembunyikan baris di dashboard tidak menyentuh sebab itu sama sekali. Yang
menyentuh sebab itu hanya dua: **jangan menulis flash saat merekam**, dan kelak
**kartu microSD** pada papan rev B.

**Penyaring ini juga tidak menyaring perekaman.** Alat tetap menerima, menghitung,
dan merekam seluruh frame di bus. Itu disengaja: identifier yang belum dinamai
siapa pun justru itu yang dibutuhkan sesi pemetaan berikutnya. Yang disaring
hanya apa yang **dikirim ke halaman dan digambar**.

Yang memang dihemat nyata tapi kelas dua: pada bus 34 identifier dengan 5 yang
bertanda, dokumen `/api/state` menyusut sekitar tiga perempat, dan ikut menyusut
pula konversi hex di alat, penulisan soket, dan kerja HP saat mengurai JSON.

## Tiga mode

| Mode | Yang ditampilkan |
|---|---|
| **TDS** | hanya identifier yang catatannya diawali `#tds` |
| **Named** | setiap identifier yang punya catatan |
| **All** | seluruh identifier di bus, yang dibutuhkan saat memetakan |

Pilihan disimpan di browser. Halaman selalu menyebutkan berapa identifier yang
disembunyikan, karena penyaring yang diam-diam adalah cara paling mudah membuat
orang menyimpulkan sebuah ECU mati padahal tidak.

**Kalau kendaraan belum pernah dipetakan**, belum ada catatan sama sekali, jadi
mode TDS dan Named akan kosong. Halaman turun sendiri ke mode yang berisi,
selama pemakainya belum pernah memilih mode secara sadar. Jadi kendaraan baru
tidak pernah tampil seperti bus mati.

**Signal probe hanya menawarkan identifier yang sedang tampil.** Saat memetakan,
pindah ke All.

## Menandai sebuah sinyal untuk TDS

Tandanya adalah awalan `#tds` di **teks catatan** identifier itu, bukan kolom
baru dan bukan berkas baru. Alasannya: tidak menambah penyimpanan, tidak
mengubah format berkas, dan operator bisa memasang atau mencabutnya dengan
mengetik.

```
#tds Kecepatan {le16(4)/256} km/jam
```

Di tabel, tanda itu tampil sebagai lencana kecil dan tidak diulang sebagai teks.

## Panel Vehicle

Setiap catatan bertanda `#tds` **yang memuat rumus** masuk ke panel paling atas.
Tidak ada satu pun identifier yang ditulis di dalam kode halaman, jadi memetakan
sinyal baru cukup dengan menamainya di tabel dan menandainya. Panel itu ikut
muncul sendiri.

### Susunan panel

Susunannya mengikuti gambar acuan dari Direktur (28 September 2026): kecepatan
pada dial besar di tengah, odometer dan motor di kiri, bacaan baterai bertumpuk
di kanan.

**Yang menentukan sebuah bacaan masuk zona mana adalah satuannya, bukan
identifier-nya.** Halaman tetap tidak memuat apa pun yang khusus satu kendaraan:

| Satuan | Tempatnya |
|---|---|
| `km/jam`, `km/h`, `mph`, `m/s` | dial besar di tengah |
| `rpm` | dial kecil di kiri |
| `km`, `m`, `mi` | angka di kiri, di atas dial motor |
| `%` | bar 0-100 di puncak tumpukan kanan |
| lainnya | kartu di tumpukan kanan, dengan jejak 40 detik di bawahnya |

Tanpa kecepatan **dan** tanpa persen tidak ada yang bisa dijadikan dial, jadi
panelnya kembali jadi sebaris kartu biasa. Kendaraan yang belum dipetakan tidak
pernah melihat dial kosong.

### Satu catatan bisa memuat beberapa bacaan

Alat menyimpan **satu catatan per identifier**, sedangkan identifier pack
mengirim tegangan, arus dan suhu sekaligus. Jadi ketiganya datang tertulis
sebagai satu kalimat:

```
#tds Pack {le16(2)} V arus {le16(4)-1000} A suhu {b6-40} C
```

Setiap rumus di kalimat itu jadi satu bacaan dengan kartunya sendiri. Kata-kata
sebelum rumus pertama menamainya; **teks di antara dua rumus dimiliki keduanya**,
kata pertamanya adalah satuan yang menutup bacaan sebelumnya dan sisanya menamai
bacaan sesudahnya. Jadi catatan di atas menghasilkan Pack 346 V, arus 24 A, dan
suhu 31 C, masing-masing jadi kartu.

Itu sebabnya spasi di catatan berarti. `{le16(2)}V arus` membuat satuannya
terbaca `V` dan nama berikutnya `arus`, sama saja, tetapi `{le16(2)} Volt pack`
akan membuat satuan `Volt` dan nama berikutnya `pack`. Tulis satuan sebagai satu
kata.

### Skala dial

Tidak ada kendaraan di sini yang batas maksimumnya sudah terbukti, jadi dial
**tidak pernah diberi tahu** skalanya. Ia memakai bacaan tertinggi sepanjang
sesi, dibulatkan ke atas dengan kelonggaran 15 persen, lalu **mencetak angka
ujungnya sendiri** di dial. Skalanya hanya membesar, tidak pernah mengecil
kembali: jarum yang berarti satu hal sekarang dan hal lain semenit kemudian
lebih buruk daripada jarum yang kemurahan.

Pengecualian satu-satunya adalah `%`, yang rentangnya dinyatakan oleh satuannya
sendiri, jadi bar-nya tetap 0 sampai 100.

**Tidak ada angka yang diwarnai karena nilainya.** Biru untuk kecepatan dan
jingga untuk motor adalah warna skala, diambil dari gambar acuan, dan tetap
tinggal di skala. Di sebuah instrumen, angka merah berarti alarm, dan alat ini
tidak punya ambang untuk membunyikannya.

### Angka yang digambar

Angka besar **digambar, bukan diketik dengan font**. Font tujuh segmen berarti
satu berkas lagi di flash pada alat yang menyajikan halamannya dari flash,
sedangkan seluruh isinya tabel bar mana yang menyala plus dua heksagon. Angka
`1` dapat sel lebih sempit, seperti pada setiap cluster digital, supaya `30461`
tidak terbaca `3046 1`.

Dial membulatkan lebih keras daripada kartu: paling banyak satu desimal, dan
tanpa desimal di atas 100. Tiga desimal adalah inti persoalan pada tegangan sel
dan kebisingan pada spidometer.

### Jejak 40 detik

Setiap kartu biasa membawa empat puluh detik terakhir dari angkanya. Di atas bus
pertanyaannya hampir tidak pernah "berapa angkanya" melainkan apakah ia bergerak,
ke arah mana, dan melompat atau merayap. Riwayatnya disimpan di halaman, bukan di
alat.

Jejak menolak memperbesar skala melewati dua persen dari angka yang dibaca,
supaya tegangan yang membulat dari 350,0 ke 350,1 tidak menggambar pegunungan.
Sinyal yang memang datar digambar tanpa isian.

Identifier yang berhenti bicara lebih dari dua detik membuat kartunya meredup dan
memunculkan kata `SILENT`. Angka terakhir tetap terbaca, karena itu informasi;
yang tidak boleh adalah angka itu terlihat seolah baru.

## Memasang catatan sekali jalan

Satu berkas per kendaraan, di folder buktinya, lalu:

```
python tools/apply_notes.py --host <ip alat> docs/evidence/dfsk-gelora-e/notes.tsv
python tools/apply_notes.py --host <ip alat> --dry-run <berkas>     # periksa saja
```

Alat itu menolak seluruh berkas kalau ada satu catatan melewati batas 60 byte,
supaya tidak ada berkas yang terpasang separuh. Yang dicetak adalah **echo dari
alat**, yaitu teks yang benar-benar tersimpan, bukan teks yang dikirim. Catatan
yang terpotong tidak boleh terlihat seperti berhasil.

Baris dengan catatan kosong **menghapus** nama identifier itu. Itu cara menarik
kembali pemetaan yang ternyata salah.

## Catatan adalah tafsiran, bukan bukti

Catatan tinggal di `/notes/<UNIT_ID>/` pada alat, tidak pernah di dalam berkas
rekaman. Tebakan yang salah dikoreksi tanpa menyentuh rekaman. Ini D-015 di
`bmt-can-bus-telemetry`, dan berlaku penuh di sini.

Konsekuensinya untuk penyaring: **daftar yang tampil di dashboard bukan
pernyataan tentang kebenaran sebuah pemetaan.** Status sebenarnya, yaitu dugaan,
kandidat, terbukti, atau gugur, ada di README folder kendaraan dan di dokumen
bukti. Sebuah identifier bisa saja bertanda `#tds` sementara statusnya masih
kandidat.

## Kalau alat diganti Orange Pi 5

Halaman ini adalah HTML biasa plus satu endpoint JSON, jadi ia berpindah apa
adanya. Yang hilang justru batasannya: pada Linux dengan SocketCAN tidak ada
lagi interupsi yang terhenti oleh penulisan flash, tidak ada batas 12 MB, dan
penyaring ini kembali murni soal keterbacaan layar, bukan soal beban.
