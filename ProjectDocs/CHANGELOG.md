# CHANGELOG: Alat Diagnostik CAN BMT

Riwayat versi dokumen dan temuan. Terbaru di atas. Riwayat perubahan **kode** ada di
`git log`, dan tidak diduplikasi di sini.

## 2026-09-23: Dashboard menyaring tampilan, dan panel nilai kendaraan

- Tiga mode penyaring identifier di dashboard: TDS, Named, All. Menyaring **apa
  yang ditampilkan**, tidak pernah apa yang diterima atau direkam (D-006).
- Panel Vehicle: nilai kendaraan sebagai angka besar, dibangun dari catatan
  bertanda `#tds` yang memuat rumus. Tidak ada identifier yang ditulis di dalam
  kode halaman.
- Penanda `#tds` adalah awalan di teks catatan, jadi tanpa penyimpanan baru dan
  tanpa perubahan format berkas.
- `tools/apply_notes.py` memasang satu berkas catatan per kendaraan sekali
  jalan, menolak berkas yang ada catatannya melewati batas, dan mencetak echo
  dari alat bukan teks yang dikirim.
- `docs/evidence/dfsk-gelora-e/notes.tsv`: delapan nama siap pasang, lima
  bertanda `#tds`.
- Diluruskan di dokumen: penyaring ini **bukan** perbaikan untuk frame yang
  hilang. Penyebabnya interupsi CAN di flash, dan itu tidak tersentuh.
- D-007 mencatat arah berpindah ke Orange Pi 5, berstatus PROVISIONAL, dengan
  empat hal yang belum dijawab: daya dan kontak, waktu siap, suhu kabin, dan
  bagaimana jaminan listen-only ditegakkan di SocketCAN.

## 2026-09-23: Skeleton ProjectDocs dibuat

- `ProjectDocs/` lahir lewat `bmt-project-onboarding`, sepuluh berkas.
- Isinya **merutekan** ke `docs/` yang sudah ada, bukan menyalinnya. `docs/` tidak
  dipindah, sehingga rujukan dari `project-mdt-tds` dan dari commit lama tetap hidup.
- Lima keputusan repo ini dicatat untuk pertama kalinya, D-001 sampai D-005. Sebelumnya
  repo ini merujuk 24 kali ke `D-XXX` tanpa punya decision log sendiri.
- Tabel keputusan yang **diwarisi** dari `bmt-can-bus-telemetry` dan `project-mdt-tds`
  dibuat, karena `D-006` sudah berarti dua hal berbeda di dua repositori.
- D-005: project ini tanpa sprint, laporan disusun per milestone.

## 2026-09-23: Uji jalan DFSK Gelora E, pemetaan naik status jadi terbukti

- Uji jalan 47 menit, 1.525.681 frame dialirkan lewat WiFi.
- Terbukti: kecepatan, kecepatan halus 1/256 km/jam, odometer, SOC, arus, tegangan pack,
  suhu baterai, dan putaran motor.
- Arus terbukti lewat arah, yaitu +133 A lalu -36 A dalam satu detik saat pedal dilepas.
- Suhu controller turun kembali jadi dugaan, tidak bergerak sedetik pun.
- Dicatat apa adanya: tautan WiFi putus 30 kali, 14,9 % waktu sesi tidak terekam.
- `docs/evidence/dfsk-gelora-e/gelora-004.md`.

## 2026-09-22: DFSK Gelora E, kandidat sinyal pertama dari kendaraan listrik

- Bus terbaca di 250 kbps, seluruhnya identifier 29-bit extended bergaya J1939.
- Dua belas nilai dicocokkan ke layar kendaraan, semuanya masih statis.
- Temuan tentang kendaraan: paket baterai tidak seimbang, sel terendah sekitar 200 mV di
  bawah mayoritas, dan itu tidak terlihat dari layar mobil.
- Temuan tentang alat: interupsi CAN berada di flash, sehingga merekam ke flash sendiri
  membuang 14,8 % frame. Mengalirkan lewat WiFi menghapusnya.
- `docs/evidence/dfsk-gelora-e/gelora-001.md`, `docs/evidence/00-umum/frame-loss.md`,
  `docs/evidence/dfsk-gelora-e/TESTING-2026-09-22.md`.

## 2026-09-20: Rancangan papan sirkuit rev A

- Skematik, netlist, dan BOM dibangkitkan dari satu berkas sumber tunggal.
- Pemeriksa terpisah membaca hasilnya kembali dan menguji 12 aturan keselamatan, diuji
  dengan tiga cacat yang sengaja disuntikkan.
- `hardware/README.md` §10 merekomendasikan slot microSD untuk rev B.

## 2026-09-08: Honda HR-V 2023, pengujian kendaraan pertama

- Alat terbukti bisa membaca bus kendaraan sungguhan di 500 kbps.
- Listen-only bertahan di kendaraan hidup.
- Kandidat sinyal dicatat, belum pernah diuji sambil berjalan.
- `docs/evidence/honda-hrv-2023/hrv-001.md`, `hrv-001-candidates.md`.

## 2026-09-06: Repo dimulai

- Firmware bring-up ESP32-S3 dengan SN65HVD230, dipisah dari firmware armada yang
  dibekukan.
- Penegakan listen-only tiga lapis sejak commit pertama.
