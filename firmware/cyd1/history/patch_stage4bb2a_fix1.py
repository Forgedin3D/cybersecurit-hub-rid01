from pathlib import Path
import sys

path = Path('CYD1_RID_Handheld.ino')

if not path.exists():
    print('[ERROR] CYD1_RID_Handheld.ino not found in current directory.')
    sys.exit(1)

text = path.read_text()

if 'STAGE 4B-B2A UNIFIED HANDHELD UI' not in text:
    print('[ERROR] Stage 4B-B2A unified UI marker not found. This fix is only for the B2A candidate.')
    sys.exit(1)

if 'STAGE 4B-B2A FIX 1: EXPLICIT UI PROTOTYPES' in text:
    print('[ERROR] Fix 1 already appears to be installed.')
    sys.exit(1)

anchor = '''};\n\nbool captureUITargetSnapshot(\n'''

if text.count(anchor) != 1:
    print(f'[ERROR] Expected one UITargetSnapshot/function seam; found {text.count(anchor)}.')
    sys.exit(1)

prototypes = r'''// ============================================================
// STAGE 4B-B2A FIX 1: EXPLICIT UI PROTOTYPES
//
// Arduino's .ino preprocessor auto-generates function prototypes.
// Functions below use custom UI types, so declare them explicitly
// after those types are fully defined. This prevents the generated
// prototypes from referring to UITargetSnapshot / UIButton / ButtonID
// before those types are known to the compiler.
// ============================================================

bool captureUITargetSnapshot(
    uint8_t index,
    UITargetSnapshot &snapshot
);

const char *uiTargetStateName(
    const UITargetSnapshot &snapshot
);

uint16_t uiTargetStateColor(
    const UITargetSnapshot &snapshot
);

bool pointInside(
    double screenX,
    double screenY,
    const UIButton &button
);

void drawButton(
    const UIButton &button,
    bool selected
);

void drawTargetTab(
    const UIButton &button,
    uint8_t index
);

ButtonID hitTestCurrentScreen(
    double x,
    double y
);

void logButtonEvent(
    ButtonID button,
    double x,
    double y
);

void processButton(
    ButtonID button
);

'''

text = text.replace(
    anchor,
    '};\n\n' + prototypes + 'bool captureUITargetSnapshot(\n',
    1,
)

path.write_text(text)

print('[OK] Stage 4B-B2A Fix 1 applied.')
print('[OK] Added explicit prototypes after the custom UI type definitions.')
print('[OK] RID decoder, BLE controller, touch calibration, and UI logic were not changed.')
print('[NEXT] Compile only. Do NOT upload yet.')
