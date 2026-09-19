# PCB BMT CAN logger, rev A

Satu papan untuk dua peran, dirakit dan disolder sendiri:

| Peran | Yang dipasang |
|---|---|
| **Perangkat uji** | semua, **kecuali** bagian bertanda `FLEET` |
| **Unit armada** | semua, termasuk bagian modem A7670C |

**Status: rancangan, belum pernah diproduksi.** Rangkaian sudah diperiksa secara otomatis
(lihat §7), tetapi berkasnya belum pernah dibuka di KiCad maupun EasyEDA. Keduanya belum
terpasang di mesin tempat berkas ini dibuat. Sebelum memesan PCB, kerjakan dulu daftar
di §8.

---

## 1. Berkas

| Berkas | Isi |
|---|---|
| `gen_schematic.py` | **sumber kebenaran**: seluruh rangkaian sebagai data. Ubah di sini, bukan di berkas hasil |
| `bmt-can-logger.kicad_sch` | schematic KiCad 7 hasil generator, untuk diimpor ke EasyEDA |
| `bmt-can-logger.kicad_pro` | berkas proyek minimal supaya KiCad membukanya sebagai proyek |
| `netlist.csv` | 37 net: nama net → `REF.pin`. Cadangan kalau impor gagal |
| `bom.csv` | daftar belanja: kolom `fit` = `ALL` atau `FLEET` |
| `schematic-preview.svg` | gambar schematic, cukup dibuka di browser |
| `verify_schematic.py` | membaca ulang `.kicad_sch` dan membuktikan isinya |
| `render_preview.py` | membuat `schematic-preview.svg` |

Setelah mengubah rangkaian:

```
python gen_schematic.py && python verify_schematic.py && python render_preview.py
```

---

## 2. Blok rangkaian

```
 12 V ACC ─ J1 ─ F1 PTC ─┬─ D1 1N5822 ─ VIN_P ─┬─ U1 LM2576HV-5.0 ─ +5V ─┬─ D3 ─ DevKit 5V
 (fuse tap, 3 A)         │                     │                         ├─ U3 VCC (CAN)
                         D2 1.5KE27A           │                         ├─ GPS
                         (surge / polaritas)   │                         └─ kipas (JP3)
                                               └─ U2 LM2576HV-ADJ ─ +4V0 ─ JP1 ─ modem   [FLEET]

 OBD-II 6/14/5 ─ J2 ─ D4 ESD ─ U3 TJA1051T/3 (S = HIGH → Silent) ─ RXD ─ GPIO4
                         └ R5 120 R ─ JP2 (bench saja)

 DevKit 3V3 ─ U3 VIO, DHT22, pull-up        GPIO18 ← GPS    GPIO15 ↔ DHT22
 GPIO13 → Q1 kipas    GPIO21/47 → LED     GPIO0 ← tombol
 GPIO16/17 ↔ Q3/Q4 (1,8 V ↔ 3,3 V) ↔ modem    GPIO14 → Q2 → PWRKEY     [FLEET]
```

Semua GPIO sama dengan firmware yang sudah jalan. **Firmware uji tidak perlu diubah.**

---

## 3. Keputusan rancangan dan alasannya

| Keputusan | Alasan |
|---|---|
| **TJA1051T/3, pin S diikat ke 3V3** (bukan SN65HVD230) | Pada mode Silent, pemancar transceiver **mati di dalam chip** (datasheet NXP §7.1.2). Tidak ada jumper, tidak ada GPIO, tidak ada bug firmware yang bisa menyalakannya. Ini lapis keempat jaminan listen-only, dan satu-satunya yang berupa hardware. SN65HVD230 tidak punya mode diam |
| **J2 hanya punya 3 kutub: CANH, CANL, GND** | OBD-II pin 16 tidak boleh disambung (fleet `01-PINOUT`, Blueprint §5.2). Konektornya tidak punya tempat untuk pin 16, jadi tidak ada yang bisa salah sambung |
| **Catu dari fuse tap ACC** lewat J1, sekring blade 3 A di kabel | Sumber terpisah dengan sekring dan proteksi sendiri, sesuai aturan pin 16. ACC mati saat kunci dicabut, jadi alat tidak menguras aki saat parkir (F-15) |
| F1 PTC 1,6 A / 72 V + D2 TVS 1.5KE27A + D1 Schottky | Lonjakan tegangan dijepit maksimum 37,5 V. Kalau polaritas terbalik, D2 menghantar sehingga F1 memutus, dan D1 memblokir |
| **LM2576HV** (TO-220, input sampai 60 V) | Kaki besar, mudah disolder, dan tahan di atas batas jepit TVS 37,5 V. Modul buck murah (MP1584 maks 28 V) tidak tahan |
| **D3** antara +5V dan pin 5V DevKit | Espressif menyebut catu USB dan catu pin 5V *saling eksklusif*. D3 mencegah 5 V dari USB laptop mengalir balik ke papan saat keduanya tersambung |
| Modem lewat **Q3/Q4 BSS138** | UART A7670 bekerja di **1,8 V** (manual hardware SIMCom). Menyambungnya langsung ke GPIO 3,3 V akan merusaknya |
| **C6 2200 µF** tepat di J6 | Modem menarik hingga 2 A saat memancar. Tanpa kapasitor ini modem me-reset sendiri (fleet `01-PINOUT`) |
| **JP1** memilih catu modem 4,0 V atau 12 V | Modul A7670C polos butuh VBAT 3,4–4,2 V. Papan breakout umumnya punya regulator sendiri dan butuh 5–12 V. Belum pasti papan mana yang dibeli |
| Semua komponen THT, kecuali U3 (SOIC-8), D4, Q3, Q4 (SOT-23) | Disolder tangan. SOIC-8 dan SOT-23 masih bisa disolder dengan solder biasa. Tidak ada versi THT dari TJA1051 maupun PESD2CAN |
| IRLZ44N dan R 330 Ω untuk kipas | Sama dengan prototipe yang sudah terbukti jalan |

---

## 4. Membawanya ke EasyEDA

Pakai **EasyEDA Pro** (pro.easyeda.com), login dengan akun Google Anda.

1. File → Import → **KiCad**. Pilih `bmt-can-logger.kicad_sch`, atau zip seluruh folder
   `hardware/` lalu impor zip-nya.
2. Periksa hasilnya terhadap `schematic-preview.svg`: jumlah komponen **61**, jumlah net
   **37**.
3. **Footprint.** Nama footprint di schematic memakai nama pustaka KiCad. EasyEDA bisa saja
   tidak mengenalinya. Kalau kosong, pasang footprint per komponen dari kolom `footprint`
   di `bom.csv`. Cara paling cepat: cari part-nya di pustaka LCSC EasyEDA dengan nama MPN,
   misalnya `TJA1051T/3`, `LM2576HVT-5.0`, `1N5822`.
4. Design → Update/Convert to PCB.

**Kalau impor gagal:** gambar ulang schematic di EasyEDA dari `netlist.csv`. Isinya 37
baris, masing-masing satu net dengan semua pin yang tersambung. Kemungkinan besar
pekerjaannya satu sampai dua jam.

Proses impor ini **belum pernah dicoba**. Kalau ada pesan error, kirimkan pesannya ke saya.

---

## 5. Aturan tata letak PCB

**Papan:** 2 lapis, 1,6 mm, tembaga 1 oz (2 oz lebih baik untuk jalur modem). Sekitar
100 × 80 mm. Empat lubang M3 di sudut, 4 mm dari tepi.

| Bagian | Aturan |
|---|---|
| **Soket DevKit J10/J11** | jarak antar-baris **22,86 mm (900 mil)** dari pusat ke pusat, pitch 2,54. Angka ini hasil hitungan dari gambar dimensi Espressif, jadi **ukur dengan jangka sorong di DevKit Anda sebelum memesan** |
| **Antena ESP32** | ujung antena DevKit (sisi yang berlawanan dengan USB) harus menjorok keluar tepi PCB, atau area di bawahnya bebas tembaga di kedua lapis |
| **Buck U1/U2** | kapasitor input, U, dioda catch, dan L dirapatkan. Loop dioda catch dibuat sekecil mungkin. Letakkan jauh dari GPS dan antena |
| **Lebar jalur** | 12 V input dan jalur modem (+4V0, MODEM_PWR, GND-nya) ≥ 1,5 mm. +5V ≥ 1,0 mm. Sinyal 0,3 mm |
| **CAN** | CANH/CANL dirutekan berpasangan dan pendek: J2 → D4 → U3. D4 di dekat J2 |
| **U3** | C8 di pin 3 (VCC), C9 di pin 5 (VIO), masing-masing kurang dari 5 mm |
| **Modem** | C6 menempel pada J6. Jalur GND modem langsung ke pour GND, bukan lewat jalur tipis |
| **GND** | pour di kedua lapis, dijahit via setiap ±10 mm |
| **Silkscreen** | `JP2: OPEN DI MOBIL` · `J2: 6=CANH 14=CANL 5=GND, TANPA PIN 16` · polaritas J1 `+12V ACC / GND` · `JP5: OPEN` · penanda `FLEET` di komponen modem |

---

## 6. Menyolder dan menguji, tanpa mobil dulu

**Catatan penting:** transceiver CAN mendapat 5 V dari buck, bukan dari USB. **USB saja
tidak cukup.** Di meja kerja pun papan butuh 12 V, dari power supply bench.

Urutan menyolder: yang paling pendek dulu. Resistor dan dioda, lalu U3, D4, Q3, Q4 (SMD),
lalu soket, kapasitor, konektor, U1/U2, dan terakhir kapasitor elektrolit besar.

Pengujian bertahap. Pakai power supply bench **12 V dengan batas arus 300 mA**, dan
**DevKit belum dipasang**:

| # | Uji | Lulus kalau |
|---|---|---|
| 1 | arus diam tanpa DevKit | < 30 mA |
| 2 | TP1 (VIN_P) | ± 11,5 V (12 V dikurangi drop D1) |
| 3 | TP3 (+5V) | 4,8–5,2 V |
| 4 | tanpa DevKit, ukur J4 pin 1 | 0 V. 3V3 berasal dari DevKit, jadi ini normal |
| 5 | pasang DevKit, nyalakan | J4 pin 1 = 3,3 V. Firmware boot, dashboard muncul |
| 6 | colok USB laptop bersamaan | tetap stabil. TP3 tidak naik |
| 7 | bench CAN dua node, **JP2 tertutup** | frame diterima seperti di prototipe |
| 8 | bench CAN, node pengirim hanya punya papan ini sebagai lawan | node pengirim melaporkan **ACK error**. Itu **bukti** pemancar U3 memang mati: papan ini tidak bisa mengakui frame |
| 9 | **[FLEET]** JP1 di posisi 1-2, **modem belum dipasang** | TP4 = 3,85–4,05 V. **Jangan lanjut kalau di atas 4,2 V** |
| 10 | **[FLEET]** pasang modem, JP5 OPEN | modem menyala lewat PWRKEY (GPIO14) atau tombol di papannya |

Baru setelah semuanya lulus: ke mobil. Ikuti `docs/RUN-PROCEDURE.md`. Ukur hambatan
OBD 6–14 dengan kunci kontak mati, dan **buka JP2**.

---

## 7. Apa yang sudah dibuktikan, dan dengan cara apa

`verify_schematic.py` tidak memakai generator sama sekali. Ia mem-parse `.kicad_sch`
sendiri, membangun ulang semua sambungan dari koordinat (ujung pin, kawat, label,
tanda no-connect), lalu:

- mencocokkan hasilnya dengan `netlist.csv`: **187 pin, 160 tersambung, 27 sengaja
  tidak dipakai**;
- gagal kalau ada pin yang tidak tersambung sekaligus tidak ditandai, atau net yang
  hanya punya satu sambungan;
- memeriksa 12 aturan keselamatan satu per satu, antara lain: U3 pin S di 3V3; J2 tidak
  menyentuh jalur catu apa pun; GPIO19/20 (USB) kosong; modem hanya dicatu lewat JP1.

Pemeriksa itu sendiri sudah diuji dengan tiga kesalahan yang disengaja: label yang salah,
tanda no-connect yang hilang, dan kawat yang bergeser 1,27 mm. **Ketiganya tertangkap.**

Fakta datasheet yang dipakai sudah dicocokkan dengan sumber pabrikan: pinout DevKitC-1
dan jarak barisnya (Espressif), TJA1051 (NXP), LM2576HV (TI), MF-RX160/72 (Bourns),
1.5KE27A (Littelfuse), PESD2CAN (Nexperia), dan A7670 (manual hardware SIMCom, dari salinan
Waveshare).

**Yang belum dibuktikan:** berkas ini belum pernah dibuka di KiCad atau EasyEDA, belum ada
ERC dari tool EDA, dan belum ada PCB yang dibuat.

---

## 8. Wajib dicek sebelum memesan PCB

1. **Papan modem yang akan dibeli.** Cocokkan urutan pin J6 dengan papan itu, lalu
   tentukan posisi JP1 (4 V untuk VBAT modul polos, atau 12 V untuk breakout yang punya
   regulator) dan JP4 (tutup hanya kalau UART papannya 3,3 V dan tidak punya pin VREF).
   Ubah `J6` di `gen_schematic.py` bila perlu, lalu jalankan ulang generator.
2. **Jarak baris soket DevKit**: ukur DevKit Anda. Nilai 22,86 mm adalah hasil hitungan.
3. **Urutan kaki LED dua warna** yang dibeli. Rancangan ini menganggap R–K–G, dengan
   katoda di tengah.
4. **Tegangan kipas**: posisi JP3 menentukan 5 V atau 12 V.
5. **Induktor L1/L2**: 100 µH dengan arus saturasi ≥ 3 A. Ukur diameter dan jarak kakinya,
   lalu pilih footprint yang cocok. Kolom footprint di BOM sengaja dikosongkan untuk
   komponen ini.
6. **JP5 (AUTO-ON) tetap OPEN** sampai terbukti di modul yang sebenarnya. Manual A7670
   tidak menjamin PWRKEY boleh diikat ke GND terus-menerus.

---

## 9. Dampak ke firmware dan repo armada

- **Firmware uji:** tidak ada perubahan. Semua pin sama.
- **Firmware armada (dibekukan, D-006):** tidak wajib diubah. `MODEM_PWRKEY_PIN` bisa
  menjadi `14` kelak, kalau PWRKEY dikendalikan lewat Q2. Dokumen armada `01-PINOUT`
  masih menyebut SN65HVD230. Transceiver di papan ini berbeda, tetapi driver TWAI tidak
  terpengaruh. Catat perbedaan ini saat freeze dibuka.
- **Batas yang tidak ikut berubah:** papan ini tetap listen-only. Mode Silent di U3
  melengkapi mode `TWAI_MODE_LISTEN_ONLY` di firmware, bukan menggantikannya.
