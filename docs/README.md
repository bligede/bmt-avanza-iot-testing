# docs/

Dokumentasi teknis alat diagnostik CAN. Pembacanya engineer atau agent.

Lapisan serah-terima project ada di `ProjectDocs/`, dan ia **merutekan** ke sini, bukan
menyalin isinya. Kalau keduanya berselisih, yang ada di `docs/` yang benar, karena di
sinilah isinya tinggal.

## Kalau kamu akan menyentuh kendaraan

Baca dua ini sampai habis, berurutan:

| Berkas | Isi |
|---|---|
| [RUN-PROCEDURE.md](RUN-PROCEDURE.md) | prosedur perekaman apa pun, termasuk yang diam |
| [PROSEDUR-TEST-JALAN.md](PROSEDUR-TEST-JALAN.md) | prosedur perekaman sambil berjalan, dengan laptop ikut di mobil |

Lalu [evidence/00-umum/frame-loss.md](evidence/00-umum/frame-loss.md), supaya kamu tahu
sejauh mana rekaman yang kamu hasilkan boleh dipercaya.

## Kalau kamu memakai dashboard

[DASHBOARD-TDS.md](DASHBOARD-TDS.md) menjelaskan tiga mode penyaring identifier,
panel Vehicle, dan cara menandai sebuah sinyal dengan `#tds` supaya ia muncul di
situ. Termasuk satu hal yang mudah disalahpahami: penyaring itu **tidak**
memperbaiki frame yang hilang, dan **tidak** menyaring perekaman.

## Kalau kamu akan memetakan sinyal

Prosesnya ada di skill **`bmt-can-signal-mapping`**, dan contoh lengkap yang sudah jadi
ada di [evidence/dfsk-gelora-e/gelora-004.md](evidence/dfsk-gelora-e/gelora-004.md).

## Arsip bukti, per jenis kendaraan

| Kendaraan | Folder | Status pemetaan |
|---|---|---|
| DFSK Gelora E | [evidence/dfsk-gelora-e/](evidence/dfsk-gelora-e/) | **terbukti** lewat uji jalan |
| Honda HR-V 2023 | [evidence/honda-hrv-2023/](evidence/honda-hrv-2023/) | kandidat, belum diuji jalan |
| Lintas kendaraan dan uji meja | [evidence/00-umum/](evidence/00-umum/) | |

Aturan penamaan folder, penamaan run, dan tingkat status sebuah pemetaan ada di
[evidence/README.md](evidence/README.md). **Mulailah dari README folder kendaraannya**,
bukan dari berkas run, karena README itu yang memuat tabel sinyal yang berlaku sekarang.

## Rujukan lain

| Berkas | Isi |
|---|---|
| [CAN-REFERENCES.md](CAN-REFERENCES.md) | rujukan luar tentang CAN dan reverse engineering |
| [PHASE-2-DASHBOARD.md](PHASE-2-DASHBOARD.md) | dashboard fase 2 dan jawaban atas masukan CEO |
