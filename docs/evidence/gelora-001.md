# gelora-001: DFSK Gelora E, 22 Sep 2026

Rekaman pertama dari kendaraan listrik, dan pertama kali angka panel bisa
dicocokkan ke byte CAN dalam satu sesi yang sama.

## Asal-usul

| | |
|---|---|
| Kendaraan | DFSK Gelora E (van listrik) |
| Kondisi | mesin mati, kunci kontak ON, kendaraan diam sepanjang rekaman |
| Firmware | env `gelora-e-250k`, unit `GELORAE-TEST-01`, listen-only |
| Bitrate | **250 kbps** |
| Ditarik | 22 Sep 2026 lewat WiFi dari `10.215.81.4`, `tools/fetch_captures.py` |
| Berkas | `captures/gelora-001/can-000.log` … `can-052.log`, 194.219 frame |
| Keutuhan | ukuran tiap berkas cocok dengan yang dilaporkan alat, semua baris terbaca |

Rekaman berhenti sendiri saat mencapai batas 12 MB. Alat terus menerima frame
(608.917 saat terakhir dicek) tetapi tidak lagi menulis, persis seperti yang
dirancang setelah perbaikan 22 Sep. Tidak ada frame yang rusak karena flash penuh.

## Bitrate: 500 kbps salah, 250 kbps benar

Percobaan pertama di 500 kbps menghasilkan **1.579.745 bus error dalam 378 detik**
(sekitar 4.200 per detik) dan **nol frame**. Dashboard menyebutnya
"Wrong bitrate, most likely", dan itu tepat.

Setelah di-flash ke 250 kbps: 34 identifier, nol bus error.

Ribuan error itu tidak mengganggu mobil. Mode listen-only pada TWAI tidak pernah
mengirim bit dominan, termasuk error frame, jadi kesalahan bitrate hanya tercatat
di sisi alat.

## Bentuk bus

Semua 34 identifier memakai **29-bit extended**, bergaya J1939 seperti kendaraan
niaga, bukan 11-bit seperti mobil penumpang. Byte terakhir ID adalah alamat
sumber: terlihat ECU `01`, `02`, `03`, `06`, `8F`, `A6`, `D0`, `D5`, `F5`.

## Sinyal yang dipetakan

Rujukannya adalah layar CarInfo head unit dan panel instrumen, difoto pada
sesi yang sama.

| Nilai di layar | ID | Byte | Rumus | Cakupan |
|---|---|---|---|---|
| Odometer **30410** | `0x18FEDCD5` | b1–b2 LE | apa adanya | 2.997 / 2.997 |
| SOC **89,5 %** | `0x0CFF7D03` | b1 = 179 | % = nilai × 0,5 | 15.142 / 15.163 |
| Tegangan pack **357 V** | `0x0CFF7E03` | b2–b3 LE | volt apa adanya | 14.887 / 14.892 |
| SOH **97 %** | `0x0CFF7E03` | b1 = 97 | apa adanya | 14.892 / 14.892 |
| Suhu baterai **maks 30 °C** | `0x0CFF7E03` | b6 = 70 | °C = nilai − 40 | tetap |
| Suhu baterai **min 29 °C** | `0x0CFF7E03` | b7 = 69 | °C = nilai − 40 | tetap |
| Suhu controller **26 °C** | `0x0CFF1601` | b2 = 26 | apa adanya | 307 / 307 |

Keempat ID itu sudah diberi nama di alat lewat `POST /api/note`, jadi tampil di
kolom Name pada dashboard dan tercatat di `/notes/GELORAE-TEST-01/journal.log`.

### Belum pasti

- **Arus 0 A** → `0x0CFF7E03` b4–b5 LE = 1000. Offset 1000 adalah pola lazim
  supaya arus negatif (regeneratif) tetap muat dalam bilangan tanpa tanda.
  Tidak bisa dibedakan dari nilai tetap apa pun selama arusnya nol.
- **Tegangan sel tertinggi 3991 mV** → `0x0CFF7D03` b3–b4 LE. Foto menampilkan
  box 1 sel 1–6 di kisaran 3.972–3.980 V, jadi 3.991 mV masuk akal sebagai sel
  tertinggi di box lain, tetapi belum terbukti. Byte b2 dan b5 di frame yang
  sama berubah bersamanya, dan bentuknya cocok dengan nomor sel.

### Tidak ditemukan

Resistansi isolasi **7480 kΩ** tidak ada di seluruh 34 identifier, pada skala
mana pun yang dicoba. Kemungkinan besar nilai itu diminta head unit lewat
permintaan diagnostik, bukan disiarkan berkala. Alat ini tidak pernah meminta.

## Kenapa semuanya masih hipotesis

Kendaraan **diam** sepanjang rekaman, jadi tidak satu pun nilai ini bergerak.
Field mana pun yang kebetulan berisi angka yang benar dan diam akan terlihat
sama meyakinkannya.

Pembuktiannya, di run berikutnya sambil berjalan:

| Kandidat | Terbukti kalau |
|---|---|
| Odometer | naik sesuai jarak yang ditempuh |
| SOC | turun mengikuti angka di layar |
| Arus | keluar dari 1000, dan berbalik arah saat regeneratif |
| Suhu | ikut naik setelah pemakaian |
| Tegangan sel | ikut turun saat akselerasi berat |

## Catatan jam

Jam di panel instrumen menunjukkan 23:18, sedangkan head unit menunjukkan 18:48
pada sesi yang sama. Salah satu dari keduanya tidak disetel. Ini perlu diingat
kalau nanti jam kendaraan dipakai untuk menyelaraskan rekaman dengan video.

## Batas pemakaian

Ini temuan tentang **DFSK**. Armada memakai Wuling, jadi tidak satu pun ID di
sini boleh masuk `signals.cfg` armada. Yang bisa dibawa adalah **caranya**:
bitrate 250 kbps, identifier extended, dan susunan frame BMS yang menaruh SOC,
tegangan pack, SOH, dan suhu dalam beberapa frame proprietary berdekatan.
