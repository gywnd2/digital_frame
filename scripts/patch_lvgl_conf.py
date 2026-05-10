from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
pio_env = env.subst("$PIOENV")
lv_conf = project_dir / ".pio" / "libdeps" / pio_env / "lv_conf.h"

if not lv_conf.exists():
    print("[LVGL] lv_conf.h not found yet; skipping font config patch")
else:
    text = lv_conf.read_text(encoding="utf-8")
    replacements = {
        "#define LV_FONT_MONTSERRAT_24 0": "#define LV_FONT_MONTSERRAT_24 1",
        "#define LV_FONT_MONTSERRAT_28 0": "#define LV_FONT_MONTSERRAT_28 1",
        "#define LV_FONT_MONTSERRAT_32 0": "#define LV_FONT_MONTSERRAT_32 1",
    }

    patched = text
    for old, new in replacements.items():
        patched = patched.replace(old, new)

    if patched != text:
        lv_conf.write_text(patched, encoding="utf-8")
        print("[LVGL] Enabled Montserrat 24/28/32 fonts in lv_conf.h")
