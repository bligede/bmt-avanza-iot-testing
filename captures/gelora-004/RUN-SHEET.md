# Lembar run gelora-004: uji kecepatan DFSK Gelora E

Tanggal: 23 Sep 2026
Alat: GELORAE-TEST-01, 250 kbps, listen-only, aliran WiFi ke laptop

## Sebelum berangkat (dari foto)

| | |
|---|---|
| Odometer | 30423 |
| SOC | 74,0 % |
| Tegangan pack | 350 V |
| Arus | 1 A (mentah 1001) |
| SOH | 97 % |
| Suhu baterai maks / min | 31 / 30 °C |
| Suhu controller | 26 °C |
| Sel tertinggi / terendah | 3911 / 3650 mV |
| Jam panel instrumen | 18:46 (tidak disetel) |
| Jam head unit | 14:18 |

## Tahapan

| Jam laptop | Tahap | Catatan |
|---|---|---|
| 14:27:41 – 14:28:07 | **penanda: rem 5 kali** | jendela pengamatan 25 detik |
| 14:34:41 – 14:35:53 | **20 km/jam ditahan** (panel 10–12 mph, ~16–19 km/jam) | |
| 14:35:53 – 14:36:54 | **berhenti penuh** (jendela nol) | |
|  | 40 km/jam ditahan |  |
|  | berhenti penuh |  |
|  | 60 km/jam ditahan |  |
|  | akselerasi kuat |  |
|  | pengereman regeneratif |  |
|  | jalan lurus 2 km |  |

## Kejadian selama run

- 14:44:14: alat sempat hilang dari jaringan, rekaman terputus lalu tersambung lagi. Uptime alat saat kembali: 1655 s.
- 14:46:47: mulai berhenti penuh kedua (jendela nol setelah sambungan pulih)
- 15:06:45: uji arus: mulai melaju dari nol, lalu melambat sendiri tanpa rem
- 14:56:05 – 14:58:35: pengemudi menyebutkan kecepatan lewat pesan tiap ~5 detik (`speed-ref.csv`)

## Hasil rekaman

| | |
|---|---|
| Rentang | 14:23:40 – 15:10:23, 46,7 menit |
| Frame tersimpan | 1.525.681, 34 identifier, rata-rata 544 frame/detik |
| Ukuran | 97 MB, `can-000.log` … `can-379.log` |
| Tautan putus | 30 kali lebih dari 1 detik, total 418 detik (**14,9 % waktu tidak terekam**) |
| Lubang terbesar | 15:07:04 selama 121 detik; 14:39:45 selama 118 detik |

Penyebab lubang: laptop terlalu jauh dari alat dan dari hotspot. Untuk run
berikutnya, dekatkan keduanya dan tutup dashboard di HP.

## Setelah berhenti (dari bus, bukan foto)

| | |
|---|---|
| Odometer | 30427 (naik 4 km; layar sempat terbaca 30426 saat masih berjalan) |
| SOC | 71,0 % (turun 3,0 poin) |
| Tegangan pack | 348 V, sempat melorot ke 333 V saat arus puncak |
| Arus | −36 A sampai +133 A sepanjang sesi |
| Suhu baterai maks / min | 32 / 30 °C |
| Suhu controller | 26 °C, **tidak berubah sama sekali** |
| Kecepatan tertinggi | 48 km/jam |
| Putaran motor tertinggi | 4143 rpm |

Analisisnya di [`docs/evidence/gelora-004.md`](../../docs/evidence/gelora-004.md).
