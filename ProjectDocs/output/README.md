# output/

Dokumen jadi untuk dibaca di luar tim teknis. Belum ada satu pun.

Folder per tipe lahir saat dokumen pertama dari tipe itu dibuat, bukan sebelumnya.
Penamaan berkas mengikuti `bmt-versioned-output`:
`<Slug>_v{MAJOR}.{MINOR}_{YYYY-MM-DD}.{ext}`.

## Daftar dokumen

| Status | Dokumen | Versi | Tanggal |
|---|---|---|---|
| | *belum ada* | | |

## Kenapa masih kosong

Seluruh hasil project ini sejauh ini berbentuk **dokumen teknis** yang tinggal di
`docs/`, dan pembacanya adalah engineer atau agent, bukan pihak luar. Selama itu masih
benar, folder ini memang kosong.

Yang akan mengubahnya: laporan temuan ketidakseimbangan baterai untuk pemilik kendaraan
uji, atau ringkasan hasil pemetaan untuk operator BMT dalam bentuk dokumen berlogo.
Keduanya disusun lewat `bmt-formal-docs`, dan build script-nya menulis ke
`output/<tipe>/`, tidak pernah rata di folder ini.
