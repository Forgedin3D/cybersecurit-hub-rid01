#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <math.h>

// ====================================================
// CYD1 UI STAGE 4A
// HANDHELD NAVIGATION / STATE MACHINE
//
// Remote ID backend NOT integrated yet.
// Uses synthetic demonstration target data.
//
// Validated Stage-2 homography +
// Stage-3 filtered touch input.
// ====================================================


// ====================================================
// TOUCH HARDWARE
// ====================================================

#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

SPIClass touchSPI = SPIClass(VSPI);

XPT2046_Touchscreen ts(
    XPT2046_CS,
    XPT2046_IRQ
);

TFT_eSPI tft = TFT_eSPI();


// ====================================================
// TOUCH CONFIG
// ====================================================

const int PRESSURE_MIN = 200;
const int TAP_SAMPLES  = 5;
const int HIT_PADDING  = 5;


// ====================================================
// VALIDATED HOMOGRAPHY
// UI STAGE 2 PASS
// ====================================================

const double h11 = 0.178699182386;
const double h12 = 0.047115489245;
const double h13 = -228.554894658;

const double h21 = 0.048093676990;
const double h22 = 0.138642946733;
const double h23 = -227.381646595;

const double h31 = 0.000113546161;
const double h32 = 0.000081218131;


// ====================================================
// SCREEN STATES
// ====================================================

enum ScreenState {

    SCREEN_HOME,
    SCREEN_PRIVACY,
    SCREEN_LIVE,
    SCREEN_SNAPSHOT
};

ScreenState currentScreen =
    SCREEN_HOME;


// ====================================================
// BUTTON IDS
// ====================================================

enum ButtonID {

    BTN_NONE = -1,

    BTN_BEGIN,
    BTN_PRIVATE,
    BTN_OPEN,
    BTN_START_SCAN,
    BTN_BACK_HOME,

    BTN_SNAPSHOT,
    BTN_RESCAN,
    BTN_MODE_TOGGLE,
    BTN_STOP,

    BTN_BACK_LIVE,
    BTN_TARGET_1,
    BTN_TARGET_2,
    BTN_TARGET_3,
    BTN_TARGET_4
};


// ====================================================
// BUTTON STRUCTURE
// ====================================================

struct UIButton {

    int x;
    int y;

    int w;
    int h;

    const char *label;
};


// ====================================================
// BUTTON GEOMETRY
// ====================================================

const UIButton buttonBegin = {
    40, 90, 240, 56,
    "BEGIN TEST"
};


const UIButton buttonPrivate = {
    15, 68, 135, 50,
    "PRIVATE"
};


const UIButton buttonOpen = {
    170, 68, 135, 50,
    "OPEN"
};


const UIButton buttonStartScan = {
    40, 138, 240, 48,
    "START SCAN"
};


const UIButton buttonBackHome = {
    100, 198, 120, 32,
    "BACK"
};


// Live-screen buttons.

const UIButton buttonSnapshot = {
    12, 160, 140, 32,
    "SNAPSHOT"
};


const UIButton buttonRescan = {
    168, 160, 140, 32,
    "RESCAN"
};


const UIButton buttonMode = {
    12, 200, 140, 32,
    "MODE"
};


const UIButton buttonStop = {
    168, 200, 140, 32,
    "STOP"
};


// Snapshot return button.

const UIButton buttonBackLive = {
    80, 198, 160, 34,
    "BACK TO LIVE"
};


// ====================================================

// ------------------------------------------------------------
// LIVE TARGET SELECTOR GEOMETRY
// ------------------------------------------------------------

const UIButton buttonTarget1 = {
    8, 42, 70, 30,
    "T01"
};

const UIButton buttonTarget2 = {
    86, 42, 70, 30,
    "T02"
};

const UIButton buttonTarget3 = {
    164, 42, 70, 30,
    "T03"
};

const UIButton buttonTarget4 = {
    242, 42, 70, 30,
    "T04"
};


// UI STATE
// ====================================================

bool privateMode = true;

uint32_t scanGeneration = 0;
uint32_t snapshotCount = 0;
uint32_t tapCounter = 0;

// ------------------------------------------------------------
// STAGE 4B-A MOCK MULTI-TARGET STATE
// ------------------------------------------------------------

uint8_t selectedTarget = 0;

const char *mockTargetState[4] = {
    "ACTIVE",
    "ACTIVE",
    "STALE",
    "EMPTY"
};

const char *mockTargetType[4] = {
    "MULTIROTOR",
    "MULTIROTOR",
    "FIXED WING",
    "--"
};

const char *mockFlightState[4] = {
    "GROUND",
    "AIRBORNE",
    "AIRBORNE",
    "--"
};

const int mockRSSI[4] = {
    -48,
    -63,
    -78,
    0
};

const int mockHeading[4] = {
    92,
    245,
    310,
    0
};

const float mockHSpeed[4] = {
    0.0f,
    5.2f,
    11.4f,
    0.0f
};

const float mockVSpeed[4] = {
    0.0f,
    1.0f,
    -0.5f,
    0.0f
};

const char *mockID[4] = {
    "LAB-RID-01",
    "LAB-RID-02",
    "LAB-RID-03",
    "--"
};

const double mockLatitude[4] = {
    1.2345678,
    1.2346678,
    1.2347678,
    0.0
};

const double mockLongitude[4] = {
    2.3456789,
    2.3457789,
    2.3458789,
    0.0
};


uint16_t mockTargetStateColor(uint8_t index) {

    if (index <= 1) {
        return TFT_GREEN;
    }

    if (index == 2) {
        return TFT_YELLOW;
    }

    return TFT_WHITE;
}


void drawTargetTab(
    const UIButton &button,
    uint8_t index
) {

    bool selected =
        selectedTarget == index;

    uint16_t fillColor =
        selected
        ? TFT_BLUE
        : TFT_BLACK;

    uint16_t borderColor =
        selected
        ? TFT_CYAN
        : mockTargetStateColor(index);

    tft.fillRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        5,
        fillColor
    );

    tft.drawRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        5,
        borderColor
    );

    tft.setTextColor(
        TFT_WHITE,
        fillColor
    );

    tft.drawCentreString(
        button.label,
        button.x + button.w / 2,
        button.y + 7,
        2
    );
}



// ====================================================
// MEDIAN FILTER
// ====================================================

int medianValues(
    int values[],
    int count
) {

    int temp[TAP_SAMPLES];


    for (
        int i = 0;
        i < count;
        i++
    ) {

        temp[i] =
            values[i];
    }


    for (
        int i = 0;
        i < count - 1;
        i++
    ) {

        for (
            int j = i + 1;
            j < count;
            j++
        ) {

            if (
                temp[j] < temp[i]
            ) {

                int swapValue =
                    temp[i];

                temp[i] =
                    temp[j];

                temp[j] =
                    swapValue;
            }
        }
    }


    return temp[
        count / 2
    ];
}


// ====================================================
// HOMOGRAPHY
// ====================================================

bool mapTouchToScreen(
    double rawTouchX,
    double rawTouchY,
    double &screenX,
    double &screenY
) {

    double denominator =
        h31 * rawTouchX
        +
        h32 * rawTouchY
        +
        1.0;


    if (
        fabs(denominator)
        <
        0.000001
    ) {

        return false;
    }


    screenX =
        (
            h11 * rawTouchX
            +
            h12 * rawTouchY
            +
            h13
        )
        /
        denominator;


    screenY =
        (
            h21 * rawTouchX
            +
            h22 * rawTouchY
            +
            h23
        )
        /
        denominator;


    return true;
}


// ====================================================
// TOUCH CAPTURE
// ====================================================

void waitForRelease() {

    unsigned long startTime =
        millis();


    while (
        ts.touched()
        &&
        millis() - startTime < 2500
    ) {

        delay(10);
    }


    delay(80);
}


bool captureFilteredTap(
    int &medianX,
    int &medianY,
    int &medianZ
) {

    if (
        !ts.tirqTouched()
        ||
        !ts.touched()
    ) {

        return false;
    }


    int sampleX[TAP_SAMPLES];
    int sampleY[TAP_SAMPLES];
    int sampleZ[TAP_SAMPLES];

    int validSamples = 0;


    unsigned long captureStart =
        millis();


    while (
        ts.touched()
        &&
        validSamples < TAP_SAMPLES
        &&
        millis() - captureStart < 400
    ) {

        TS_Point p =
            ts.getPoint();


        if (
            p.z >= PRESSURE_MIN
        ) {

            sampleX[validSamples] =
                p.x;

            sampleY[validSamples] =
                p.y;

            sampleZ[validSamples] =
                p.z;

            validSamples++;
        }


        delay(10);
    }


    waitForRelease();


    if (
        validSamples < 3
    ) {

        return false;
    }


    medianX =
        medianValues(
            sampleX,
            validSamples
        );


    medianY =
        medianValues(
            sampleY,
            validSamples
        );


    medianZ =
        medianValues(
            sampleZ,
            validSamples
        );


    return true;
}


// ====================================================
// BUTTON HELPERS
// ====================================================

bool pointInside(
    double screenX,
    double screenY,
    const UIButton &button
) {

    return (
        screenX >=
            button.x - HIT_PADDING
        &&
        screenX <=
            button.x
            +
            button.w
            +
            HIT_PADDING
        &&
        screenY >=
            button.y - HIT_PADDING
        &&
        screenY <=
            button.y
            +
            button.h
            +
            HIT_PADDING
    );
}


void drawButton(
    const UIButton &button,
    bool selected = false
) {

    uint16_t fillColor =
        TFT_DARKGREY;

    uint16_t borderColor =
        TFT_WHITE;

    uint16_t textColor =
        TFT_WHITE;


    if (
        selected
    ) {

        fillColor =
            TFT_BLUE;

        borderColor =
            TFT_CYAN;
    }


    tft.fillRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        6,
        fillColor
    );


    tft.drawRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        6,
        borderColor
    );


    tft.setTextColor(
        textColor,
        fillColor
    );


    tft.drawCentreString(
        button.label,
        button.x
            +
            button.w / 2,
        button.y
            +
            (
                button.h / 2
            )
            -
            7,
        2
    );
}


// ====================================================
// HEADER
// ====================================================

void drawHeader(
    const char *title
) {

    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );


    tft.drawCentreString(
        title,
        160,
        5,
        2
    );
}


// ====================================================
// HOME SCREEN
// ====================================================

void drawHomeScreen() {

    tft.fillScreen(
        TFT_BLACK
    );


    drawHeader(
        "REMOTE ID SENSOR"
    );


    tft.setTextColor(
        TFT_CYAN,
        TFT_BLACK
    );


    tft.drawCentreString(
        "CYD1 HANDHELD",
        160,
        35,
        2
    );


    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );


    tft.drawCentreString(
        "Passive RID Detection",
        160,
        60,
        1
    );


    drawButton(
        buttonBegin
    );


    tft.setTextColor(
        TFT_GREEN,
        TFT_BLACK
    );


    tft.drawCentreString(
        "SYSTEM READY",
        160,
        170,
        2
    );


    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );


    tft.drawCentreString(
        "Touch BEGIN TEST",
        160,
        200,
        1
    );
}


// ====================================================
// PRIVACY SCREEN
// ====================================================

void drawPrivacyScreen() {

    tft.fillScreen(
        TFT_BLACK
    );


    drawHeader(
        "SELECT OUTPUT MODE"
    );


    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );


    tft.drawCentreString(
        "PRIVATE is default",
        160,
        35,
        1
    );


    drawButton(
        buttonPrivate,
        privateMode
    );


    drawButton(
        buttonOpen,
        !privateMode
    );


    drawButton(
        buttonStartScan
    );


    drawButton(
        buttonBackHome
    );
}


// ====================================================
// LIVE SCREEN
// ====================================================

void drawLiveScreen() {

    tft.fillScreen(
        TFT_BLACK
    );

    drawHeader(
        "REMOTE ID - LIVE"
    );


    // --------------------------------------------------------
    // MODE / SCAN STATUS
    // --------------------------------------------------------

    if (privateMode) {

        tft.setTextColor(
            TFT_CYAN,
            TFT_BLACK
        );

        tft.drawString(
            "PRIVATE",
            10,
            26,
            1
        );
    }

    else {

        tft.setTextColor(
            TFT_YELLOW,
            TFT_BLACK
        );

        tft.drawString(
            "OPEN",
            10,
            26,
            1
        );
    }

    tft.setTextColor(
        TFT_GREEN,
        TFT_BLACK
    );

    tft.drawRightString(
        "SCANNING",
        310,
        26,
        1
    );


    // --------------------------------------------------------
    // TARGET SELECTOR
    // --------------------------------------------------------

    drawTargetTab(
        buttonTarget1,
        0
    );

    drawTargetTab(
        buttonTarget2,
        1
    );

    drawTargetTab(
        buttonTarget3,
        2
    );

    drawTargetTab(
        buttonTarget4,
        3
    );


    // --------------------------------------------------------
    // SELECTED TARGET PANEL
    // --------------------------------------------------------

    uint8_t i =
        selectedTarget;

    String targetName =
        String("T0")
        + String(i + 1);

    tft.setTextColor(
        mockTargetStateColor(i),
        TFT_BLACK
    );

    String line1 =
        targetName
        + "  "
        + mockTargetState[i]
        + "  "
        + mockTargetType[i];

    tft.drawString(
        line1,
        10,
        80,
        1
    );


    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );

    if (i < 3) {

        String line2 =
            String("RSSI: ")
            + String(mockRSSI[i])
            + " dBm   "
            + mockFlightState[i];

        tft.drawString(
            line2,
            10,
            96,
            1
        );


        String line3 =
            String("POS: VALID   HDG: ")
            + String(mockHeading[i]);

        tft.drawString(
            line3,
            10,
            112,
            1
        );


        String line4 =
            String("HSPD: ")
            + String(mockHSpeed[i], 1)
            + "   VSPD: "
            + String(mockVSpeed[i], 1);

        tft.drawString(
            line4,
            10,
            128,
            1
        );


        String line5;

        if (privateMode) {

            line5 =
                "ID: [REDACTED]";
        }

        else {

            line5 =
                String("ID: ")
                + mockID[i];
        }

        tft.drawString(
            line5,
            10,
            144,
            1
        );
    }

    else {

        tft.drawString(
            "NO ALLOCATED TARGET DATA",
            10,
            104,
            1
        );

        tft.drawString(
            "SELECT AN ACTIVE TARGET",
            10,
            122,
            1
        );
    }


    // --------------------------------------------------------
    // CONTROLS
    // --------------------------------------------------------

    drawButton(
        buttonSnapshot
    );

    drawButton(
        buttonRescan
    );

    UIButton modeButton =
        buttonMode;

    modeButton.label =
        privateMode
        ? "SWITCH OPEN"
        : "SWITCH PRIVATE";

    drawButton(
        modeButton
    );

    drawButton(
        buttonStop
    );
}


// SNAPSHOT SCREEN
// ====================================================

void drawSnapshotScreen() {

    tft.fillScreen(
        TFT_BLACK
    );

    drawHeader(
        "TARGET SNAPSHOT"
    );

    uint8_t i =
        selectedTarget;

    String targetName =
        String("T0")
        + String(i + 1);

    tft.setTextColor(
        mockTargetStateColor(i),
        TFT_BLACK
    );

    String titleLine =
        targetName
        + "  "
        + mockTargetState[i];

    tft.drawString(
        titleLine,
        12,
        35,
        2
    );


    tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
    );

    if (i < 3) {

        tft.drawString(
            String("TYPE: ")
            + mockTargetType[i],
            12,
            58,
            1
        );

        tft.drawString(
            String("FLIGHT: ")
            + mockFlightState[i],
            12,
            74,
            1
        );

        tft.drawString(
            String("RSSI: ")
            + String(mockRSSI[i])
            + " dBm",
            12,
            90,
            1
        );

        tft.drawString(
            String("HDG: ")
            + String(mockHeading[i]),
            12,
            106,
            1
        );

        tft.drawString(
            String("HSPD: ")
            + String(mockHSpeed[i], 1)
            + "   VSPD: "
            + String(mockVSpeed[i], 1),
            12,
            122,
            1
        );


        if (privateMode) {

            tft.drawString(
                "ID: [REDACTED]",
                12,
                138,
                1
            );

            tft.drawString(
                "POS: VALID [REDACTED]",
                12,
                154,
                1
            );
        }

        else {

            tft.drawString(
                String("ID: ")
                + mockID[i],
                12,
                138,
                1
            );

            String positionLine =
                String("POS: ")
                + String(mockLatitude[i], 7)
                + ", "
                + String(mockLongitude[i], 7);

            tft.drawString(
                positionLine,
                12,
                154,
                1
            );
        }
    }

    else {

        tft.drawString(
            "NO ALLOCATED TARGET DATA",
            12,
            90,
            1
        );
    }


    tft.setTextColor(
        TFT_CYAN,
        TFT_BLACK
    );

    tft.drawString(
        "STAGE 4B-A MOCK DATA",
        12,
        176,
        1
    );

    drawButton(
        buttonBackLive
    );
}


void drawCurrentScreen() {

    switch (
        currentScreen
    ) {

        case SCREEN_HOME:

            drawHomeScreen();

            break;


        case SCREEN_PRIVACY:

            drawPrivacyScreen();

            break;


        case SCREEN_LIVE:

            drawLiveScreen();

            break;


        case SCREEN_SNAPSHOT:

            drawSnapshotScreen();

            break;
    }
}


// ====================================================
// HIT TEST CURRENT SCREEN
// ====================================================

ButtonID hitTestCurrentScreen(
    double x,
    double y
) {

    switch (
        currentScreen
    ) {

        case SCREEN_HOME:

            if (
                pointInside(
                    x,
                    y,
                    buttonBegin
                )
            ) {

                return BTN_BEGIN;
            }

            break;


        case SCREEN_PRIVACY:

            if (
                pointInside(
                    x,
                    y,
                    buttonPrivate
                )
            ) {

                return BTN_PRIVATE;
            }


            if (
                pointInside(
                    x,
                    y,
                    buttonOpen
                )
            ) {

                return BTN_OPEN;
            }


            if (
                pointInside(
                    x,
                    y,
                    buttonStartScan
                )
            ) {

                return BTN_START_SCAN;
            }


            if (
                pointInside(
                    x,
                    y,
                    buttonBackHome
                )
            ) {

                return BTN_BACK_HOME;
            }

            break;


        case SCREEN_LIVE:

            if (
                pointInside(
                    x,
                    y,
                    buttonTarget1
                )
            ) {
                return BTN_TARGET_1;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonTarget2
                )
            ) {
                return BTN_TARGET_2;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonTarget3
                )
            ) {
                return BTN_TARGET_3;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonTarget4
                )
            ) {
                return BTN_TARGET_4;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonSnapshot
                )
            ) {
                return BTN_SNAPSHOT;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonRescan
                )
            ) {
                return BTN_RESCAN;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonMode
                )
            ) {
                return BTN_MODE_TOGGLE;
            }

            if (
                pointInside(
                    x,
                    y,
                    buttonStop
                )
            ) {
                return BTN_STOP;
            }

            break;


        case SCREEN_SNAPSHOT:

            if (
                pointInside(
                    x,
                    y,
                    buttonBackLive
                )
            ) {

                return BTN_BACK_LIVE;
            }

            break;
    }


    return BTN_NONE;
}


// ====================================================
// SERIAL EVENT LOG
// ====================================================

void logButtonEvent(
    ButtonID button,
    double x,
    double y
) {

    tapCounter++;


    Serial.println();

    Serial.println(
        "----------------------------------------"
    );


    Serial.print(
        "UI TAP #"
    );

    Serial.println(
        tapCounter
    );


    Serial.print(
        "Mapped X: "
    );

    Serial.println(
        x,
        2
    );


    Serial.print(
        "Mapped Y: "
    );

    Serial.println(
        y,
        2
    );


    Serial.print(
        "Button ID: "
    );

    Serial.println(
        static_cast<int>(
            button
        )
    );


    Serial.print(
        "Private Mode: "
    );

    Serial.println(
        privateMode
        ?
        "YES"
        :
        "NO"
    );


    Serial.print(
        "Scan Generation: "
    );

    Serial.println(
        scanGeneration
    );


    Serial.println(
        "----------------------------------------"
    );
}


// ====================================================
// PROCESS BUTTON ACTION
// ====================================================

void processButton(
    ButtonID button
) {

    switch (
        button
    ) {

        case BTN_BEGIN:

            selectedTarget = 0;

            // Every NEW test starts PRIVATE.

            privateMode =
                true;

            currentScreen =
                SCREEN_PRIVACY;

            Serial.println(
                "ACTION: BEGIN TEST"
            );

            break;


        case BTN_PRIVATE:

            privateMode =
                true;

            Serial.println(
                "ACTION: PRIVATE"
            );

            break;


        case BTN_OPEN:

            privateMode =
                false;

            Serial.println(
                "ACTION: OPEN"
            );

            break;


        case BTN_START_SCAN:

            scanGeneration++;

            currentScreen =
                SCREEN_LIVE;

            Serial.println(
                "ACTION: START SCAN"
            );

            break;


        case BTN_BACK_HOME:

            currentScreen =
                SCREEN_HOME;

            Serial.println(
                "ACTION: BACK HOME"
            );

            break;


        case BTN_SNAPSHOT:

            snapshotCount++;

            currentScreen =
                SCREEN_SNAPSHOT;

            Serial.println(
                "ACTION: SNAPSHOT"
            );

            break;


        case BTN_RESCAN:

            scanGeneration++;

            Serial.println(
                "ACTION: RESCAN"
            );

            break;


        case BTN_MODE_TOGGLE:

            privateMode =
                !privateMode;

            Serial.println(
                "ACTION: MODE TOGGLE"
            );

            break;


        case BTN_TARGET_1:

            selectedTarget = 0;

            Serial.println(
                "ACTION: SELECT T01"
            );

            break;


        case BTN_TARGET_2:

            selectedTarget = 1;

            Serial.println(
                "ACTION: SELECT T02"
            );

            break;


        case BTN_TARGET_3:

            selectedTarget = 2;

            Serial.println(
                "ACTION: SELECT T03"
            );

            break;


        case BTN_TARGET_4:

            selectedTarget = 3;

            Serial.println(
                "ACTION: SELECT T04"
            );

            break;


        case BTN_STOP:

            currentScreen =
                SCREEN_HOME;

            Serial.println(
                "ACTION: STOP / HOME"
            );

            break;


        case BTN_BACK_LIVE:

            currentScreen =
                SCREEN_LIVE;

            Serial.println(
                "ACTION: BACK TO LIVE"
            );

            break;


        default:

            Serial.println(
                "ACTION: NONE"
            );

            break;
    }


    drawCurrentScreen();
}


// ====================================================
// SETUP
// ====================================================

void setup() {

    Serial.begin(
        115200
    );


    delay(
        1000
    );


    touchSPI.begin(
        XPT2046_CLK,
        XPT2046_MISO,
        XPT2046_MOSI,
        XPT2046_CS
    );


    ts.begin(
        touchSPI
    );


    ts.setRotation(
        1
    );


    tft.init();


    tft.setRotation(
        1
    );


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        " CYD1 UI STAGE 4A"
    );

    Serial.println(
        " HANDHELD NAVIGATION"
    );

    Serial.println(
        "========================================"
    );

    Serial.println(
        "RID backend NOT integrated."
    );

    Serial.println(
        "Synthetic target data active."
    );

    Serial.println(
        "Default mode: PRIVATE."
    );

    Serial.println();


    drawCurrentScreen();
}


// ====================================================
// LOOP
// ====================================================

void loop() {

    int rawX = 0;
    int rawY = 0;
    int rawZ = 0;


    if (
        !captureFilteredTap(
            rawX,
            rawY,
            rawZ
        )
    ) {

        delay(15);

        return;
    }


    double screenX = 0.0;
    double screenY = 0.0;


    if (
        !mapTouchToScreen(
            rawX,
            rawY,
            screenX,
            screenY
        )
    ) {

        Serial.println(
            "ERROR: TOUCH MAP"
        );

        return;
    }


    ButtonID button =
        hitTestCurrentScreen(
            screenX,
            screenY
        );


    logButtonEvent(
        button,
        screenX,
        screenY
    );


    processButton(
        button
    );
}
