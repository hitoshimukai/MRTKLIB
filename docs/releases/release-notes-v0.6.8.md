# Release Notes — v0.6.8

## Real-time IGS-RTS float PPP via `correction = "igs-rts"`

**Release date:** 2026-05-23
**Type:** Feature — new correction source (real-time RTCM-SSR / IGS-SSR float PPP)
**Branch:** `release/v0.6.8`

---

### Overview

v0.6.8 enables the **`correction = "igs-rts"`** axis: conventional float PPP
driven by a real-time **RTCM-SSR / IGS-SSR (MT4076)** correction stream — the
IGS Real-Time Service and equivalents (IGS01 / IGS03, CNES `CLK9x`, BKG, …).

The decode-and-apply pipeline was already present in MRTKLIB, inherited from the
MADOCA-via-RTCM integration (RTCM-SSR decoders, `satpos_ssr` /
`EPHOPT_SSRAPC`). v0.6.7 introduced the `correction` axis but kept `igs-rts`
**reserved**. This release unlocks it and — importantly — routes it through the
**correct measurement model**, then validates it end-to-end against a real RTS
stream and upstream RTKLIB.

This builds on the v0.6.7 `correction`-axis framework
([`docs/design/configuration.md`](../design/configuration.md)). The remaining
real-time SSR sources `gal-has` and `bds-b2b` stay reserved.

---

### Major changes

#### `correction = "igs-rts"` unlocked (#138)

`resolve_correction()` ([`src/pos/mrtk_opt.c`](https://github.com/h-shiono/MRTKLIB/blob/main/src/pos/mrtk_opt.c)):

- Removes `igs-rts` from the reserved-reject (`gal-has` / `bds-b2b` stay reserved).
- Adds it to the PPP validity matrix (`ppp-static` / `ppp-kine` / `ppp-fixed`).
- Requires `satellite_ephemeris = "brdc+ssrapc"` (or `"brdc+ssrcom"`).
- **Not auto-inferred** — `igs-rts` shares `brdc+ssrapc` with `qzs-madoca`, so it
  must be set explicitly (omitting `correction` with `brdc+ssrapc` still infers
  `qzs-madoca`, preserving MADOCA behaviour).

#### Correct measurement model for RTCM-SSR

`corr_meas()` ([`src/pos/mrtk_ppp.c`](https://github.com/h-shiono/MRTKLIB/blob/main/src/pos/mrtk_ppp.c))
now routes `igs-rts` through the **RTKLIB float-PPP measurement model** — the
same branch as `correction = igs` — **not** the MADOCA SSR-bias path. The
satellite orbit/clock still come from `satpos_ssr` (`EPHOPT_SSRAPC`); only the
receiver-side measurement model differs.

The per-signal RTCM-SSR code/phase bias is deliberately **not** applied:
RTCM-SSR uses a different bias convention than MADOCA-CSSR (the code bias even
has the opposite sign), and applying it via the MADOCA path biases the solution
by metres. The MADOCA / `gal-has` / `bds-b2b` SSR path is **unchanged
(bit-identical)**.

#### Validation

End-to-end against a real RTS stream captured live from the CDDIS NTRIP caster:
**AIRA00JPN** (Aira, Japan, IGS), GPS L1/L2 + GAL E1/E5a iono-free, IGS combined
RTCM-SSR (`SSRA02IGS0`) + broadcast ephemeris. Against the IGS final weekly
SINEX coordinate (epoch-propagated), float PPP reaches **3D 1σ ≈ 0.18 m / 95% ≈
0.33 m** — the expected real-time IGS-RTS float level — and **agrees with
upstream RTKLIB 2.4.3 `rnx2rtkp`** on the identical obs/nav/SSR inputs to the
LAPACK-vs-embedded-LU solver level. `satpos_ssr` is byte-identical to upstream.

A new `igsrts_setup` / `igsrts_ppp` / `igsrts_ppp_check` / `igsrts_cleanup`
regression group exercises this path (SINEX-based absolute check, tolerance
0.5 m).

#### Release-notes hygiene

The previously missing
[`release-notes-v0.6.7.md`](release-notes-v0.6.7.md) was backfilled.

---

### Files Changed

| File | Change |
|------|--------|
| `src/pos/mrtk_opt.c` | unlock `CORR_IGS_RTS` in `resolve_correction()` (validity matrix + `sateph` guard, explicit-only) |
| `src/pos/mrtk_ppp.c` | route `igs-rts` through the RTKLIB float-PPP measurement model in `corr_meas` |
| `conf/igs/rnx2rtkp_igsrts.toml` | `igs-rts` sample config |
| `conf/igs/rnx2rtkp_igsrts_test.toml`, `tests/data/igs/igsrts_testdata.tar.gz`, `CMakeLists.txt` | `igsrts` regression test |
| `docs/design/configuration.md`, `docs/reference/config-options.md` | `igs-rts` documented as implemented (float) |
| `docs/releases/release-notes-v0.6.7.md` | backfilled v0.6.7 notes |

---

### Upgrade notes

- **Existing configs need no change.** `igs-rts` is opt-in and explicit.
- To run real-time IGS-RTS float PPP, set `correction = "igs-rts"` and
  `satellite_ephemeris = "brdc+ssrapc"`, and feed the RTCM-SSR stream (live, or
  a recorded `.rtcm3` log for `mrtk post`) alongside the rover obs and a
  broadcast navigation file. See `conf/igs/rnx2rtkp_igsrts.toml`.
- `igs-rts` is **float** PPP. Integer PPP-AR (a phase-bias product such as CNES
  `CLK9x`) and the F9P-class GAL E5b / BDS B2I fallback are follow-ups.

### Test Results

The `igsrts` regression group passes (AIRA, 3D 1σ ≈ 0.18 m vs SINEX). The only
failures are the two known pre-existing ones on the maintainer host —
`rtkrcv_rt` (real-time timing) and `madocalib_pppar_ion_check` (LAPACK vs
embedded-LU numerical difference, see CLAUDE.md §7.2) — both unrelated to this
release. CI `build` + `test-regression` are green. MADOCA / CLAS / IGS-files PPP
paths are unchanged.

---

### PRs

- [#146](https://github.com/h-shiono/MRTKLIB/pull/146) — `feat(ppp): enable IGS-RTS correction (RTCM-SSR / IGS-SSR) float PPP (#138)`
- [#145](https://github.com/h-shiono/MRTKLIB/pull/145) — `docs(releases): backfill v0.6.7 release notes`

### Closes

- [#138](https://github.com/h-shiono/MRTKLIB/issues/138) — `feat(ppp): enable IGS-RTS correction (RTCM-SSR / IGS-SSR MT4076) — pipeline ported, unlock the axis`
