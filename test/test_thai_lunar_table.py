"""Check the Thai lunar year table against officially announced holiday dates."""

from datetime import date, timedelta
from pathlib import Path
import csv
import importlib.util
import sys

ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "generate_thai_lunar_table.py"
HEADER = ROOT / "src" / "util" / "ThaiLunarTable.h"
FIXTURE = ROOT / "test" / "thai_calendar" / "fixtures" / "official_holidays.csv"

LENGTHS = {0: 354, 1: 355, 2: 384}


def load_generator():
    spec = importlib.util.spec_from_file_location("thai_lunar_gen", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def month_starts(start: date, year_type: int) -> list[date]:
    lengths = [29, 30] * 6
    if year_type == 2:
        lengths.insert(8, 30)  # month 8 หลัง after month 8
    elif year_type == 1:
        lengths[6] += 1
    starts = [start]
    for n in lengths[:-1]:
        starts.append(starts[-1] + timedelta(n))
    return starts


def holiday_date(start: date, year_type: int, name: str) -> date:
    m = month_starts(start, year_type)
    extra = 1 if year_type == 2 else 0
    full = {"makha": 2 + extra, "visakha": 5 + extra, "asalha": 7 + extra, "loy": 11 + extra}
    return m[full[name]] + timedelta(14)


def main() -> int:
    gen = load_generator()
    years = gen.years()
    failures = []

    for prev, cur in zip(years, years[1:]):
        if prev.start + timedelta(LENGTHS[prev.type]) != cur.start:
            failures.append(f"gap between {prev.label} and {cur.label}")
    if years[0].start > date(2026, 1, 1):
        failures.append(f"table starts after 2026-01-01: {years[0].start}")
    last = years[-1]
    if last.start + timedelta(LENGTHS[last.type]) <= date(2047, 12, 31):
        failures.append("table ends before 2047-12-31")

    by_label = {y.label: y for y in years}
    with FIXTURE.open(encoding="utf-8") as f:
        for row in csv.DictReader(f):
            y = by_label.get(int(row["lunar_year_gregorian_label"]))
            if y is None:
                failures.append(f"missing year {row['lunar_year_gregorian_label']}")
                continue
            got = holiday_date(y.start, y.type, row["holiday"])
            if got.isoformat() != row["date"]:
                failures.append(f"{row['holiday']} {y.label}: {got} != {row['date']}")

    if HEADER.read_text(encoding="utf-8") != gen.render(years):
        failures.append("ThaiLunarTable.h is stale; rerun the generator")

    for msg in failures:
        print("FAIL:", msg)
    print("ok" if not failures else f"{len(failures)} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
