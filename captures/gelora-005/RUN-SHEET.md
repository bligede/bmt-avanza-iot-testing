# Lembar run gelora-005: rekaman flash 22 Sep yang baru ditemukan

**Bukan sesi baru.** Ini isi flash alat yang **belum pernah ditarik**, ditemukan
23 Sep 2026 saat hendak mem-flash firmware baru, dan ditarik sebelum apa pun
disentuh.

| | |
|---|---|
| Kendaraan | DFSK Gelora E |
| Unit | `GELORAE-TEST-01`, bitrate 250 kbps, listen-only |
| Isi rekaman | **22 September 2026 malam**, mulai sekitar 20:35 |
| Ditarik | 23 September 2026, lewat HTTP dari `10.215.81.4` |
| Berkas | `can-000.log` sampai `can-063.log`, 64 berkas |
| Isi | **213.979 frame**, 34 identifier |
| Keutuhan | seluruh 64 berkas cocok ukuran dengan yang dilaporkan alat dan terbaca |

Nomor run 005 dipakai walaupun isinya lebih tua daripada `gelora-004`. Nomor run
adalah **identitas, bukan urutan waktu**, dan nomor tidak pernah dipakai ulang.

## Kenapa ini nyaris hilang

Saat alat dinyalakan 23 Sep, log boot berbunyi:

```
Capture budget 0 B (487424 B free on flash, ceiling 12582912 B)
Flash is nearly full: pull the captures and run clearcaptures
```

Jalan pintas yang wajar adalah menjalankan `clearcaptures` supaya alat bisa
merekam lagi. Kalau itu dilakukan, 13,2 MB ini hilang tanpa pernah ada yang tahu
isinya apa.

Yang menahannya: membandingkan daftar berkas di alat dengan salinan di laptop
lebih dulu. Hasilnya, **hampir setiap berkas berbeda ukuran** dari salinan
`gelora-001`, dan 11 berkas belum ada di laptop sama sekali. Jadi isi flash
bukan salinan `gelora-001` seperti yang diduga.

## Hubungannya dengan rekaman lain malam itu

Malam 22 September menghasilkan tiga rekaman yang tumpang tindih, dan ketiganya
perlu dibaca bersama, bukan sendiri-sendiri:

| Run | Cara merekam | Mulai | Frame |
|---|---|---|---|
| `gelora-002` | dialirkan ke laptop | 19:56 | 216.454 |
| `gelora-003` | dialirkan ke laptop | 20:14 | 200.389 |
| `gelora-005` | **ditulis ke flash alat** | sekitar 20:35 | 213.979 |

Alat sempat **reboot di tengah** `gelora-005`, terlihat dari penghitung millis
yang kembali ke nol di tengah berkas. Segmen tetap berlanjut karena perekam
memang dirancang menyambung setelah nomor tertinggi, bukan menimpa.

## Nilainya

Ketiga rekaman itu adalah bahan mentah yang menghasilkan angka di
`docs/evidence/00-umum/frame-loss.md`, yaitu 14,8 % hilang saat menulis ke flash
melawan 0,0 % saat dialirkan. `gelora-005` adalah sisi flash dari perbandingan
itu, dan sampai sekarang belum ada dokumen bukti yang menelusurinya.

Dicatat sebagai hutang B-07 di `ProjectDocs/BACKLOG.md`.

## Yang belum dilakukan

- [ ] Dokumen bukti untuk `gelora-002`, `gelora-003`, dan `gelora-005`.
- [ ] Salin ketiganya ke penyimpanan di luar laptop ini.
- [ ] Baru setelah itu, flash alat boleh dikosongkan.
