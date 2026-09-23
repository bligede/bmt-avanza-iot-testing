# DFSK Gelora E

Van listrik. Kendaraan uji, **bukan** kendaraan armada.

| | |
|---|---|
| Env PlatformIO | `gelora-e-250k` |
| UNIT_ID | `GELORAE-TEST-01` |
| Bitrate | **250 kbps**, terukur. 500 kbps menghasilkan nol frame |
| Bentuk identifier | 29-bit extended seluruhnya, bergaya J1939 |
| Jumlah identifier | 34 |
| Alamat ECU terlihat | `01`, `02`, `03`, `06`, `8F`, `A6`, `D0`, `D5`, `F5` |

## Sesi

| Run | Tanggal | Kondisi | Isi | Berkas |
|---|---|---|---|---|
| `gelora-001` | 22 Sep 2026 | diam, kontak ON | 194.219 frame, 12 nilai dipetakan | [gelora-001.md](gelora-001.md) |
| `gelora-004` | 23 Sep 2026 | berjalan, 47 menit | 1.525.681 frame, pemetaan dibuktikan | [gelora-004.md](gelora-004.md) |

Laporan pengujian 22 September yang lebih luas, termasuk soal bitrate dan bentuk bus, ada
di [TESTING-2026-09-22.md](TESTING-2026-09-22.md).

### Dua sesi yang belum punya dokumen bukti

`gelora-002` dan `gelora-003` **ada dan berisi**, tetapi belum pernah ditulis dokumennya.
Keduanya sesi aliran WiFi pertama pada 22 September malam, dan angkanya yang membuktikan
bahwa mengalirkan lewat WiFi menghapus kehilangan frame:

| Run | Mulai | Segmen | Frame |
|---|---|---|---|
| `gelora-002` | 22 Sep 2026 19:56 | 60 | 216.454 |
| `gelora-003` | 22 Sep 2026 20:14 | 51 | 200.389 |

Rekamannya tersimpan di `captures/`. Yang belum ada adalah dokumen analisisnya. Sebagian
angkanya sudah terpakai di [../00-umum/frame-loss.md](../00-umum/frame-loss.md), tetapi
tanpa dokumen sendiri kedua sesi itu tidak bisa ditelusuri. Dicatat sebagai hutang.

## Sinyal yang berlaku

| Sinyal | ID | Byte | Rumus | Status |
|---|---|---|---|---|
| Kecepatan halus | `0x18FFDC01` | b4-b5 LE | km/jam = nilai dibagi 256 | **terbukti** |
| Kecepatan bulat | `0x18FEDCD5` | b0 | km/jam apa adanya | **terbukti** |
| Odometer | `0x18FEDCD5` | b1-b2 LE | km apa adanya | **terbukti** |
| Putaran motor | `0x0CFF7902` | b4-b5 LE | rpm = nilai dikurangi 12000 | **terbukti** |
| Arus | `0x0CFF7E03` | b4-b5 LE | A = nilai dikurangi 1000, negatif saat regeneratif | **terbukti** |
| SOC | `0x0CFF7D03` | b1 | % = nilai dikali 0,5 | **terbukti** |
| Tegangan pack | `0x0CFF7E03` | b2-b3 LE | volt apa adanya | **terbukti** |
| SOH | `0x0CFF7E03` | b1 | % apa adanya | tetap, belum diuji |
| Suhu baterai maks | `0x0CFF7E03` | b6 | °C = nilai dikurangi 40 | **terbukti** |
| Suhu baterai min | `0x0CFF7E03` | b7 | °C = nilai dikurangi 40 | **terbukti** |
| Tegangan 90 sel | `0x0CFF8203` | multiplex | mV, b1 nomor sel awal, 3 x 16-bit LE | **terbukti** |
| Suhu 30 sensor | `0x0CFF8303` | multiplex | °C = nilai dikurangi 40 | **terbukti** |
| Suhu controller | `0x0CFF1601` | b2 | °C apa adanya | **gugur**, tidak bergerak dalam 47 menit |

Untuk konsumen data, pakai **kecepatan halus**, bukan yang bulat.

Seluruh nama di atas siap dipasang ke alat dalam satu perintah:

```
python tools/apply_notes.py --host <ip alat> docs/evidence/dfsk-gelora-e/notes.tsv
```

Berkasnya [notes.tsv](notes.tsv). Lima di antaranya bertanda `#tds`, jadi hanya
kelima itu yang muncul di panel Vehicle dan di penyaring TDS pada dashboard.
Lihat [../../DASHBOARD-TDS.md](../../DASHBOARD-TDS.md).

## Yang tidak ditemukan

Resistansi isolasi tidak ada di seluruh 34 identifier, pada skala mana pun yang dicoba.
Kemungkinan besar nilai itu diminta head unit lewat permintaan diagnostik, bukan
disiarkan berkala. Alat ini tidak pernah meminta, dan tidak akan pernah.

## Temuan tentang kendaraannya

Paket baterai **tidak seimbang**. Sel terendah berada sekitar 200 sampai 260 mV di bawah
mayoritas sel, dan selisihnya melebar antara 22 dan 23 September. Layar CarInfo kendaraan
hanya menampilkan enam sel pertama, semuanya sehat, jadi selisih ini tidak terlihat dari
dalam mobil. Ini perlu disampaikan ke pemilik kendaraan.

## Batas pemakaian

Angka di halaman ini milik **DFSK**. Armada memakai Wuling. Tidak satu pun identifier di
sini boleh masuk profil sinyal armada. Yang berpindah adalah metodenya, dan metode itu
ada di skill `bmt-can-signal-mapping`.
