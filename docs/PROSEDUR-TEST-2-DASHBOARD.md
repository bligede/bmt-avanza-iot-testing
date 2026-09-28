# Prosedur test jalan dua dashboard

Untuk uji jalan 29 September 2026 di DFSK Gelora E, atas instruksi Direktur BMT
28 September: alat menampilkan dua dashboard sekaligus dan lokal dulu, dan tombol
mulai/akhiri argo dipencet manual, orang yang memencet berperan sebagai penumpang.

Ini melengkapi `PROSEDUR-TEST-JALAN.md`, bukan menggantikannya. Yang di sana
tentang merekam bus; yang di sini tentang menonton hasilnya hidup-hidup.

## Apa yang berubah di alat

| | Alamat | Untuk siapa | Isinya |
|---|---|---|---|
| Dashboard teknis | `http://<ip>/` | teknisi | status bus, semua identifier, frame mentah, panel Vehicle berisi nilai hasil rumus di catatan |
| Dashboard argo | `http://<ip>/argo` | pengemudi dan penumpang | layar SELARIDE: tarif, jarak isi, durasi, kecepatan, daya baterai, tombol trip |

Keduanya dari satu alat, dan boleh dibuka bersamaan dari dua gawai.

Endpoint baru: `GET /api/signals` (nilai ter-decode plus keadaan argo) dan
`POST /api/argo?action=start|stop`.

## Melihat kedua layar tanpa alat

Alat sering ada di mobil, sedangkan layarnya perlu dilihat di meja. `tools/fake_device.py`
adalah alat tiruan: satu berkas Python yang melayani kedua halaman dan semua endpoint
yang mereka minta, dengan satu kendaraan tiruan yang menyetir keduanya.

```
python tools/fake_device.py 8131 --lan
```

Tanpa `--lan` hanya bisa dibuka dari komputer ini. Dengan `--lan`, dua gawai di WiFi
yang sama bisa membuka dua alamat itu, persis susunan yang dipakai di jalan besok.

Kendaraan tiruannya menjalani satu rute pendek yang berulang: berangkat, jalan lepas,
macet, merayap di bawah ambang tunggu, lalu berhenti. Merayapnya sengaja ada, karena
"di bawah 5 km/jam tapi masih bergerak" adalah kasus yang tidak akan pernah muncul di
rute yang cuma berhenti mati.

Nilainya **dikodekan kembali menjadi byte CAN** seperti Gelora E mengirimnya, jadi panel
Vehicle di dashboard teknis benar-benar menjalankan rumus `{le16(4)/256}` dari
`notes.tsv`, bukan menerima angka jadi. Kalau rumus di catatan dan tabel di
`src/SignalDecoder.cpp` berbeda, pratinjau ini yang memperlihatkannya lebih dulu.

**Yang tidak bisa dijawab pratinjau ini:** beban CPU alat, `rx_overrun` saat dua gawai
membuka dua halaman sekaligus, dan bagaimana ponsel berlaku di kendaraan yang bergerak.
Itu semua hanya bisa diukur di jalan. Pratinjau ini menjawab "layarnya jalan dan
hitungannya benar", tidak lebih.

## Sebelum berangkat

1. **Pasang catatan identifier.** Panel Vehicle di dashboard teknis menghitung
   nilainya dari rumus yang ada di catatan, bukan dari firmware, jadi tanpa
   langkah ini panel itu kosong:

   ```
   python tools/apply_notes.py --host <ip alat> docs/evidence/dfsk-gelora-e/notes.tsv
   ```

2. **Buka kedua alamat, satu di tiap gawai.** Pastikan keduanya hidup sebelum
   kendaraan bergerak. Kalau salah satu tidak mau terbuka, periksa WiFi alat
   dulu, bukan halamannya.

3. **Jangan lupa rekamannya.** Uji ini soal tampilan, tetapi rekaman mentah tetap
   bukti. Nyalakan seperti biasa dari dashboard teknis.

4. **Cek angka pertama.** Di dashboard argo, sebelum argo dimulai, tarif harus
   `Rp 0` dan tulisannya `Argo berhenti, kilometer kosong tidak ditagih`. Kalau
   sudah ada angka sebelum dipencet, hentikan dan laporkan: itu bukan kesalahan
   tampilan, itu argo menagih tanpa penumpang.

## Urutan jalan

1. **Berangkat dengan argo mati.** Berkendara beberapa ratus meter. Ini kilometer
   kosong, dan tarifnya harus tetap `Rp 0`. Kalau bertambah, hentikan uji.
2. **Berhenti, pencet `Mulai trip`.** Tombolnya berubah jadi `Akhiri trip` dan
   pilnya berubah jadi `TERISI`.
3. **Jalan normal.** Perhatikan `Argometer` bertambah tiap 100 meter, tidak
   mulus. Memang begitu: yang ditagih adalah detak, bukan jarak berjalan.
4. **Berhenti di lampu merah atau macet.** Di bawah 5 km/jam, baris
   `Waktu tunggu` yang berjalan dan `Argometer` berhenti. Dua-duanya tidak boleh
   berjalan bersamaan. Ini yang paling perlu dilihat langsung.
5. **Merayap pelan, di bawah 5 km/jam tapi bergerak.** Waktu tunggu harus tetap
   berjalan, jarak tetap diam. Ambangnya kecepatan, bukan berhenti.
6. **Pencet `Akhiri trip`.** Angkanya berhenti di tempat dan tetap terbaca, karena
   di situlah penumpang membayar.
7. **Jalan lagi dengan argo mati**, pastikan tarifnya tidak bertambah.

## Yang dicatat, dan kenapa

| Yang dicatat | Kenapa |
|---|---|
| Odometer di panel kendaraan saat `Mulai` dan saat `Akhiri` | jarak isi di layar harus sama dengan selisihnya, dibulatkan ke bawah per 100 m |
| Tarif akhir, jarak isi, menit tunggu | kalikan sendiri: `km x 8.200 + menit x 5.000`. Kalau tidak sama, layar berdebat dengan dirinya sendiri |
| Apa yang terjadi saat dua gawai membuka dua halaman bersamaan | alat ini melayani satu permintaan pada satu waktu |
| `rx_overrun`, `rx_missed`, dan status kesehatan di dashboard teknis | dua dashboard hidup adalah beban baru yang belum pernah diukur |
| Apakah panel Vehicle dan layar argo menunjukkan angka yang sama | keduanya punya sumber rumus yang berbeda, lihat bagian di bawah |

## Dua sumber rumus, dan ini harus diawasi

Nilai di **panel Vehicle** dihitung dari rumus yang ditulis di catatan identifier,
di dalam alat, dan bisa diubah siapa pun dari halaman itu. Nilai di **layar argo**
dihitung oleh tabel di firmware (`src/SignalDecoder.cpp`), yang hanya berubah lewat
build baru.

Keduanya berasal dari bukti yang sama (`docs/evidence/dfsk-gelora-e/gelora-004.md`),
jadi seharusnya selalu sama. Kalau berbeda di jalan, yang salah bukan salah satunya
begitu saja: catat keduanya, jangan diperbaiki di tempat, dan bandingkan dengan
berkas bukti setelah di meja.

## Batas yang perlu diketahui sebelum, bukan sesudah

- **Argo tidak disimpan.** Alat reboot di tengah trip berarti tarifnya hilang dan
  harus dipencet `Mulai` lagi. Disengaja: alternatifnya menulis ke filesystem yang
  menyimpan rekaman CAN, dan tidak ada fitur tampilan yang sepadan dengan risiko itu.
- **Jarak ditagih dari odometer**, bukan dari integral kecepatan. Odometer DFSK
  beresolusi 1 km, jadi jarak isi bertambah dalam langkah 1 km, bukan mulus.
  Kalau ini terasa kasar di jalan, itu temuan yang bagus dan perlu dicatat, bukan
  bug.
- **Waktu tunggu Rp 5.000 per menit belum disahkan** Direktur dan Direktur Utama,
  dan layar menandainya. Angkanya berjalan supaya bisa dilihat, bukan supaya
  ditagihkan.
- **Identitas pengemudi di layar adalah contoh**, bukan data orang.
- **Semua angka ini milik DFSK Gelora E**, kendaraan uji. Armada memakai Wuling dan
  belum pernah dipetakan.
