# captures/

Raw CAN captures pulled off the device. The `.log` files themselves are
git-ignored — they are large and per-vehicle. What belongs in git is the
finding, not the raw bus traffic.

## Naming

```
avanza-<year>-<plate-or-vin6>-<yyyymmdd>-<hhmm>-<what>.log
```

Example:

```
avanza-2016-B1234XY-20260824-1030-ignition-on-stationary.log
avanza-2016-B1234XY-20260824-1105-driving-40kmh.log
```

Put the observed dashboard reading in the filename. A capture whose dashboard
value nobody wrote down is nearly worthless later.

## Pulling one off the device

```
capture off
ls
cat /capture/can-000.log
```

Save the console output here.

## Searching one

```sh
python ../tools/can_find_value.py <file> --value 40 --name speed \
    --tolerance 1 --no-stable
```

Remember: an Avanza candidate is a hypothesis about a **Toyota**. The fleet runs
a Wuling EV. Nothing found here transfers.

## Safety

Capturing while driving is a **two-person** job. One drives, one operates.
