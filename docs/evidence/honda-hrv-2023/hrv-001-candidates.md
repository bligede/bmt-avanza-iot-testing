# hrv-001: kandidat sinyal dari pencocokan angka panel

Hasil mencocokkan angka yang terbaca di panel instrumen HR-V (foto 8 Sep 2026,
16:04) dengan isi rekaman `hrv-001`. Metodenya Blueprint §9.1: cari nilai yang
sudah diketahui di dalam frame, lalu validasi dengan menggerakkan kendaraan.

Alat: `tools/match_dashboard.py`. Ia menguji setiap posisi byte, panjang 1 sampai
4 byte, big dan little endian, serta field 12 bit, pada seluruh 40 identifier.

## Kandidat

| Angka di panel | ID | Field | Rumus | Cakupan | Rentang di rekaman |
|---|---|---|---|---|---|
| Odometer **069620 km** | `0x294` | byte 3–5, big-endian | nilai = mentah | **3.847 dari 3.847 frame (100%)** | 69.620 tetap |
| Suhu luar **29 °C** | `0x324` | byte 0 | °C = mentah − 40 | **1.475 dari 1.481 frame (99,6%)** | 68–69, yaitu 28–29 °C |
| Trip A **1459,6 km** | (tidak ada) | (tidak ada) | (tidak ada) | **tidak ditemukan** | (tidak ada) |

Kandidat lain untuk suhu (`0x396`, `0x374`, `0x1A6`) dibuang: rentang nilainya
sampai 4.608, jadi kecocokannya kebetulan, bukan besaran suhu.

Kedua kandidat sudah ditulis sebagai catatan di alat, lewat `POST /api/note`,
sehingga tampil di kolom catatan pada dashboard dan tercatat di
`/notes/journal.log`.

## Mengapa trip A tidak ada

Tiga kemungkinan, berurutan dari yang paling mungkin:

1. Trip meter dihitung dan disimpan di dalam kluster instrumen sendiri, tidak
   pernah disiarkan ke bus.
2. Nilainya disiarkan di bus bodi yang tidak dirutekan ke pin 6/14 OBD-II.
3. Skalanya bukan yang dicoba (1, 0,1, 0,01, 0,001 dan pembulatan ke 1459).

Ketiadaan hasil di sini adalah temuan, bukan kegagalan alat: odometer dan suhu
ditemukan dengan metode yang sama pada rekaman yang sama.

## Yang masih kurang

Keduanya **hipotesis**, karena nilainya **tidak bergerak** selama 3 menit
rekaman. Odometer yang tetap 69.620 cocok dengan panel, tetapi field mana pun
yang kebetulan berisi 69.620 dan diam akan terlihat sama.

Cara membuktikannya, di run berikutnya:

| Kandidat | Uji | Lulus kalau |
|---|---|---|
| `0x294` odometer | jalan tepat 2 km, rekam terus | nilai naik tepat 2 |
| `0x324` suhu | rekam saat pagi dingin dan siang panas | selisihnya mengikuti panel |

Sampai itu terjadi, keduanya tidak boleh masuk `signals.cfg` armada. Dan walau
nanti terbukti, ini tetap temuan tentang **Honda**: armada memakai kendaraan
lain, dan ID tidak pernah dipindah antar jenis kendaraan.

## Perintah untuk mengulang

```
python tools/match_dashboard.py captures/device-2026-09-20 --stop-ms 229600 \
    --value 69620 --scales 1 0.1 0.01 --label "odometer km"

python tools/match_dashboard.py captures/device-2026-09-20 --stop-ms 229600 \
    --value 29 --scales 1 0.5 --offsets 0 40 50 --min-coverage 0.5 --label "suhu C"
```

`--stop-ms 229600` memotong gangguan saat konektor dilepas. Alasannya di
[hrv-001.md](hrv-001.md).
