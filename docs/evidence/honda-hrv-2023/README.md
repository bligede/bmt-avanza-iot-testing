# Honda HR-V 2023

Mobil penumpang bermesin bensin. Kendaraan uji pertama, dipakai untuk membuktikan bahwa
alat ini bisa membaca bus kendaraan sungguhan.

| | |
|---|---|
| Env PlatformIO | `hrv` |
| UNIT_ID | `HRV-TEST-01` |
| Bitrate | **500 kbps**, terukur |
| Bentuk identifier | 11-bit standard |
| Kepadatan bus | sekitar 1.000 frame per detik, jauh lebih padat daripada Gelora E |

## Sesi

| Run | Tanggal | Kondisi | Isi | Berkas |
|---|---|---|---|---|
| `hrv-001` | 8 Sep 2026 | kendaraan hidup | 194.219 frame, 40 identifier | [hrv-001.md](hrv-001.md) |

Kandidat sinyalnya terpisah di [hrv-001-candidates.md](hrv-001-candidates.md), karena
kandidat adalah tafsiran dan harus bisa direvisi tanpa menyentuh dokumen asal-usulnya.

Rekamannya ditarik 20 September 2026 dan keutuhannya diperiksa: 34 berkas, seluruh 34
sidik jari SHA-256 cocok, dan dua kali unduh ulang menghasilkan berkas yang identik.

## Status pemetaan

**Kandidat, belum satu pun terbukti.** Seluruh nilai diambil tanpa sesi sambil berjalan,
jadi statusnya sama persis dengan status Gelora E sebelum 23 September: cocok di atas
kertas, belum pernah diadu dengan nilai yang bergerak.

Untuk menaikkannya, kendaraan ini butuh satu kali uji jalan mengikuti
`docs/PROSEDUR-TEST-JALAN.md`.

## Yang perlu diperhatikan kalau sesi berikutnya jadi

Bus HR-V sekitar 1.000 frame per detik, jadi kehilangan frame saat merekam ke flash
internal mencapai **22 sampai 26 persen**, jauh lebih buruk daripada 14,8 % pada Gelora
E. Sesi berikutnya **wajib** memakai aliran WiFi dengan perekaman flash dijeda. Lihat
[../00-umum/frame-loss.md](../00-umum/frame-loss.md).

## Batas pemakaian

HR-V adalah mobil bensin. **Tidak ada state of charge**, dan tidak ada satu pun besaran
baterai traksi. Armada memakai kendaraan listrik Wuling, jadi identifier di sini tidak
punya hubungan apa pun dengan armada. Yang dibuktikan kendaraan ini adalah **jalur
bacanya**, bukan peta sinyalnya.
