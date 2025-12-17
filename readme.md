# Leaf can-filter

---

**leaf_can_filter.h**    - single header module (main logic)

**leafspy_can_filter.h** - single header module (leafspy message filtering)

---

## Leafspy messages (Reverse engineering):
Total message length: 41 bytes, Nissan Leaf (ze0) 2012,
iso-tp LBC(0x79B,0x7BB), UDS service data id (0x01):
```C
        0,     1 : (UDS service response bytes)
MSB ->  2, ... 5 : (int32_t)current_A_0 /* unknown value, not in LeafSpy */
        6,     7 : /* Unknown purpose (not found during survey) */
MSB ->  8, ... 11: (int32_t)current_A_1 /* in LeafSpy */
       12, ... 19: /* Unknown purpose (not found during survey) */
MSB -> 20,     21: (uint16_t)voltage_V /* in LeafSpy */
       22, ... 27: /* Unknown purpose (not found during survey) */
MSB -> 28,     29: (uint16_t)hx /* in LeafSpy */
       30        : /* Unknown purpose (not found during survey) */
MSB -> 31,     33: (uint24_t)soc_pct /* in LeafSpy */
       34        : /* Unknown purpose (not found during survey) */
MSB -> 35,     36: (uint16_t)ah /* in LeafSpy */
       37, ... 40: /* Unknown purpose (not found during survey) */
```

## To-Do List

---
* ~~energy counter must have 0.5V/bit precision~~
* ~~override bms capacity~~
* ~~Write filesystem~~
* ~~Write web interface~~
* Eliminate race conditions (status is unknown)
* More clear separation for various vehicles
* ~~ISO-TP subset for effective can filtering of leafspy requests~~
* ISO-TP more complete version 
* Redesign WEB interface
* Enhance compatiblity between hardware variants
* Store version details based on request
* More unified interface to make CAN filter more universal
* add overcharge
* chademo power limit
* recuperation multiplier

**Completion: 5/15**

---
