# Release Notes — v0.6.7

## Conventional IGS precise-products float PPP via a new `correction` configuration axis

**Release date:** 2026-05-21
**Type:** Feature — new positioning capability (IGS-products float PPP) + a multi-GNSS iono-free fix
**Branch:** `release/v0.6.7`

---

### Overview

v0.6.7 adds conventional **IGS precise-products float PPP** to MRTKLIB. A new
`correction` configuration axis decouples the *correction source* from the
positioning `mode`, so `mrtk post` can now run float PPP from precise IGS
products (SP3 / CLK / Bias-SINEX / ERP) — not only from the QZSS/MADOCA and
CLAS augmentation paths it already supported.

It also fixes a long-standing iono-free gap that silently dropped Galileo and
BeiDou on **F9P-class receivers** whose second band is GAL **E5b** (`C7Q`) or
BDS **B2I** (`C7I`) with no E5a/B3I.

This release establishes the `correction`-axis framework (design in
[`docs/design/configuration.md`](../design/configuration.md)). The
real-time SSR sources `igs-rts`, `gal-has`, and `bds-b2b` remain **reserved**
(accepted by the enum but rejected at load time) and are implemented in later
releases.

---

### Major changes

#### `correction` configuration axis (#130, #136)

A new TOML key `correction` names the correction source independently of `mode`:

- Implemented: `none`, `igs` (precise SP3/CLK files), `qzs-madoca`, `qzs-clas`.
- Reserved (enum-only, rejected at load time): `igs-rts`, `gal-has`, `bds-b2b`.

When `correction` is omitted it is **inferred** from `mode` +
`satellite_ephemeris` for backward compatibility, so existing configs keep
working unchanged. `resolve_correction()` validates the `(mode, correction)`
pair at load time (a hard error on an invalid combination).

#### IGS precise-products float PPP path

`corr_meas()` now branches on the correction source (IGS files / SSR / none),
replacing the previous SSR-only measurement path. For `correction = igs` it
uses the RTKLIB-style float-PPP model: applies optional P1-C1 / P2-C2 DCB from
`nav->cbias`, never discards a measurement for a missing satellite bias, and
lets the float phase-ambiguity absorb the satellite phase bias. SP3 / CLK /
Bias-SINEX / ERP products are read into the `nav` fields the IGS path consumes.

#### Data-driven iono-free pair for IGS PPP (#135)

For `correction = igs`, the iono-free pair is selected from the **available**
observations. GAL E5b (`C7Q`) / BDS B2I (`C7I`) satellites on receivers without
E5a/B3I (e.g. u-blox ZED-F9P) are no longer silently dropped. The legacy
`pos2-sig` obsdef reshaping is skipped so the receiver's actual second band
survives, and a one-line-per-system stderr notice reports any non-nominal
second-band auto-selection. The change is gated on `correction == igs` and is a
no-op when the conventional slot-1 band is present — the IGS geodetic
regression `.pos` is bit-identical, and MADOCA / CLAS / RTK paths are unchanged.

#### Regression tests

- **`igs_ppp`** (#137) — GSI GEONET station 3034 (FUJISAWA), GPS+GAL, validated
  against the published GSI daily coordinate
  (`igs_setup` / `igs_ppp` / `igs_ppp_check` / `igs_cleanup`).
- **`igs_iflc`** (#135) — ECJ02 u-blox F9P, 1 h window, exercising the GAL E5b /
  BDS B2I iono-free fallback against a full-day static-PPP reference coordinate
  (lightweight 0.9 MB data archive).
- **`compare_pos_abs.py`** gains `--llh` and `--ecef` fixed-reference modes.

---

### Files Changed

| File | Change |
|------|--------|
| `src/pos/mrtk_opt.c`, `include/mrtklib/mrtk_opt.h` | `correction` enum + `resolve_correction()` |
| `src/pos/mrtk_ppp.c` | `corr_meas` IGS/SSR/none branches; data-driven IFLC pair; auto-select notice |
| `src/pos/mrtk_spp.c` | data-driven IFLC 2nd-frequency for `correction = igs` |
| `apps/rnx2rtkp/rnx2rtkp.c` | `resolve_correction()` wiring; gate `apply_pppsig` off for IGS PPP |
| `conf/igs/*.toml` | IGS PPP sample configs |
| `tests/data/igs/*`, `CMakeLists.txt` | `igs_ppp` and `igs_iflc` regression tests |
| `scripts/tests/compare_pos_abs.py` | `--llh` / `--ecef` reference modes |
| `docs/design/configuration.md` | correction-axis design and validity matrix |

---

### Upgrade notes

- **Existing configs need no change.** `correction` is optional; when omitted it
  is inferred from `mode` + `satellite_ephemeris` exactly as before.
- New IGS-products PPP setups should set `correction = "igs"` and provide the
  precise products via `[files]`. Note the existing input-routing trap: an OSB
  Bias-SINEX file must go to `bias_sinex =` (→ `readbsnx`), **not** `dcb =`
  (→ the legacy CODE-DCB text parser, which silently ignores SINEX).
- MADOCA / CLAS / VRS-RTK behaviour is unchanged (those sources are inferred to
  `qzs-madoca` / `qzs-clas`).

### Test Results

The IGS PPP (`igs_ppp`) and IFLC fallback (`igs_iflc`) regression groups pass.
The only failures are the two known pre-existing ones on the maintainer host —
`rtkrcv_rt` (real-time timing) and `madocalib_pppar_ion_check` (LAPACK vs
embedded-LU numerical difference, see CLAUDE.md §7.2) — both unrelated to this
release. The IGS geodetic regression `.pos` is bit-identical with the IFLC fix
applied (no-op when the nominal band is present).

---

### PRs

- [#130](https://github.com/h-shiono/MRTKLIB/pull/130) — `feat(ppp): add correction config axis with IGS-files float PPP path`
- [#136](https://github.com/h-shiono/MRTKLIB/pull/136) — `feat: correction-axis phase 1`
- [#137](https://github.com/h-shiono/MRTKLIB/pull/137) — `test: IGS-products PPP regression`
- [#139](https://github.com/h-shiono/MRTKLIB/pull/139) — `fix(ppp): use available 2nd frequency for IGS iono-free PPP (#135)`
- [#140](https://github.com/h-shiono/MRTKLIB/pull/140) — `release: v0.6.7`
- [#143](https://github.com/h-shiono/MRTKLIB/pull/143) — `fix: address Copilot review on PR #140`

### Closes

- [#130](https://github.com/h-shiono/MRTKLIB/issues/130) — `feat: add a correction config axis (decouple from mode)`
- [#135](https://github.com/h-shiono/MRTKLIB/issues/135) — `IGS iono-free PPP drops GAL E5b / BDS B2I on receivers without E5a/B3I`
