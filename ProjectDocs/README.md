# Alat Diagnostik CAN BMT

Project internal PT Bali Mikro Teknologi. Dimulai 6 September 2026.

## Apa ini

Sebuah alat uji yang dipasang sebentar ke kendaraan lewat konektor OBD-II, lalu
**membaca** lalu lintas data komputer kendaraan tanpa pernah ikut bicara di jalur itu.
Gunanya satu: mencari tahu angka mana di dalam data kendaraan yang berarti kecepatan,
sisa baterai, jarak tempuh, dan seterusnya, supaya angka-angka itu nanti bisa dipakai
oleh sistem armada.

Alat ini **bukan** produk yang akan dipasang permanen di armada. Perangkat armada
dibangun di repositori terpisah. Yang dihasilkan project ini adalah **pengetahuan**:
peta angka per jenis kendaraan, cara membuktikannya, dan prosedur pengujiannya.

## Satu aturan yang tidak pernah dilanggar

Alat ini **hanya mendengarkan**. Ia tidak pernah mengirim apa pun ke kendaraan. Itu
ditegakkan di tiga tempat yang saling bebas, dan salah satunya menggagalkan proses build
sehingga firmware yang melanggar tidak akan pernah jadi. Konsekuensinya: alat ini tidak
bisa merusak, mengubah, atau mengganggu kendaraan yang diujinya.

## Peta folder

| Folder | Isi |
|---|---|
| `ProjectDocs/` | dokumentasi project, folder ini |
| `ProjectDocs/agent-documentation/` | dokumen serah-terima bernomor 00 sampai 09 |
| `ProjectDocs/output/` | dokumen jadi untuk dibaca di luar tim teknis |
| `docs/` | laporan pengujian, bukti, dan prosedur, dalam bahasa teknis |
| `docs/evidence/` | bukti mentah tiap pengujian, tidak pernah diedit setelah tes |
| `src/` | kode firmware alat |
| `web/` | halaman dashboard yang tampil saat alat menyala |
| `tools/` | program bantu di laptop untuk menarik dan mengolah rekaman |
| `hardware/` | rancangan papan sirkuit |
| `captures/` | rekaman mentah dari kendaraan, tidak ikut ke repositori karena besar |

## Hasil sejauh ini

| Kendaraan | Kapan | Hasil |
|---|---|---|
| Honda HR-V 2023 | 8 Sep 2026 | alat terbukti bisa membaca bus kendaraan sungguhan |
| DFSK Gelora E | 22 Sep 2026 | 12 nilai dipetakan saat kendaraan diam |
| DFSK Gelora E | 23 Sep 2026 | uji jalan 47 menit, pemetaan naik status jadi **terbukti** |

Laporan lengkapnya ada di `docs/`, dan yang paling baru di
`docs/evidence/gelora-004.md`.

## Kontak

PT Bali Mikro Teknologi, `balimicrotechnology@gmail.com`.
Repositori: `github.com/bligede/bmt-avanza-iot-testing`, privat.
