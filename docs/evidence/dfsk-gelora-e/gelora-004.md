# gelora-004: DFSK Gelora E sambil berjalan, 23 Sep 2026

Rekaman pertama dengan kendaraan **bergerak**. Ini yang menaikkan status
pemetaan `gelora-001` dari dugaan menjadi terbukti, karena nilai yang diam tidak
membuktikan apa pun: field mana pun yang kebetulan berisi angka benar dan tidak
berubah akan terlihat sama meyakinkannya.

## Asal-usul

| | |
|---|---|
| Kendaraan | DFSK Gelora E (van listrik), dikemudikan di jalan umum |
| Firmware | env `gelora-e-250k`, unit `GELORAE-TEST-01`, listen-only |
| Bitrate | 250 kbps |
| Cara merekam | **dialirkan lewat WiFi ke laptop di dalam mobil**, penulisan flash dijeda |
| Waktu | 14:23:40 – 15:10:23 waktu laptop, 46,7 menit |
| Berkas | `captures/gelora-004/can-000.log` … `can-379.log`, 97 MB |
| Isi | **1.525.681 frame**, 34 identifier, rata-rata 544 frame/detik |
| Lembar run | `captures/gelora-004/RUN-SHEET.md` |

Delapan kali lipat isi rekaman `gelora-001` yang berhenti di batas 12 MB flash,
dan tanpa kehilangan frame akibat penulisan flash. Itu memang alasan cara
merekam ini dibuat (lihat `frame-loss.md`).

## Lubang di rekaman, dinyatakan di muka

Tautan WiFi putus **30 kali** dengan jeda lebih dari satu detik, total **418
detik**, atau **14,9 % dari rentang waktu sesi**. Empat lubang terbesar:

| Mulai | Lama |
|---|---|
| 15:07:04 | 121 detik |
| 14:39:45 | 118 detik |
| 14:38:17 | 59 detik |
| 14:42:06 | 46 detik |

Setiap putus ditandai baris `# LINK` di dalam berkas, tidak pernah ditutup
diam-diam. Konsekuensinya jujur: **rekaman ini sampel, bukan salinan utuh
perjalanan.** Untuk memetakan nilai yang disiarkan berkala hal itu tidak
mengubah kesimpulan, dan setiap bukti di bawah ini disusun supaya tahan terhadap
lubang tersebut. Untuk analisis yang bergantung pada urutan antar-frame, rekaman
ini tidak layak dipakai.

Penyebab yang sudah diketahui: laptop berada agak jauh dari alat dan dari
hotspot. Pada run berikutnya, dekatkan keduanya.

## Yang berhasil dibuktikan

### 1. Kecepatan: `0x18FEDCD5` b0, satuan km/jam

Buktinya berdiri sendiri, tanpa perlu foto maupun laporan pengemudi: **kecepatan
diintegrasikan terhadap waktu, lalu dibandingkan dengan odometer di bus yang
sama.**

| | |
|---|---|
| Kecepatan diintegrasikan sepanjang rekaman | **4,031 km** |
| Odometer CAN naik | 30423 → 30427, yaitu **4 km** |
| Kalau field itu mph, integralnya jadi | 6,49 km, butuh odometer naik 6–7 |

Angka odometer bertambah satu pada 14:29:41, 14:35:11, 14:56:35, dan 15:09:47.
Karena 14,9 % waktu tidak terekam, 4,031 km adalah **batas bawah**, dan
jarak sebenarnya ada di antara 4 dan 5 km. Pembacaan km/jam muat di situ,
pembacaan mph tidak. **Satuannya km/jam, dan mph tertolak.**

Perhatikan bahwa ini sekaligus membuktikan **odometer** `0x18FEDCD5` b1–b2 LE:
angkanya bergerak, arahnya benar, dan lajunya cocok dengan kecepatan di frame
yang sama.

Rentang terukur: 0 sampai **48 km/jam**.

### 2. Kecepatan halus: `0x18FFDC01` b4–b5 LE dibagi 256

Field kedua yang membawa kecepatan yang sama dengan resolusi 1/256 km/jam.
Korelasi terhadap field bulat **r = 0,9998**, kemiringan 256,1 per km/jam.

Yang menguatkan bahwa ini bukan kebetulan: maksimumnya **48,02 km/jam**
sementara field bulat berhenti di 48, dan integrasinya menghasilkan 4,031 km
melawan 3,66 km dari field bulat. Selisih 0,37 km itu persis sebesar bias
pembulatan ke bawah yang memang harus ada pada field bulat. Dua field saling
menjelaskan kesalahan masing-masing.

Untuk TDS, **field inilah yang dipakai**, bukan yang bulat: 1/256 km/jam cukup
halus untuk mendeteksi kendaraan mulai bergerak dan berhenti dengan rapi.

### 3. Putaran motor: `0x0CFF7902` b4–b5 LE dikurangi 12000

Ini yang paling tajam buktinya, karena ada satu angka di layar untuk diadu.

Foto panel instrumen pukul **15:09** menunjukkan **2045 rpm**. Di rekaman, pada
**15:09:07**, field itu bernilai mentah 14046, yaitu **2046 rpm**. **Meleset
satu rpm.**

Offsetnya ditentukan dengan regresi terhadap kecepatan yang sudah terbukti,
bukan dengan mencocok-cocokkan ke satu foto:

```
rpm_mentah = 87,77 x kecepatan + 12002,8      (11.765 pasangan)
nilai saat kendaraan diam   = 11.955 .. 12.109, rata-rata 12.000
```

Jadi **rpm = nilai − 12000**, dan 87,8 rpm motor per km/jam. Rentang terukur
−45 sampai **4143 rpm**; nilai negatif kecil itu getaran saat diam, dan tidak
ada satu pun sampel di bawah −100, jadi rekaman ini tidak pernah memuat mundur.

Pemeriksaan kewajaran mekanis: dengan ban 185/65R15 (keliling sekitar 1,90 m),
87,8 rpm per km/jam berarti rasio reduksi **10,0 : 1**. Itu angka yang lazim
untuk EV satu kecepatan. Ukuran ban di sini adalah asumsi, jadi perhitungan ini
hanya penguat, bukan bukti.

### 4. Arus: `0x0CFF7E03` b4–b5 LE dikurangi 1000, satuan ampere

Di `gelora-001` ini belum pasti, karena arusnya tetap 1000 sepanjang rekaman.
Sekarang terbukti, dan yang membuktikannya adalah **arah**:

| Jam | Kejadian | Arus | Kecepatan |
|---|---|---|---|
| 15:06:54 | menarik | **+133 A** | 47 km/jam |
| 15:06:55 | pedal dilepas | **−36 A** | 46 km/jam |

Satu detik, dari menarik penuh ke arus **berbalik arah**. Tidak ada tafsiran
lain untuk itu selain **pengereman regeneratif**, dan itu sekaligus menjelaskan
kenapa offset 1000 ada: supaya arus negatif muat dalam bilangan tanpa tanda.
Arus negatif tercatat pada 4,6 % waktu.

Rentang sepanjang sesi: −36 A sampai +133 A.

### 5. Yang ikut bergerak dan cocok

Semuanya dari `gelora-001`, sekarang terlihat berubah ke arah yang benar:

| Nilai | Awal | Akhir | Arah |
|---|---|---|---|
| SOC `0x0CFF7D03` b1 × 0,5 | 74,0 % | 71,0 % | turun karena dipakai |
| Tegangan pack `0x0CFF7E03` b2–b3 LE | 350 V | 348 V | turun sampai 333 V saat menarik |
| Suhu baterai maks `0x0CFF7E03` b6 − 40 | 31 °C | 32 °C | naik setelah dipakai |
| SOH `0x0CFF7E03` b1 | 97 % | 97 % | memang harus tetap |

Tegangan pack yang **melorot ke 333 V saat arus memuncak lalu pulih** adalah
perilaku baterai yang benar, dan itu mengunci pemetaan tegangan dan arus
sekaligus.

## Referensi lisan pengemudi: tidak dipakai

Selama uji, pengemudi menyebutkan kecepatan lewat pesan setiap sekitar 5 detik
(tersimpan di `speed-ref.csv`, 23 titik). Data itu **tidak dipakai sebagai
bukti**, dan alasannya perlu dicatat supaya tidak diulang:

- Korelasi terbaik hanya **r = 0,72**, dan baru tercapai setelah digeser 20
  detik. Jeda antara membaca panel, mengetik, dan pesan sampai tidak tetap.
- Kemiringannya keluar 0,64 terhadap mph yang dilaporkan, sedangkan km/jam
  menuntut 1,61 dan mph menuntut 1,00. Tidak memutuskan apa pun.

Pelajarannya untuk prosedur: **referensi kecepatan harus direkam, bukan
diketik.** Video panel instrumen dengan jam laptop terlihat di layar akan jauh
lebih berguna daripada 23 pesan. Sementara itu, odometer di bus yang sama
ternyata referensi yang lebih baik daripada manusia, dan gratis.

## Ringkasan status setelah run ini

| Sinyal | ID | Rumus | Status |
|---|---|---|---|
| Kecepatan | `0x18FEDCD5` b0 | km/jam apa adanya | **terbukti** |
| Kecepatan halus | `0x18FFDC01` b4–b5 LE | km/jam = nilai ÷ 256 | **terbukti** |
| Odometer | `0x18FEDCD5` b1–b2 LE | km apa adanya | **terbukti** |
| Putaran motor | `0x0CFF7902` b4–b5 LE | rpm = nilai − 12000 | **terbukti** |
| Arus | `0x0CFF7E03` b4–b5 LE | A = nilai − 1000 | **terbukti** |
| SOC | `0x0CFF7D03` b1 | % = nilai × 0,5 | **terbukti** |
| Tegangan pack | `0x0CFF7E03` b2–b3 LE | volt apa adanya | **terbukti** |
| Suhu baterai maks/min | `0x0CFF7E03` b6 / b7 | °C = nilai − 40 | **terbukti** |
| SOH | `0x0CFF7E03` b1 | % apa adanya | tetap, belum diuji |
| Suhu controller | `0x0CFF1601` b2 | °C apa adanya | **tetap 26 °C sepanjang sesi, dugaan** |
| Tegangan 90 sel | `0x0CFF8203` | multiplex, mV | terbukti di `gelora-001` |
| Suhu 30 sensor | `0x0CFF8303` | multiplex, °C − 40 | terbukti di `gelora-001` |

Suhu controller perlu dicatat: nilainya **tidak bergerak sama sekali** selama 47
menit berkendara, termasuk saat arus 133 A. Kalau benar itu suhu controller,
seharusnya naik. Statusnya turun kembali menjadi dugaan.

## Paket baterai: selisih sel melebar

`gelora-001` mencatat sel 7 di 3,778 V melawan mayoritas di 3,96–3,99 V. Foto
sebelum uji jalan ini menunjukkan sel terendah **3,650 V** dan tertinggi 3,911 V,
jadi selisihnya kini sekitar **260 mV**, naik dari sekitar 200 mV sehari
sebelumnya.

Ini temuan tentang kendaraannya, bukan tentang alat, dan sebaiknya disampaikan
ke pemilik. Layar CarInfo hanya menampilkan enam sel pertama, semuanya sehat,
jadi selisih ini tidak terlihat dari dalam mobil.

## Batas pemakaian

Sama seperti `gelora-001`: ini temuan tentang **DFSK**. Armada memakai Wuling,
jadi tidak satu pun ID di sini boleh masuk `signals.cfg` armada. Yang terbawa ke
TDS adalah **caranya membuktikan**, dan itu yang paling berharga dari run ini:

1. **Integrasi kecepatan terhadap odometer di bus yang sama** memutuskan satuan
   tanpa perlu referensi manusia sama sekali.
2. **Arah, bukan nilai**, yang membuktikan arus. Satu peristiwa melepas pedal
   lebih kuat daripada satu jam data yang tenang.
3. **Regresi terhadap sinyal yang sudah terbukti** memberi offset yang bulat dan
   masuk akal, dan foto hanya dipakai untuk memeriksanya, bukan untuk
   menurunkannya.
