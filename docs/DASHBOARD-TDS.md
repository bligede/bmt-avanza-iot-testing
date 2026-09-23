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

Setiap catatan bertanda `#tds` **yang memuat rumus** menjadi satu kartu di panel
paling atas: angkanya besar, namanya kecil di bawahnya. Tidak ada satu pun
identifier yang ditulis di dalam kode halaman, jadi memetakan sinyal baru cukup
dengan menamainya di tabel dan menandainya. Panel itu ikut muncul sendiri.

Catatan bertanda tetapi tanpa rumus tetap lolos penyaring dan tampil di tabel,
hanya tidak jadi kartu, karena tidak ada angka yang bisa ditampilkan.

Kartu membulatkan angka lebih agresif daripada tabel: nilai di bawah 10 tetap
tiga desimal, karena 3,979 V adalah tegangan sel yang justru desimal ketiganya
yang penting; nilai di atasnya cukup satu desimal, karena 47,016 km/jam hanya
kebisingan di sekitar 47.

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
