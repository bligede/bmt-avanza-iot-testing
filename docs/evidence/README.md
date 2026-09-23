# docs/evidence/

Arsip bukti, **disusun per jenis kendaraan**. Satu folder per kendaraan, dan satu folder
untuk temuan yang tidak milik kendaraan mana pun.

```
docs/evidence/
├── 00-umum/                 temuan lintas kendaraan dan uji di meja
├── dfsk-gelora-e/           DFSK Gelora E, van listrik
└── honda-hrv-2023/          Honda HR-V 2023
```

## Peta kendaraan

| Folder | Kendaraan | Env PlatformIO | UNIT_ID | Bitrate | Bentuk ID | Status pemetaan |
|---|---|---|---|---|---|---|
| `dfsk-gelora-e/` | DFSK Gelora E (van listrik) | `gelora-e-250k` | `GELORAE-TEST-01` | 250 kbps | 29-bit extended | **terbukti** lewat uji jalan |
| `honda-hrv-2023/` | Honda HR-V 2023 | `hrv` | `HRV-TEST-01` | 500 kbps | 11-bit standard | kandidat, belum diuji jalan |
| *(belum ada)* | Wuling armada | belum dibuat | belum | belum diukur | belum diketahui | **belum pernah diuji** |

## Aturan penamaan

**Folder kendaraan:** `<merek>-<model>[-<tahun>]`, huruf kecil, dipisah tanda hubung.
Tahun disertakan kalau model itu berubah antar tahun model. Contoh: `honda-hrv-2023`,
`dfsk-gelora-e`.

**Nama run:** `<slug-pendek>-NNN`, urut naik per kendaraan, tidak pernah dipakai ulang.
Contoh: `hrv-001`, `gelora-001`, `gelora-004`. Nama run inilah yang dipakai sebagai
nama folder di `captures/` dan yang disebut di dalam prosa, jadi ia **tidak berubah**
walau berkasnya dipindah.

Nomor run melompat kalau sebuah sesi dibatalkan atau gagal. Lompatan itu wajar dan tidak
boleh ditutup dengan menomori ulang, karena nomor run muncul di header berkas rekaman
yang tidak pernah disunting.

**Satu run yang diulang membuat run baru**, bukan menimpa yang lama.

## Aturan isi

**BUKTI MENTAH BUKAN TAFSIRAN TEKNISI.**

Berkas rekaman dan log serial adalah sumber utama dan **tidak pernah diedit setelah
tes**. Berkas hasil analisis boleh direvisi kapan saja. Kalau keduanya bertentangan,
yang mentah yang benar. Ini D-015 di `bmt-can-bus-telemetry`.

1. **Seluruh sesi, bukan bagian yang menariknya saja.** Dari baris boot pertama sampai
   akhir. Alasan reset, urutan init, peringatan, heap, dan waktu startup justru itu yang
   menjawab "apakah perangkat kita pernah sehat?" saat tes berikutnya bermasalah.
2. **Apa adanya, bukan ringkasan.** Log yang sudah diformat ulang tidak bisa diaudit.
3. **Catat kegagalan.** Jangan menghapus sesi gagal lalu menyimpan yang "PASS setelah
   diperbaiki". Itu membuang riwayat diagnostiknya.
4. **Nama dan tafsiran tinggal di luar berkas rekaman**, yaitu di `/notes/<UNIT_ID>/`
   pada alat. Tebakan yang salah bisa dikoreksi tanpa menyentuh rekaman.

## Status sebuah pemetaan

Empat tingkat, dan tingkatnya harus ditulis di setiap tabel sinyal:

| Status | Artinya |
|---|---|
| **dugaan** | angkanya masuk akal, belum pernah diadu dengan apa pun |
| **kandidat** | cocok dengan layar kendaraan, tetapi hanya saat kendaraan diam |
| **terbukti** | nilainya **bergerak** mengikuti kendaraan dan lolos salah satu cara pembuktian |
| **gugur** | pernah naik, lalu dibatalkan oleh bukti baru. Ditulis sejelas kenaikannya |

Cara menaikkan status ada di skill `bmt-can-signal-mapping` dan contoh lengkapnya di
`dfsk-gelora-e/gelora-004.md`.

## Isi tiap folder kendaraan

Setiap folder kendaraan punya `README.md` yang memuat ringkasan kendaraan itu, daftar
sesi, dan tabel sinyal terakhir yang berlaku. Mulailah dari situ, bukan dari berkas run.
