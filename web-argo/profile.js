/* =============================================================================
   Signal profile

   The screen renders THIS, not a hard-coded layout. Every value the driver sees
   is declared here with its status, and the renderer draws nothing whose status
   is not "terbukti".

   That is the point. The house rule says an ECU number may never appear without
   a confidence marker, and a rule enforced by discipline is a rule that survives
   until the first hurried afternoon. Enforced by the data instead, a signal
   nobody has proven cannot reach the glass even if somebody writes markup for
   it.

   Status ladder, from bmt-can-signal-mapping:

     dugaan    the number is plausible, never tested against anything
     kandidat  matches the vehicle's own screen, but only while parked
     terbukti  the value MOVED with the vehicle and survived a proof
     gugur     was raised, then withdrawn by later evidence

   Kandidat is not half-proven. It is not proven at all: any field holding the
   right number while standing still looks exactly as convincing as a correct
   mapping.

   THE IDENTIFIERS BELOW BELONG TO A DFSK GELORA E, the test vehicle. The fleet
   runs Wuling and has never been mapped. They are here as the SHAPE of a
   profile, never as fleet truth, and copying them into a fleet profile is the
   fastest way to make the system display a confident wrong number.
   ============================================================================= */

export const VEHICLE = {
  slug: 'dfsk-gelora-e',
  label: 'DFSK Gelora E',
  note: 'kendaraan uji, bukan kendaraan armada',
  bitrate: 250000,
  evidence: 'bmt-avanza-iot-testing/docs/evidence/dfsk-gelora-e/gelora-004.md'
};

/* Who is driving, and in what.

   NOT an ECU value and not a reading of any kind. On the real device this
   arrives at sign-on, from the dispatch server by way of the Driver App, which
   is where the SELARIDE documentation puts the whole order flow: "Seluruh order
   lewat Driver App; MDT berfungsi sebagai argometer".

   It is on the glass because a taxi screen is a public-facing object. The
   passenger sitting behind it is the one who needs to know who is driving and
   which vehicle they are in, and it is the first thing they look for when
   something goes wrong. The reference the operator supplied puts the same three
   facts in the same place.

   THESE VALUES ARE PLACEHOLDERS. No driver has been enrolled, no fleet plate
   has been issued to this build, and nothing here came from an operator record.
   The screen says so in its provenance line. */
export const DRIVER = {
  name:   'I Made Suardana',
  nip:    '2026-0142',
  taxi:   'SR-017',
  plate:  'DK 1234 AB',
  phone:  '0812 3456 7890',
  org:    'Koperasi Baswara',
  /* Rating and trip count are dispatch values like the rest of this object, and
     like the rest of it they are EXAMPLES. Nobody has driven a trip on this
     system yet, so a 4,9 on the glass is a shape, not a score. */
  rating: 4.9,
  trips:  128,
  /* The portrait at mdt-ui/img/driver.jpg. Supplied by the operator on
     27 September and AI-GENERATED, which is the point: it is a painted sample
     of nobody, so no real person's face sits on a card carrying a made-up name,
     NIP and plate. Replace the file to change the face; the screen falls back
     to a drawn stand-in if it is ever missing. */
  photo:  '/img/driver.jpg'
};

/* A signal the screen may draw.

   `source` is documentation, not code: the MDT never decodes a byte (D-006). It
   is carried so that anyone reading this file can find the evidence behind the
   number without leaving it.

   `range` is the instrument's span, not the signal's. A speedometer that ends
   at the fastest value ever recorded is a speedometer that redraws itself. */
export const SIGNALS = [
  {
    id: 'speed',
    label: 'Kecepatan',
    unit: 'km/jam',
    status: 'terbukti',
    decimals: 0,
    range: [0, 120],
    source: {
      can: '0x18FFDC01', bytes: 'b4-b5 LE', formula: 'km/jam = nilai / 256',
      proof: 'integrasi kecepatan 4,031 km melawan odometer naik 4 km di bus yang sama'
    }
  },
  {
    id: 'rpm',
    label: 'Putaran motor',
    unit: 'rpm',
    status: 'terbukti',
    decimals: 0,
    range: [0, 6000],
    source: {
      can: '0x0CFF7902', bytes: 'b4-b5 LE', formula: 'rpm = nilai - 12000',
      proof: 'regresi 11.765 pasangan terhadap kecepatan; meleset 1 rpm dari panel'
    }
  },
  {
    id: 'soc',
    label: 'Daya baterai',
    unit: '%',
    status: 'terbukti',
    decimals: 0,
    range: [0, 100],
    source: {
      can: '0x0CFF7D03', bytes: 'b1', formula: '% = nilai x 0,5',
      proof: 'turun 74,0 ke 71,0 % sepanjang uji jalan 47 menit'
    }
  },
  {
    id: 'odometer',
    label: 'Odometer',
    unit: 'km',
    status: 'terbukti',
    decimals: 0,
    source: {
      can: '0x18FEDCD5', bytes: 'b1-b2 LE', formula: 'km apa adanya',
      proof: 'naik 30423 ke 30427 selama uji jalan'
    }
  },
  {
    id: 'pack',
    label: 'Tegangan pack',
    unit: 'V',
    status: 'terbukti',
    decimals: 1,
    source: {
      can: '0x0CFF7E03', bytes: 'b2-b3 LE', formula: 'volt apa adanya',
      proof: 'melorot ke 333 V saat arus memuncak lalu pulih'
    }
  },
  {
    id: 'current',
    label: 'Arus',
    unit: 'A',
    status: 'terbukti',
    decimals: 0,
    signed: true,
    range: [-60, 150],
    source: {
      can: '0x0CFF7E03', bytes: 'b4-b5 LE', formula: 'A = nilai - 1000',
      proof: '+133 A lalu -36 A dalam satu detik saat pedal dilepas di 46 km/jam'
    }
  },
  {
    id: 'battTempMax',
    label: 'Suhu baterai tertinggi',
    unit: '°C',
    status: 'terbukti',
    decimals: 0,
    source: {
      can: '0x0CFF7E03', bytes: 'b6', formula: '°C = nilai - 40',
      proof: 'naik 31 ke 32 °C setelah pemakaian'
    }
  },
  {
    id: 'battTempMin',
    label: 'Suhu baterai terendah',
    unit: '°C',
    status: 'terbukti',
    decimals: 0,
    source: {
      can: '0x0CFF7E03', bytes: 'b7', formula: '°C = nilai - 40',
      proof: 'bergerak bersama b6 sepanjang uji jalan'
    }
  },

  /* Below this line: declared so the gap is visible in the profile rather than
     invisible in the markup. The renderer skips every one of them.

     These are exactly the tiles the reference image showed and this build does
     not: eco mode, gear position, headlights, parking brake. None has a proven
     signal. Showing them would have meant inventing four numbers on a screen
     whose whole argument is that it does not. */
  { id: 'gear',      label: 'Posisi gigi',   status: 'belum-dipetakan' },
  { id: 'ecoMode',   label: 'Mode eco',      status: 'belum-dipetakan' },
  { id: 'headlight', label: 'Lampu utama',   status: 'belum-dipetakan' },
  { id: 'parking',   label: 'Rem parkir',    status: 'belum-dipetakan' },
  { id: 'soh',       label: 'SOH baterai',   status: 'kandidat',
    note: 'tidak pernah berubah selama pengamatan, jadi belum teruji sama sekali' },
  { id: 'ctrlTemp',  label: 'Suhu controller', status: 'gugur',
    note: 'tidak bergerak sedetik pun dalam 47 menit berkendara, termasuk saat arus 133 A' }
];

export const byId = Object.fromEntries(SIGNALS.map(s => [s.id, s]));

/* The only question the renderer asks before drawing anything. */
export const isProven = id => byId[id] ? byId[id].status === 'terbukti' : false;

/* Values the screen shows that are NOT ECU readings: they come from the device
   and from the dispatch server, so the confidence rule does not govern them.
   Kept in one list so nobody has to guess which column a value came from. */
export const NON_ECU = ['clock', 'dispatch', 'driver', 'gps', 'network', 'nav',
                        'trip', 'tripKm', 'tripSec', 'fare'];

/* `fare` deserves a note. It is not an ECU value and the status rule above does
   not govern it, but it is COMPUTED FROM one: trip distance, which comes from
   the speed signal proven on the bus. So the fare inherits that signal's
   trustworthiness exactly, and it goes stale when the feed does. Its commercial
   parameters live in tariff.js, never here. */
