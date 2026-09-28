/* =============================================================================
   Tariff

   SALINAN VERBATIM dari project-mdt-tds/mdt-ui/tariff.js.

   Ini satu-satunya duplikasi angka rupiah di seluruh sistem, dan ia disengaja:
   alat ini tidak bisa mengambil berkas dari repo lain saat build. Kalau tarif
   berubah, ubah di repo itu dulu, baru salin ke sini. Jangan pernah mengetik
   ulang angkanya di sini.

   The fare the passenger pays. Kept in its own file, and deliberately small,
   because this is the one part of the screen that is about MONEY and it will be
   edited by somebody who is not reading the rest of the code.

   EVERY NUMBER HERE HAS A SOURCE, AND THE SOURCE IS NAMED.

   The authority is the SELARIDE documentation for PT Baswara Trans Selaras, on
   the operator's Drive, and specifically the answer sheet
   "Pertanyaan-untuk-Baswara_SELARIDE_v3.9_2026-09-02.docx", whose tariff rows
   were answered by Baswara on 25 August 2026 and confirmed on 26 August with
   the word "Betul". Where a number here does NOT come from there, it says so.

   Tariff changes are not a developer's call. The same sheet records the rule:
   "Distribusi tarif diinput menurut ketentuan yang disetujui Direktur dan
   Direktur Utama". Editing a figure in this file without that approval puts a
   number on a passenger's screen that nobody authorised.

   The fare is NOT an ECU value and the confidence rule in profile.js does not
   govern it. What it is computed FROM is: trip distance and speed, from signals
   proven on the bus. So the fare is only as good as those signals, and it goes
   stale when they do.
   ============================================================================= */

export const TARIFF = {
  /* Rupiah per kilometre. Confirmed by Baswara.

     26 August 2026: "Betul, yang 8,500 itu hanya dummy testing sebelumnya".
     Worth knowing, because Rp 8.500 reached this build anyway: it was the first
     figure given here on 26 September before being revised the same day. The
     documentation had settled it a month earlier. */
  perKm: 8200,

  /* The billing basis, and the rule this build had wrong before the
     documentation was read.

     Baswara, 26 August 2026, on what the billing basis is: "Kilometer isi,
     yaitu kilometer berbayar", LOADED kilometres. Confirmed "Betul".

     So the meter charges only while a passenger is aboard. Distance driven to
     reach a passenger, or back to the pool, is kilometer kosong and is not
     billed. The first version of this meter charged every kilometre once it
     started, which would have overcharged every trip.

     The same sheet applies the same idea to working hours: "Yang kita
     kategorikan jam mengemudi adalah ketika isi penumpang". */
  billOnlyWhenOnHire: true,

  /* No opening charge. Inferred, and the arithmetic is the evidence.

     The daily target is recorded as "85 kilometer, setara Rp 697.000", and
     85 x 8.200 = 697.000 exactly. A flag-fall would have made the target larger
     than distance alone accounts for.

     One residual assumption: that target may be an aggregate over several
     trips, in which case a per-trip flag-fall could still exist and simply not
     show up in the figure. Nobody has been asked directly. Zero is a reasoned
     position, not a confirmed one, and it is in the open questions. */
  flagFall: 0,

  /* The distance meter ticks every 100 m rather than billing a continuous
     distance and then rounding the money.

     Not a detail. Billing raw distance and rounding rupiah made the screen
     contradict itself: it printed one distance beside a rate, and a passenger
     doing that multiplication got a different number from the meter. Ticking
     removes it at the source, and one tick is a whole number of rupiah
     (0,1 km x Rp 8.200 = Rp 820), so no rounding rule needs inventing.

     Check this whenever the price changes: a rate that is not a multiple of ten
     would put fractions of a rupiah back into the total. */
  kmStep: 0.1,

  /* ---- NOT IN THE DOCUMENTATION -------------------------------------------

     Waiting time, rupiah per minute, and the speed below which it runs.

     Source: Surya Wirasdyartha, WhatsApp, 26 September 2026 at 13.02:
       "Waktu tunggu pakai dl 5k permenit"
       "Jika kexepatan di bawah 5 km/h saat argo aktif, waktu tunggu idup"

     Implemented as given. Three things about it belong in front of whoever
     signs it off, because none of them is visible from the numbers alone:

     1. NO WAITING TARIFF APPEARS ANYWHERE IN THE SELARIDE DOCUMENTATION. The
        confirmed basis is distance alone. This is a new charge, so it needs the
        Direktur and Direktur Utama approval the tariff rule requires.

     2. Rp 5.000 per minute is Rp 300.000 per hour, and at Rp 8.200 per km that
        is what the meter earns driving 36,6 km/jam. A vehicle stuck in traffic
        bills roughly what a vehicle moving at city speed bills.

     3. The meter is subject to TERA, legal metrology sealing. Baswara Finance
        carries a role called "Pengendali Tarif dan Tera", and the permit regime
        is a taxi permit under PM 117/2018. Adding a time component changes what
        the sealed instrument computes, which is a question for the metrology
        authority and not only a commercial one.

     Until it is approved, the screen marks this line as awaiting approval
     rather than presenting it as settled. */
  perMinute: 5000,
  waitBelowKmh: 5,
  waitStepMin: 0.1,
  waitApproved: false,

  currency: 'Rp'
};

/* A running meter.

   Stateful on purpose: what a fare owes depends on how the trip was spent, not
   only on where it ended. Two kilometres driven loaded and two driven empty are
   different fares, and no function of total distance can tell them apart.

   Fed a reading each frame, it splits the trip into distance charged and time
   charged, and never charges both for the same second.

   `onHire` is the passenger being aboard. On the real device that comes from
   the dispatch flow, which the documentation places in the Driver App:
   "Seluruh order lewat Driver App; MDT berfungsi sebagai argometer". This
   screen IS the argometer. It is told; it does not decide. */
export function createMeter() {
  let billedKm = 0;
  let waitSec  = 0;
  let prevKm   = null;
  let waiting  = false;
  let onHire   = false;

  return {
    update(speed, tripKm, dt, hired) {
      if (prevKm === null) prevKm = tripKm;
      const dKm = Math.max(0, tripKm - prevKm);
      prevKm = tripKm;

      const was = onHire;
      onHire = !!hired;
      if (!was && onHire) { billedKm = 0; waitSec = 0; }   // a new trip starts at zero

      // Kilometer kosong: the vehicle moves, the meter does not.
      if (TARIFF.billOnlyWhenOnHire && !onHire) {
        waiting = false;
        return this.read();
      }

      waiting = speed < TARIFF.waitBelowKmh;
      if (waiting) waitSec += dt;
      else billedKm += dKm;
      return this.read();
    },

    read() {
      const kmStep  = TARIFF.kmStep > 0 ? TARIFF.kmStep : 0.1;
      const minStep = TARIFF.waitStepMin > 0 ? TARIFF.waitStepMin : 0.1;
      const km      = Math.floor(billedKm / kmStep + 1e-9) * kmStep;
      const waitMin = Math.floor((waitSec / 60) / minStep + 1e-9) * minStep;
      const kmAmount   = Math.round(km * TARIFF.perKm);
      const waitAmount = Math.round(waitMin * TARIFF.perMinute);
      return {
        onHire, waiting,
        km, waitMin,
        kmAmount, waitAmount,
        base: TARIFF.flagFall,
        total: TARIFF.flagFall + kmAmount + waitAmount
      };
    }
  };
}

/* "Rp 12.700". Indonesian grouping, no decimals: rupiah has no subunit in use.
   The space is non-breaking: a currency word that wraps away from its amount is
   a different number. */
export function rupiah(amount) {
  return TARIFF.currency + ' ' + grouped(amount);
}

/* The same figure without the currency word, for the hero amount, where the
   markup sets "Rp" at its own size beside the digits. Using rupiah() there
   printed the currency twice, which at that size was not subtle. */
export function grouped(amount) {
  return Math.round(amount).toLocaleString('id-ID');
}

/* The line that lets a passenger check the arithmetic, naming the basis the
   documentation settled. */
export function rateLabel() {
  return rupiah(TARIFF.perKm) + ' per kilometer isi';
}

/* The rule that switched the meter, printed beside the charge it produced. */
export function waitRuleLabel() {
  return 'waktu tunggu ' + rupiah(TARIFF.perMinute) + ' per menit, di bawah '
       + TARIFF.waitBelowKmh + ' km/jam';
}
