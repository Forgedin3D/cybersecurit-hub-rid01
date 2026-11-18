#include "esp_system.h"
#include "esp_attr.h"
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "TCA9554.h"
#include "TouchDrvFT6X36.hpp"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
// CYD1 display/touch dependencies intentionally disabled
// for ESP32-S3 backend-only parity validation.
// #include <SPI.h>
// #include <XPT2046_Touchscreen.h>
// #include <TFT_eSPI.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


// ====================================================
// CYD1 REMOTE ID MULTI-TARGET ENGINE
// STAGE 7C
// ====================================================

BLEScan *pBLEScan;

const uint32_t STARTUP_DELAY_MS = 1000;
const uint32_t SCAN_WINDOW_SECONDS = 60;

const uint32_t DASHBOARD_INTERVAL_MS = 1000;

const uint8_t MAX_TARGETS = 4;

const uint32_t TARGET_ACTIVE_MS = 3000;
const uint32_t TARGET_STALE_MS = 10000;
const uint32_t TARGET_REUSE_MS = 15000;


// ====================================================
// OUTPUT MODE
// ====================================================

enum OutputMode {
    MODE_PRIVATE,
    MODE_OPEN
};

OutputMode outputMode = MODE_PRIVATE;


// ====================================================
// GLOBAL STATISTICS
// ====================================================

volatile uint32_t totalAdvertisements = 0;
volatile uint32_t remoteIDPackets = 0;

uint32_t messageTypeCounts[16] = {0};

uint32_t lastDashboardPrintMs = 0;

uint32_t targetAllocationFailures = 0;

// D4H cold-boot acknowledgment.
bool ridBootAckAccepted = false;

// D4H-B: Retain accepted state across supported warm resets.
// Never persist acceptance in flash.
static constexpr uint32_t RID_ACK_MAGIC = 0x52494431u;
RTC_NOINIT_ATTR uint32_t ridAckRtcMagic;
RTC_NOINIT_ATTR uint32_t ridAckRtcCheck;


// ============================================================
// ASYNCHRONOUS SCAN CONTROLLER
// Stage 4B-B
// ============================================================

volatile bool scanCompletePending = false;

bool scanSessionActive = false;
bool scanStopRequested = false;

uint32_t scanStartedMs = 0;
uint32_t scanWindowCount = 0;



// ====================================================
// TARGET STRUCTURE
// ====================================================

struct RIDTarget {

    bool allocated = false;

    String sourceAddress = "";


    // --------------------------------
    // Identity
    // --------------------------------

    bool hasSerialID = false;
    bool hasRegistrationID = false;
    bool hasOperatorID = false;

    String serialID = "";
    String registrationID = "";
    String operatorID = "";

    uint8_t uaType = 0;
    uint8_t protocolVersion = 0;


    // --------------------------------
    // Position / telemetry
    // --------------------------------

    bool hasPosition = false;

    uint8_t flightStatus = 0;

    double latitude = 0.0;
    double longitude = 0.0;

    bool directionValid = false;
    float direction = 0.0f;

    bool horizontalSpeedValid = false;
    float horizontalSpeed = 0.0f;

    bool verticalSpeedValid = false;
    float verticalSpeed = 0.0f;

    bool barometricAltitudeValid = false;
    float barometricAltitude = 0.0f;

    bool geodeticAltitudeValid = false;
    float geodeticAltitude = 0.0f;

    bool heightValid = false;
    float height = 0.0f;

    bool heightFromGround = false;


    // --------------------------------
    // Accuracy
    // --------------------------------

    uint8_t horizontalAccuracy = 0;
    uint8_t verticalAccuracy = 0;
    uint8_t barometricAccuracy = 0;
    uint8_t speedAccuracy = 0;
    uint8_t timestampAccuracy = 0;


    // --------------------------------
    // RID timestamp
    // --------------------------------

    float remoteIDTimestamp = 0.0f;

    // --------------------------------
    // System message
    // --------------------------------

    bool hasSystemData = false;

    uint8_t operatorLocationType = 0;
    uint8_t classificationType = 0;

    bool hasSystemOperatorPosition = false;

    double systemOperatorLatitude = 0.0;
    double systemOperatorLongitude = 0.0;

    uint16_t areaCount = 0;
    uint16_t areaRadius = 0;

    bool areaCeilingValid = false;
    float areaCeiling = 0.0f;

    bool areaFloorValid = false;
    float areaFloor = 0.0f;

    uint8_t categoryEU = 0;
    uint8_t classEU = 0;

    bool operatorGeoAltitudeValid = false;
    float operatorGeoAltitude = 0.0f;

    uint32_t systemTimestamp = 0;


    // --------------------------------
    // RSSI filter
    // --------------------------------

    int currentRSSI = 0;

    int rssiSamples[5] = {0};

    uint8_t rssiCount = 0;
    uint8_t rssiIndex = 0;


    // --------------------------------
    // Packet counters
    // --------------------------------

    uint32_t packetCount = 0;

    uint32_t basicIDCount = 0;
    uint32_t locationCount = 0;
    uint32_t systemCount = 0;
    uint32_t operatorIDCount = 0;


    // --------------------------------
    // Receiver timing
    // --------------------------------

    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs = 0;

    // Updated ONLY when a Location/Vector packet supplies
    // a nonzero geographic position. General RID traffic
    // must not keep geographic data artificially fresh.
    uint32_t lastPositionSeenMs = 0;
};


RIDTarget targets[MAX_TARGETS];

// Protects targets[] now that BLE callbacks and the UI run
// concurrently on the ESP32.
SemaphoreHandle_t targetMutex = nullptr;


// ====================================================
// BYTE HELPERS
// ====================================================

uint8_t getByte(
    const String &data,
    size_t index
) {

    return static_cast<uint8_t>(
        data[index]
    );
}


uint16_t readU16LE(
    const String &data,
    size_t index
) {

    uint16_t value = 0;

    value |=
        static_cast<uint16_t>(
            getByte(data, index)
        );

    value |=
        static_cast<uint16_t>(
            getByte(data, index + 1)
        ) << 8;

    return value;
}


int32_t readI32LE(
    const String &data,
    size_t index
) {

    uint32_t value = 0;

    value |=
        static_cast<uint32_t>(
            getByte(data, index)
        );

    value |=
        static_cast<uint32_t>(
            getByte(data, index + 1)
        ) << 8;

    value |=
        static_cast<uint32_t>(
            getByte(data, index + 2)
        ) << 16;

    value |=
        static_cast<uint32_t>(
            getByte(data, index + 3)
        ) << 24;

    return static_cast<int32_t>(
        value
    );
}


String readTextField(
    const String &data,
    size_t startIndex,
    size_t length
) {

    String text = "";

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        char c =
            data[startIndex + i];

        if (c == '\0') {
            break;
        }

        if (
            c >= 32
            &&
            c <= 126
        ) {

            text += c;
        }
    }

    return text;
}


// ====================================================
// MESSAGE / TARGET NAMES
// ====================================================

const char *messageTypeName(
    uint8_t messageType
) {

    switch (messageType) {

        case 0:
            return "BASIC ID";

        case 1:
            return "LOCATION / VECTOR";

        case 2:
            return "AUTHENTICATION";

        case 3:
            return "SELF ID";

        case 4:
            return "SYSTEM";

        case 5:
            return "OPERATOR ID";

        case 15:
            return "MESSAGE PACK";

        default:
            return "UNKNOWN / RESERVED";
    }
}


const char *uaTypeName(
    uint8_t uaType
) {

    switch (uaType) {

        case 0:
            return "NONE";

        case 1:
            return "AEROPLANE";

        case 2:
            return "HELICOPTER / MULTIROTOR";

        case 3:
            return "GYROPLANE";

        case 4:
            return "HYBRID LIFT";

        case 5:
            return "ORNITHOPTER";

        case 6:
            return "GLIDER";

        case 7:
            return "KITE";

        case 8:
            return "FREE BALLOON";

        case 9:
            return "CAPTIVE BALLOON";

        case 10:
            return "AIRSHIP";

        case 11:
            return "FREE FALL / PARACHUTE";

        case 12:
            return "ROCKET";

        case 13:
            return "TETHERED POWERED AIRCRAFT";

        case 14:
            return "GROUND OBSTACLE";

        case 15:
            return "OTHER";

        default:
            return "UNKNOWN";
    }
}


const char *flightStatusName(
    uint8_t status
) {

    switch (status) {

        case 0:
            return "UNDECLARED";

        case 1:
            return "GROUND";

        case 2:
            return "AIRBORNE";

        case 3:
            return "EMERGENCY";

        default:
            return "UNKNOWN";
    }
}


// ====================================================
// SYSTEM MESSAGE NAMES
// ====================================================

const char *operatorLocationTypeName(
    uint8_t type
) {

    switch (type) {

        case 0:
            return "TAKEOFF";

        case 1:
            return "LIVE GNSS";

        case 2:
            return "FIXED";

        default:
            return "RESERVED / UNKNOWN";
    }
}


const char *classificationTypeName(
    uint8_t type
) {

    switch (type) {

        case 0:
            return "UNDECLARED";

        case 1:
            return "EU";

        default:
            return "RESERVED / UNKNOWN";
    }
}


const char *categoryEUName(
    uint8_t category
) {

    switch (category) {

        case 0:
            return "UNDECLARED";

        case 1:
            return "OPEN";

        case 2:
            return "SPECIFIC";

        case 3:
            return "CERTIFIED";

        default:
            return "RESERVED / UNKNOWN";
    }
}


const char *classEUName(
    uint8_t value
) {

    switch (value) {

        case 0:
            return "UNDECLARED";

        case 1:
            return "CLASS 0";

        case 2:
            return "CLASS 1";

        case 3:
            return "CLASS 2";

        case 4:
            return "CLASS 3";

        case 5:
            return "CLASS 4";

        case 6:
            return "CLASS 5";

        case 7:
            return "CLASS 6";

        default:
            return "RESERVED / UNKNOWN";
    }
}


// ====================================================
// TARGET LIFECYCLE
// ====================================================

const char *targetStateName(
    const RIDTarget &target
) {

    if (!target.allocated) {
        return "EMPTY";
    }


    uint32_t age =
        millis()
        -
        target.lastSeenMs;


    if (
        age <= TARGET_ACTIVE_MS
    ) {

        return "ACTIVE";
    }


    if (
        age <= TARGET_STALE_MS
    ) {

        return "STALE";
    }


    return "LOST";
}


// ====================================================
// RSSI FILTER
// ====================================================

void addRSSISample(
    RIDTarget &target,
    int rssi
) {

    target.currentRSSI =
        rssi;


    target.rssiSamples[
        target.rssiIndex
    ] = rssi;


    target.rssiIndex++;


    if (
        target.rssiIndex >= 5
    ) {

        target.rssiIndex = 0;
    }


    if (
        target.rssiCount < 5
    ) {

        target.rssiCount++;
    }
}


int getFilteredRSSI(
    RIDTarget &target
) {

    if (
        target.rssiCount == 0
    ) {

        return 0;
    }


    int sorted[5];


    for (
        uint8_t i = 0;
        i < target.rssiCount;
        i++
    ) {

        sorted[i] =
            target.rssiSamples[i];
    }


    for (
        uint8_t i = 1;
        i < target.rssiCount;
        i++
    ) {

        int key =
            sorted[i];

        int j =
            i - 1;


        while (
            j >= 0
            &&
            sorted[j] > key
        ) {

            sorted[j + 1] =
                sorted[j];

            j--;
        }


        sorted[j + 1] =
            key;
    }


    if (
        target.rssiCount % 2 == 1
    ) {

        return
            sorted[
                target.rssiCount / 2
            ];
    }


    int upper =
        sorted[
            target.rssiCount / 2
        ];


    int lower =
        sorted[
            (target.rssiCount / 2) - 1
        ];


    return
        (lower + upper)
        / 2;
}


// ====================================================
// TARGET LOOKUP / ALLOCATION
// ====================================================

int findTargetByAddress(
    const String &address
) {

    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        if (
            targets[i].allocated
            &&
            targets[i].sourceAddress
                .equalsIgnoreCase(address)
        ) {

            return i;
        }
    }


    return -1;
}


int allocateTarget(
    const String &address
) {

    // --------------------------------
    // First try an unused slot.
    // --------------------------------

    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        if (
            !targets[i].allocated
        ) {

            targets[i] =
                RIDTarget();


            targets[i].allocated =
                true;


            targets[i].sourceAddress =
                address;


            targets[i].firstSeenMs =
                millis();


            targets[i].lastSeenMs =
                millis();


            return i;
        }
    }


    // --------------------------------
    // Otherwise reuse a sufficiently
    // old LOST target.
    // --------------------------------

    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        uint32_t age =
            millis()
            -
            targets[i].lastSeenMs;


        if (
            age > TARGET_REUSE_MS
        ) {

            targets[i] =
                RIDTarget();


            targets[i].allocated =
                true;


            targets[i].sourceAddress =
                address;


            targets[i].firstSeenMs =
                millis();


            targets[i].lastSeenMs =
                millis();


            return i;
        }
    }


    targetAllocationFailures++;

    return -1;
}


int findOrCreateTarget(
    const String &address
) {

    int targetIndex =
        findTargetByAddress(
            address
        );


    if (
        targetIndex >= 0
    ) {

        return targetIndex;
    }


    return
        allocateTarget(
            address
        );
}


// ====================================================
// TELEMETRY DECODERS
// ====================================================

float decodeAltitude(
    uint16_t encodedAltitude
) {

    return
        (
            static_cast<float>(
                encodedAltitude
            )
            * 0.5f
        )
        - 1000.0f;
}


float decodeHorizontalSpeed(
    uint8_t encodedSpeed,
    bool speedMultiplier
) {

    if (
        speedMultiplier
    ) {

        return
            (
                static_cast<float>(
                    encodedSpeed
                )
                * 0.75f
            )
            + 63.75f;
    }


    return
        static_cast<float>(
            encodedSpeed
        )
        * 0.25f;
}


float decodeVerticalSpeed(
    int8_t encodedSpeed
) {

    return
        static_cast<float>(
            encodedSpeed
        )
        * 0.5f;
}


// ====================================================
// BASIC ID UPDATE
// ====================================================

void updateBasicID(
    RIDTarget &target,
    const String &serviceData
) {

    target.basicIDCount++;


    uint8_t idAndUAType =
        getByte(
            serviceData,
            3
        );


    uint8_t idType =
        (
            idAndUAType >> 4
        )
        & 0x0F;


    uint8_t uaType =
        idAndUAType
        & 0x0F;


    String uasID =
        readTextField(
            serviceData,
            4,
            20
        );


    target.uaType =
        uaType;


    if (
        idType == 1
        &&
        uasID.length() > 0
    ) {

        target.serialID =
            uasID;

        target.hasSerialID =
            true;
    }


    else if (
        idType == 2
        &&
        uasID.length() > 0
    ) {

        target.registrationID =
            uasID;

        target.hasRegistrationID =
            true;
    }
}


// ====================================================
// LOCATION UPDATE
// ====================================================

void updateLocation(
    RIDTarget &target,
    const String &serviceData
) {

    target.locationCount++;


    uint8_t statusFlags =
        getByte(
            serviceData,
            3
        );


    bool speedMultiplier =
        statusFlags
        & 0x01;


    bool eastWestDirection =
        (
            statusFlags >> 1
        )
        & 0x01;


    bool heightType =
        (
            statusFlags >> 2
        )
        & 0x01;


    target.flightStatus =
        (
            statusFlags >> 4
        )
        & 0x0F;


    uint8_t directionEncoded =
        getByte(
            serviceData,
            4
        );


    float direction =
        static_cast<float>(
            directionEncoded
        );


    if (
        eastWestDirection
    ) {

        direction += 180.0f;
    }


    if (
        direction <= 360.0f
    ) {

        target.direction =
            direction;

        target.directionValid =
            true;
    }

    else {

        target.directionValid =
            false;
    }


    uint8_t horizontalSpeedEncoded =
        getByte(
            serviceData,
            5
        );


    float horizontalSpeed =
        decodeHorizontalSpeed(
            horizontalSpeedEncoded,
            speedMultiplier
        );


    if (
        horizontalSpeed < 255.0f
    ) {

        target.horizontalSpeed =
            horizontalSpeed;

        target.horizontalSpeedValid =
            true;
    }

    else {

        target.horizontalSpeedValid =
            false;
    }


    int8_t verticalSpeedEncoded =
        static_cast<int8_t>(
            getByte(
                serviceData,
                6
            )
        );


    float verticalSpeed =
        decodeVerticalSpeed(
            verticalSpeedEncoded
        );


    if (
        verticalSpeed != 63.0f
    ) {

        target.verticalSpeed =
            verticalSpeed;

        target.verticalSpeedValid =
            true;
    }

    else {

        target.verticalSpeedValid =
            false;
    }


    int32_t latitudeRaw =
        readI32LE(
            serviceData,
            7
        );


    int32_t longitudeRaw =
        readI32LE(
            serviceData,
            11
        );


    target.latitude =
        static_cast<double>(
            latitudeRaw
        )
        / 10000000.0;


    target.longitude =
        static_cast<double>(
            longitudeRaw
        )
        / 10000000.0;


    target.hasPosition =
        (
            latitudeRaw != 0
            ||
            longitudeRaw != 0
        );


    if (target.hasPosition) {

        target.lastPositionSeenMs =
            millis();
    }


    uint16_t baroRaw =
        readU16LE(
            serviceData,
            15
        );


    uint16_t geoRaw =
        readU16LE(
            serviceData,
            17
        );


    uint16_t heightRaw =
        readU16LE(
            serviceData,
            19
        );


    target.barometricAltitudeValid =
        baroRaw != 0;


    target.geodeticAltitudeValid =
        geoRaw != 0;


    target.heightValid =
        heightRaw != 0;


    if (
        target.barometricAltitudeValid
    ) {

        target.barometricAltitude =
            decodeAltitude(
                baroRaw
            );
    }


    if (
        target.geodeticAltitudeValid
    ) {

        target.geodeticAltitude =
            decodeAltitude(
                geoRaw
            );
    }


    if (
        target.heightValid
    ) {

        target.height =
            decodeAltitude(
                heightRaw
            );
    }


    target.heightFromGround =
        heightType;


    uint8_t accuracyByte1 =
        getByte(
            serviceData,
            21
        );


    target.horizontalAccuracy =
        accuracyByte1
        & 0x0F;


    target.verticalAccuracy =
        (
            accuracyByte1 >> 4
        )
        & 0x0F;


    uint8_t accuracyByte2 =
        getByte(
            serviceData,
            22
        );


    target.speedAccuracy =
        accuracyByte2
        & 0x0F;


    target.barometricAccuracy =
        (
            accuracyByte2 >> 4
        )
        & 0x0F;


    uint16_t timestampRaw =
        readU16LE(
            serviceData,
            23
        );


    if (
        timestampRaw
        !=
        0xFFFF
    ) {

        target.remoteIDTimestamp =
            static_cast<float>(
                timestampRaw
            )
            / 10.0f;
    }


    target.timestampAccuracy =
        getByte(
            serviceData,
            25
        )
        & 0x0F;
}


// ====================================================
// SYSTEM UPDATE
// ====================================================

void updateSystem(
    RIDTarget &target,
    const String &serviceData
) {

    target.systemCount++;

    target.hasSystemData =
        true;


    // --------------------------------
    // Operator location /
    // classification flags
    // --------------------------------

    uint8_t flags =
        getByte(
            serviceData,
            3
        );


    target.operatorLocationType =
        flags
        &
        0x03;


    target.classificationType =
        (
            flags >> 2
        )
        &
        0x07;


    // --------------------------------
    // Operator position
    // --------------------------------

    int32_t operatorLatitudeRaw =
        readI32LE(
            serviceData,
            4
        );


    int32_t operatorLongitudeRaw =
        readI32LE(
            serviceData,
            8
        );


    if (
        operatorLatitudeRaw != 0
        ||
        operatorLongitudeRaw != 0
    ) {

        target.systemOperatorLatitude =
            static_cast<double>(
                operatorLatitudeRaw
            )
            /
            10000000.0;


        target.systemOperatorLongitude =
            static_cast<double>(
                operatorLongitudeRaw
            )
            /
            10000000.0;


        target.hasSystemOperatorPosition =
            true;
    }

    else {

        target.systemOperatorLatitude =
            0.0;

        target.systemOperatorLongitude =
            0.0;

        target.hasSystemOperatorPosition =
            false;
    }


    // --------------------------------
    // Area data
    // --------------------------------

    target.areaCount =
        readU16LE(
            serviceData,
            12
        );


    target.areaRadius =
        static_cast<uint16_t>(
            getByte(
                serviceData,
                14
            )
        )
        *
        10U;


    // --------------------------------
    // Area ceiling
    // --------------------------------

    uint16_t areaCeilingRaw =
        readU16LE(
            serviceData,
            15
        );


    if (
        areaCeilingRaw
        !=
        0
    ) {

        target.areaCeiling =
            decodeAltitude(
                areaCeilingRaw
            );

        target.areaCeilingValid =
            true;
    }

    else {

        target.areaCeilingValid =
            false;
    }


    // --------------------------------
    // Area floor
    // --------------------------------

    uint16_t areaFloorRaw =
        readU16LE(
            serviceData,
            17
        );


    if (
        areaFloorRaw
        !=
        0
    ) {

        target.areaFloor =
            decodeAltitude(
                areaFloorRaw
            );

        target.areaFloorValid =
            true;
    }

    else {

        target.areaFloorValid =
            false;
    }


    // --------------------------------
    // EU category / class
    // --------------------------------

    uint8_t classificationByte =
        getByte(
            serviceData,
            19
        );


    target.classEU =
        classificationByte
        &
        0x0F;


    target.categoryEU =
        (
            classificationByte >> 4
        )
        &
        0x0F;


    // --------------------------------
    // Operator geodetic altitude
    // --------------------------------

    uint16_t operatorAltitudeRaw =
        readU16LE(
            serviceData,
            20
        );


    if (
        operatorAltitudeRaw
        !=
        0
    ) {

        target.operatorGeoAltitude =
            decodeAltitude(
                operatorAltitudeRaw
            );

        target.operatorGeoAltitudeValid =
            true;
    }

    else {

        target.operatorGeoAltitudeValid =
            false;
    }


    // --------------------------------
    // System timestamp
    //
    // uint32 little-endian,
    // seconds since 2019-01-01 UTC.
    // --------------------------------

    target.systemTimestamp =
        static_cast<uint32_t>(
            getByte(
                serviceData,
                22
            )
        )
        |
        (
            static_cast<uint32_t>(
                getByte(
                    serviceData,
                    23
                )
            )
            << 8
        )
        |
        (
            static_cast<uint32_t>(
                getByte(
                    serviceData,
                    24
                )
            )
            << 16
        )
        |
        (
            static_cast<uint32_t>(
                getByte(
                    serviceData,
                    25
                )
            )
            << 24
        );
}


// ====================================================
// OPERATOR ID UPDATE
// ====================================================

void updateOperatorID(
    RIDTarget &target,
    const String &serviceData
) {

    target.operatorIDCount++;


    String operatorID =
        readTextField(
            serviceData,
            4,
            20
        );


    if (
        operatorID.length() > 0
    ) {

        target.operatorID =
            operatorID;

        target.hasOperatorID =
            true;
    }

    else {

        target.operatorID =
            "";

        target.hasOperatorID =
            false;
    }
}


// ====================================================
// PRIVACY-AWARE OUTPUT
// ====================================================

void printProtectedID(
    const char *label,
    const String &value,
    bool present
) {

    Serial.print(label);


    if (!present) {

        Serial.println(
            "<not provided>"
        );

        return;
    }


    if (
        outputMode
        ==
        MODE_PRIVATE
    ) {

        Serial.println(
            "[REDACTED]"
        );
    }

    else {

        Serial.println(
            value
        );
    }
}


// ====================================================
// TARGET COUNTS
// ====================================================

uint8_t getTrackedTargetCount() {

    uint8_t count = 0;


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        if (
            targets[i].allocated
        ) {

            count++;
        }
    }


    return count;
}


uint8_t getActiveTargetCount() {

    uint8_t count = 0;


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        if (
            !targets[i].allocated
        ) {

            continue;
        }


        uint32_t age =
            millis()
            -
            targets[i].lastSeenMs;


        if (
            age <= TARGET_ACTIVE_MS
        ) {

            count++;
        }
    }


    return count;
}


// ====================================================
// COMPACT MULTI-TARGET SUMMARY
// ====================================================

void printTargetSummary() {

    // --------------------------------------------------------
    // Copy only the compact dashboard fields while holding the
    // target mutex. Serial output happens AFTER releasing it.
    // --------------------------------------------------------

    struct DashboardTargetCopy {

        bool allocated = false;

        uint8_t stateCode = 0;

        uint8_t uaType = 0;
        uint8_t flightStatus = 0;

        bool hasPosition = false;

        int filteredRSSI = 0;

        uint32_t packetCount = 0;
        uint32_t lastSeenMs = 0;
    };


    DashboardTargetCopy copies[MAX_TARGETS];

    uint8_t trackedCount = 0;
    uint8_t activeCount = 0;

    uint32_t snapshotMs =
        millis();

    OutputMode summaryMode =
        outputMode;


    if (targetMutex != nullptr) {

        xSemaphoreTake(
            targetMutex,
            portMAX_DELAY
        );
    }


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        RIDTarget &target =
            targets[i];

        DashboardTargetCopy &copy =
            copies[i];


        copy.allocated =
            target.allocated;


        if (!target.allocated) {

            continue;
        }


        trackedCount++;


        uint32_t age =
            snapshotMs
            -
            target.lastSeenMs;


        if (
            age <= TARGET_ACTIVE_MS
        ) {

            copy.stateCode = 1;
            activeCount++;
        }

        else if (
            age <= TARGET_STALE_MS
        ) {

            copy.stateCode = 2;
        }

        else {

            copy.stateCode = 3;
        }


        copy.uaType =
            target.uaType;

        copy.flightStatus =
            target.flightStatus;

        copy.hasPosition =
            target.hasPosition;

        copy.filteredRSSI =
            getFilteredRSSI(
                target
            );

        copy.packetCount =
            target.packetCount;

        copy.lastSeenMs =
            target.lastSeenMs;
    }


    if (targetMutex != nullptr) {

        xSemaphoreGive(
            targetMutex
        );
    }


    // --------------------------------------------------------
    // Serial work is intentionally outside targetMutex.
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "========== RID TARGET SUMMARY =========="
    );


    Serial.print(
        "Output Mode: "
    );

    Serial.println(
        summaryMode == MODE_PRIVATE
        ? "PRIVATE"
        : "OPEN"
    );


    Serial.print(
        "Targets Tracked: "
    );

    Serial.println(
        trackedCount
    );


    Serial.print(
        "Targets Active: "
    );

    Serial.println(
        activeCount
    );


    Serial.println();


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        DashboardTargetCopy &copy =
            copies[i];


        if (!copy.allocated) {

            continue;
        }


        Serial.print(
            "T"
        );


        if (
            i + 1 < 10
        ) {

            Serial.print(
                "0"
            );
        }


        Serial.print(
            i + 1
        );


        Serial.print(
            "  "
        );


        switch (
            copy.stateCode
        ) {

            case 1:
                Serial.print(
                    "ACTIVE"
                );
                break;

            case 2:
                Serial.print(
                    "STALE"
                );
                break;

            case 3:
                Serial.print(
                    "LOST"
                );
                break;

            default:
                Serial.print(
                    "EMPTY"
                );
                break;
        }


        Serial.print(
            "  "
        );


        Serial.println(
            uaTypeName(
                copy.uaType
            )
        );


        Serial.print(
            "State: "
        );

        Serial.println(
            flightStatusName(
                copy.flightStatus
            )
        );


        Serial.print(
            "Position: "
        );

        Serial.println(
            copy.hasPosition
            ? "VALID"
            : "UNKNOWN"
        );


        Serial.print(
            "RSSI: "
        );

        Serial.print(
            copy.filteredRSSI
        );

        Serial.println(
            " dBm"
        );


        Serial.print(
            "Packets: "
        );

        Serial.println(
            copy.packetCount
        );


        Serial.print(
            "Last Seen: "
        );

        Serial.print(
            snapshotMs
            -
            copy.lastSeenMs
        );

        Serial.println(
            " ms ago"
        );


        Serial.println();
    }


    Serial.println(
        "========================================"
    );
}


// ============================================================
// MAIN-LOOP DASHBOARD SERVICE
//
// Heavy formatted Serial output must never run inside the BLE
// advertisement callback. RF processing owns the callback;
// diagnostics are serviced independently by the main loop.
// ============================================================

void serviceTargetDashboard() {

    if (
        !scanSessionActive
        || scanStopRequested
    ) {

        return;
    }


    uint32_t now =
        millis();


    if (
        now
        -
        lastDashboardPrintMs
        <
        DASHBOARD_INTERVAL_MS
    ) {

        return;
    }


    // Advance the timer before printing so diagnostic timing
    // cannot cause an immediate duplicate emission.
    lastDashboardPrintMs =
        now;


    printTargetSummary();
}


// ====================================================
// FULL TARGET SNAPSHOT
// ====================================================

void printTargetDetail(
    uint8_t index
) {

    RIDTarget &target =
        targets[index];


    if (
        !target.allocated
    ) {

        return;
    }


    Serial.println();

    Serial.println(
        "========================================"
    );


    Serial.print(
        "           RID TARGET "
    );


    if (
        index + 1 < 10
    ) {

        Serial.print(
            "0"
        );
    }


    Serial.println(
        index + 1
    );


    Serial.println(
        "========================================"
    );


    Serial.print(
        "Target State: "
    );

    Serial.println(
        targetStateName(
            target
        )
    );


    Serial.print(
        "Aircraft Type: "
    );

    Serial.println(
        uaTypeName(
            target.uaType
        )
    );


    printProtectedID(
        "Serial ID: ",
        target.serialID,
        target.hasSerialID
    );


    printProtectedID(
        "Registration ID: ",
        target.registrationID,
        target.hasRegistrationID
    );


    if (
        target.hasOperatorID
    ) {

        printProtectedID(
            "Operator ID: ",
            target.operatorID,
            true
        );
    }

    else {

        Serial.println(
            "Operator ID: <not configured>"
        );
    }


    if (
        outputMode == MODE_OPEN
    ) {

        Serial.print(
            "BLE Source: "
        );

        Serial.println(
            target.sourceAddress
        );
    }


    Serial.println();


    Serial.print(
        "Flight State: "
    );

    Serial.println(
        flightStatusName(
            target.flightStatus
        )
    );


    if (
        target.hasPosition
    ) {

        if (
            outputMode == MODE_PRIVATE
        ) {

            Serial.println(
                "Position: VALID [REDACTED]"
            );
        }

        else {

            Serial.print(
                "Latitude: "
            );

            Serial.println(
                target.latitude,
                7
            );


            Serial.print(
                "Longitude: "
            );

            Serial.println(
                target.longitude,
                7
            );
        }
    }

    else {

        Serial.println(
            "Position: UNKNOWN"
        );
    }


    Serial.print(
        "Heading: "
    );


    if (
        target.directionValid
    ) {

        Serial.print(
            target.direction,
            1
        );

        Serial.println(
            " deg"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.print(
        "Horizontal Speed: "
    );


    if (
        target.horizontalSpeedValid
    ) {

        Serial.print(
            target.horizontalSpeed,
            2
        );

        Serial.println(
            " m/s"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.print(
        "Vertical Speed: "
    );


    if (
        target.verticalSpeedValid
    ) {

        Serial.print(
            target.verticalSpeed,
            2
        );

        Serial.println(
            " m/s"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.println();


    Serial.print(
        "Barometric Altitude: "
    );


    if (
        target.barometricAltitudeValid
    ) {

        Serial.print(
            target.barometricAltitude,
            2
        );

        Serial.println(
            " m"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.print(
        "Geodetic Altitude: "
    );


    if (
        target.geodeticAltitudeValid
    ) {

        Serial.print(
            target.geodeticAltitude,
            2
        );

        Serial.println(
            " m"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.print(
        "Relative Height: "
    );


    if (
        target.heightValid
    ) {

        Serial.print(
            target.height,
            2
        );

        Serial.println(
            " m"
        );
    }

    else {

        Serial.println(
            "UNKNOWN"
        );
    }


    Serial.println();


    // --------------------------------
    // System message
    // --------------------------------

    if (
        target.hasSystemData
    ) {

        Serial.println();

        Serial.println(
            "System Data:"
        );


        Serial.print(
            "Operator Location Type: "
        );

        Serial.println(
            operatorLocationTypeName(
                target.operatorLocationType
            )
        );


        Serial.print(
            "Classification: "
        );

        Serial.println(
            classificationTypeName(
                target.classificationType
            )
        );


        if (
            target.hasSystemOperatorPosition
        ) {

            if (
                outputMode == MODE_PRIVATE
            ) {

                Serial.println(
                    "Operator Position: VALID [REDACTED]"
                );
            }

            else {

                Serial.print(
                    "Operator Latitude: "
                );

                Serial.println(
                    target.systemOperatorLatitude,
                    7
                );


                Serial.print(
                    "Operator Longitude: "
                );

                Serial.println(
                    target.systemOperatorLongitude,
                    7
                );
            }
        }

        else {

            Serial.println(
                "Operator Position: UNKNOWN"
            );
        }


        Serial.print(
            "Area Count: "
        );

        Serial.println(
            target.areaCount
        );


        Serial.print(
            "Area Radius: "
        );

        Serial.print(
            target.areaRadius
        );

        Serial.println(
            " m"
        );


        Serial.print(
            "Area Ceiling: "
        );

        if (
            target.areaCeilingValid
        ) {

            Serial.print(
                target.areaCeiling,
                2
            );

            Serial.println(
                " m"
            );
        }

        else {

            Serial.println(
                "UNKNOWN"
            );
        }


        Serial.print(
            "Area Floor: "
        );

        if (
            target.areaFloorValid
        ) {

            Serial.print(
                target.areaFloor,
                2
            );

            Serial.println(
                " m"
            );
        }

        else {

            Serial.println(
                "UNKNOWN"
            );
        }


        if (
            target.classificationType == 1
        ) {

            Serial.print(
                "EU Category: "
            );

            Serial.println(
                categoryEUName(
                    target.categoryEU
                )
            );


            Serial.print(
                "EU Class: "
            );

            Serial.println(
                classEUName(
                    target.classEU
                )
            );
        }


        Serial.print(
            "Operator Geodetic Altitude: "
        );

        if (
            target.operatorGeoAltitudeValid
        ) {

            Serial.print(
                target.operatorGeoAltitude,
                2
            );

            Serial.println(
                " m"
            );
        }

        else {

            Serial.println(
                "UNKNOWN"
            );
        }


        Serial.print(
            "System Timestamp: "
        );

        Serial.print(
            target.systemTimestamp
        );

        Serial.println(
            " s since 2019-01-01 UTC"
        );
    }


    Serial.println();


    Serial.print(
        "RSSI Current: "
    );

    Serial.print(
        target.currentRSSI
    );

    Serial.println(
        " dBm"
    );


    Serial.print(
        "RSSI Filtered: "
    );

    Serial.print(
        getFilteredRSSI(
            target
        )
    );

    Serial.println(
        " dBm"
    );


    Serial.println();


    Serial.print(
        "RID Packets: "
    );

    Serial.println(
        target.packetCount
    );


    Serial.print(
        "Basic ID Packets: "
    );

    Serial.println(
        target.basicIDCount
    );


    Serial.print(
        "Location Packets: "
    );

    Serial.println(
        target.locationCount
    );


    Serial.print(
        "System Packets: "
    );

    Serial.println(
        target.systemCount
    );


    Serial.print(
        "Operator ID Packets: "
    );

    Serial.println(
        target.operatorIDCount
    );


    Serial.print(
        "Last Seen: "
    );

    Serial.print(
        millis()
        -
        target.lastSeenMs
    );

    Serial.println(
        " ms ago"
    );


    Serial.print(
        "Protocol: ODID v"
    );

    Serial.println(
        target.protocolVersion
    );


    Serial.println(
        "========================================"
    );
}


void printAllTargetDetails() {

    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        if (
            targets[i].allocated
        ) {

            printTargetDetail(
                i
            );
        }
    }
}


// ====================================================
// SERIAL COMMANDS
// ====================================================

// D4E-A:
// Explicit declarations because the receiver-reference provider
// is implemented later in this .ino.

bool wsSetManualFixedReceiverReference(
    double latitude,
    double longitude
);

void wsClearReceiverReference();

uint8_t wsReceiverReferenceStatusCode();


void processSerialCommands() {

    static char commandBuffer[64];
    static size_t commandLength = 0;
    static bool discardUntilEOL = false;


    while (
        Serial.available() > 0
    ) {

        int incoming =
            Serial.read();


        if (
            incoming < 0
        ) {
            continue;
        }


        char c =
            (char)incoming;


        if (
            c == '\r'
            ||
            c == '\n'
        ) {

            if (
                discardUntilEOL
            ) {

                discardUntilEOL = false;
                commandLength = 0;
                continue;
            }


            if (
                commandLength == 0
            ) {
                continue;
            }


            commandBuffer[
                commandLength
            ] = '\0';


            if (
                commandLength == 1
            ) {

                char command =
                    commandBuffer[0];


                if (
                    command == 'p'
                    ||
                    command == 'P'
                ) {

                    outputMode =
                        MODE_PRIVATE;


                    Serial.println();

                    Serial.println(
                        "*** OUTPUT MODE -> PRIVATE ***"
                    );


                    Serial.println(
                        "Identifiers and coordinates redacted."
                    );
                }


                else if (
                    command == 'o'
                    ||
                    command == 'O'
                ) {

                    outputMode =
                        MODE_OPEN;


                    Serial.println();

                    Serial.println(
                        "*** OUTPUT MODE -> OPEN ***"
                    );


                    Serial.println(
                        "Full identifiers and coordinates enabled."
                    );
                }


                else if (
                    command == 's'
                    ||
                    command == 'S'
                ) {

                    Serial.println();

                    Serial.println(
                        "*** MULTI-TARGET SNAPSHOT ***"
                    );


                    if (targetMutex != nullptr) {
                        xSemaphoreTake(
                            targetMutex,
                            portMAX_DELAY
                        );
                    }

                    printAllTargetDetails();

                    if (targetMutex != nullptr) {
                        xSemaphoreGive(
                            targetMutex
                        );
                    }
                }
            }


            // ------------------------------------------------
            // D4E-A RECEIVER REFERENCE COMMANDS
            //
            // R,<lat>,<lon>
            // R,STATUS
            // R,CLEAR
            //
            // Coordinates are intentionally never printed back.
            // ------------------------------------------------

            else if (
                commandLength >= 3
                &&
                (
                    commandBuffer[0] == 'R'
                    ||
                    commandBuffer[0] == 'r'
                )
                &&
                commandBuffer[1] == ','
            ) {

                const char *payload =
                    commandBuffer + 2;


                // --------------------------------------------
                // R,CLEAR
                // --------------------------------------------

                if (
                    strcmp(
                        payload,
                        "CLEAR"
                    )
                    == 0
                ) {

                    ridGnssD3b2SetMode(false);
                    wsClearReceiverReference();

                    Serial.println();
                    Serial.println(
                        "*** RECEIVER REFERENCE -> NONE ***"
                    );

                    Serial.println(
                        "Manual fixed reference cleared."
                    );
                }


                // --------------------------------------------
                // R,STATUS
                //
                // Deliberately reports state only.
                // Never prints coordinates.
                // --------------------------------------------

                else if (
                    strcmp(
                        payload,
                        "STATUS"
                    )
                    == 0
                ) {

                    Serial.println();

                    const uint8_t referenceStatus =
                        wsReceiverReferenceStatusCode();


                    if (referenceStatus == 1) {

                        Serial.println(
                            "*** RECEIVER REFERENCE: FIXED / VALID ***"
                        );
                    }

                    else if (referenceStatus == 2) {

                        Serial.println(
                            "*** RECEIVER REFERENCE: GNSS / VALID ***"
                        );
                    }

                    else {

                        Serial.println(
                            "*** RECEIVER REFERENCE: NONE ***"
                        );
                    }
                }


                // --------------------------------------------
                // R,GNSS: explicit GNSS reference request
                // --------------------------------------------
                else if (strcmp(payload, "GNSS") == 0) {
                    ridGnssD3b2SetMode(true);
                    Serial.println("*** RECEIVER REFERENCE -> GNSS REQUESTED ***");
                    Serial.println("Waiting for fresh valid GNSS position.");
                }

                // --------------------------------------------
                // R,<latitude>,<longitude>
                // --------------------------------------------

                else {

                    char *latitudeEnd =
                        nullptr;

                    char *longitudeEnd =
                        nullptr;


                    double latitude =
                        strtod(
                            payload,
                            &latitudeEnd
                        );


                    bool formatValid =
                        (
                            latitudeEnd
                            != payload
                            &&
                            latitudeEnd
                            != nullptr
                            &&
                            *latitudeEnd
                                == ','
                        );


                    double longitude =
                        0.0;


                    if (formatValid) {

                        char *longitudeStart =
                            latitudeEnd + 1;


                        longitude =
                            strtod(
                                longitudeStart,
                                &longitudeEnd
                            );


                        formatValid =
                            (
                                longitudeEnd
                                != longitudeStart
                                &&
                                longitudeEnd
                                != nullptr
                                &&
                                *longitudeEnd
                                    == '\0'
                            );
                    }


                    bool coordinateRangeValid =
                        (
                            formatValid
                            &&
                            isfinite(latitude)
                            &&
                            isfinite(longitude)
                            &&
                            latitude >= -90.0
                            &&
                            latitude <= 90.0
                            &&
                            longitude >= -180.0
                            &&
                            longitude <= 180.0
                        );


                    if (
                        coordinateRangeValid
                        &&
                        wsSetManualFixedReceiverReference(
                            latitude,
                            longitude
                        )
                    ) {

                        ridGnssD3b2SetMode(false);

                        Serial.println();

                        Serial.println(
                            "*** RECEIVER REFERENCE -> FIXED / VALID ***"
                        );

                        Serial.println(
                            "Coordinates accepted locally; values not echoed."
                        );
                    }

                    else {

                        Serial.println();

                        Serial.println(
                            "*** RECEIVER REFERENCE REJECTED ***"
                        );

                        Serial.println(
                            "Use: R,<latitude>,<longitude>"
                        );

                        Serial.println(
                            "Existing valid reference was not intentionally replaced."
                        );
                    }
                }
            }


            commandLength = 0;
            continue;
        }


        if (
            discardUntilEOL
        ) {
            continue;
        }


        if (
            commandLength <
            sizeof(commandBuffer) - 1
        ) {

            commandBuffer[
                commandLength++
            ] = c;
        }

        else {

            commandLength = 0;
            discardUntilEOL = true;
        }
    }
}


// ====================================================
// RESET CAPTURE STATE FOR A NEW HANDHELD TEST
// Call only while BLE scanning is stopped.
// ====================================================

void resetRIDCaptureState() {

    if (targetMutex != nullptr) {
        xSemaphoreTake(
            targetMutex,
            portMAX_DELAY
        );
    }

    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {
        targets[i] = RIDTarget();
    }

    if (targetMutex != nullptr) {
        xSemaphoreGive(
            targetMutex
        );
    }

    totalAdvertisements = 0;
    remoteIDPackets = 0;
    targetAllocationFailures = 0;
    lastDashboardPrintMs = 0;

    for (
        uint8_t i = 0;
        i < 16;
        i++
    ) {
        messageTypeCounts[i] = 0;
    }

    scanCompletePending = false;
    scanStopRequested = false;
    scanStartedMs = 0;
    scanWindowCount = 0;

    if (
        pBLEScan != nullptr
        &&
        !pBLEScan->isScanning()
    ) {
        pBLEScan->clearResults();
    }
}


// ====================================================
// BLE CALLBACK
// ====================================================

class RemoteIDCallbacks
    :
    public BLEAdvertisedDeviceCallbacks {

    void onResult(
        BLEAdvertisedDevice device
    ) override {

        totalAdvertisements++;


        // =================================================
        // ESP32-S3 / NimBLE RAW ADVERTISEMENT TRANSPORT
        //
        // ESP32 Arduino 3.3.11 NimBLE reuses advertiser
        // objects and parseAdvertisement() accumulates parsed
        // Service Data entries across duplicate reports.
        //
        // Scanner parsing is therefore disabled for the active
        // S3 harness. getPayload() now represents only the
        // current passive advertisement because setPayload()
        // replaces the previous raw payload when append=false.
        //
        // Walk the current BLE AD structures directly and pass
        // only validated 0xFFFA / 27-byte RID Service Data into
        // the accepted RID decoder path below.
        // =================================================

        const uint8_t *payload =
            device.getPayload();

        size_t payloadLength =
            device.getPayloadLength();


        if (
            payload != nullptr
            &&
            payloadLength > 0
        ) {

            size_t offset = 0;


            while (
                offset < payloadLength
            ) {

                uint8_t adLength =
                    payload[offset];


                // A zero-length field terminates the AD data.
                if (
                    adLength == 0
                ) {

                    break;
                }


                // adLength counts every byte after the length
                // byte: one AD type byte plus the AD data.
                size_t completeFieldLength =
                    static_cast<size_t>(
                        adLength
                    )
                    +
                    1;


                // Reject a malformed/truncated AD field before
                // accessing anything beyond the current buffer.
                if (
                    completeFieldLength
                    >
                    (
                        payloadLength
                        -
                        offset
                    )
                ) {

                    break;
                }


                // Every non-zero AD structure contains a type.
                uint8_t adType =
                    payload[
                        offset + 1
                    ];


                size_t adDataLength =
                    static_cast<size_t>(
                        adLength
                    )
                    -
                    1;


                const uint8_t *adData =
                    payload
                    +
                    offset
                    +
                    2;


                // Bluetooth AD Type 0x16:
                // Service Data - 16-bit UUID.
                if (
                    adType == 0x16
                    &&
                    adDataLength >= 2
                ) {

                    // Bluetooth 16-bit UUID is little-endian
                    // inside the Service Data AD structure.
                    uint16_t serviceUUID =
                        static_cast<uint16_t>(
                            adData[0]
                        )
                        |
                        (
                            static_cast<uint16_t>(
                                adData[1]
                            )
                            << 8
                        );


                    size_t serviceDataLength =
                        adDataLength
                        -
                        2;


                    const uint8_t *serviceDataBytes =
                        adData
                        +
                        2;


                    bool isRemoteIDUUID =
                        serviceUUID
                        ==
                        0xFFFA;


                    bool hasExpectedRemoteIDLength =
                        serviceDataLength
                        ==
                        27;


                    bool hasOpenDroneIDCode =

                        serviceDataLength >= 1

                        &&

                        serviceDataBytes[0]
                        ==
                        0x0D;


                    if (
                        isRemoteIDUUID
                        &&
                        hasOpenDroneIDCode
                        &&
                        hasExpectedRemoteIDLength
                    ) {

                        // Preserve binary RID bytes exactly,
                        // including any embedded zero values.
                        String serviceData(
                            reinterpret_cast<const char *>(
                                serviceDataBytes
                            ),
                            serviceDataLength
                        );


                        remoteIDPackets++;


                        String sourceAddress =
                            device
                            .getAddress()
                            .toString()
                            .c_str();


                        if (targetMutex != nullptr) {
                            xSemaphoreTake(
                                targetMutex,
                                portMAX_DELAY
                            );
                        }


                        int targetIndex =
                            findOrCreateTarget(
                                sourceAddress
                            );


                        if (
                            targetIndex < 0
                        ) {

                            if (targetMutex != nullptr) {
                                xSemaphoreGive(
                                    targetMutex
                                );
                            }

                            offset +=
                                completeFieldLength;

                            continue;
                        }


                        RIDTarget &target =
                            targets[
                                targetIndex
                            ];


                        target.packetCount++;

                        target.lastSeenMs =
                            millis();


                        addRSSISample(
                            target,
                            device.getRSSI()
                        );


                        uint8_t odidHeader =
                            getByte(
                                serviceData,
                                2
                            );


                        uint8_t messageType =
                            (
                                odidHeader >> 4
                            )
                            &
                            0x0F;


                        uint8_t protocolVersion =
                            odidHeader
                            &
                            0x0F;


                        target.protocolVersion =
                            protocolVersion;


                        messageTypeCounts[
                            messageType
                        ]++;


                        if (
                            messageType == 0
                        ) {

                            updateBasicID(
                                target,
                                serviceData
                            );
                        }


                        else if (
                            messageType == 1
                        ) {

                            updateLocation(
                                target,
                                serviceData
                            );
                        }


                        else if (
                            messageType == 4
                        ) {

                            updateSystem(
                                target,
                                serviceData
                            );
                        }


                        else if (
                            messageType == 5
                        ) {

                            updateOperatorID(
                                target,
                                serviceData
                            );
                        }


                        if (targetMutex != nullptr) {
                            xSemaphoreGive(
                                targetMutex
                            );
                        }
                    }
                }


                offset +=
                    completeFieldLength;
            }
        }


        // Periodic dashboard output is intentionally NOT
        // performed inside this BLE callback.
        //
        // serviceTargetDashboard() owns diagnostic output from
        // the main loop so USB Serial can never hold up RF work.
    }
};


RemoteIDCallbacks ridCallbacks;


// ============================================================
// STAGE 4B-B ASYNCHRONOUS BLE SCAN CONTROL
// ============================================================

void ridScanCompleteCallback(
    BLEScanResults results
) {
    (void)results;

    // Do not perform display/serial-heavy work inside
    // the BLE completion callback. A completed BLE window is
    // serviced from loop() and automatically rolled over unless
    // the operator requested STOP/HOME.
    scanCompletePending = true;
}


bool startRIDScanWindow(
    bool newOperatorSession
) {
    if (!ridBootAckAccepted) {
        Serial.println("[ACK] BLE START blocked.");
        return false;
    }


    if (pBLEScan == nullptr) {
        Serial.println(
            "[ERROR] BLE scanner is not initialized."
        );

        return false;
    }

    if (pBLEScan->isScanning()) {
        Serial.println(
            "[INFO] BLE scan window is already running."
        );

        return false;
    }

    scanCompletePending = false;

    if (newOperatorSession) {
        scanStopRequested = false;
        scanStartedMs = millis();
        scanWindowCount = 0;
    }

    bool started =
        pBLEScan->start(
            SCAN_WINDOW_SECONDS,
            ridScanCompleteCallback,
            false
        );

    if (started) {
        scanSessionActive = true;
        scanWindowCount++;

        if (newOperatorSession) {
            Serial.println(
                "[SCAN] Operator-owned continuous RID session started."
            );
        }
        else {
            Serial.print(
                "[SCAN] Continuous RID window rollover -> #"
            );
            Serial.println(scanWindowCount);
        }
    }
    else {
        if (newOperatorSession) {
            scanSessionActive = false;
        }

        Serial.println(
            "[ERROR] Asynchronous RID scan window failed to start."
        );
    }

    return started;
}


bool startRIDScanAsync() {
    return startRIDScanWindow(true);
}


bool continueRIDScanAsync() {

    if (
        pBLEScan == nullptr ||
        !scanSessionActive ||
        scanStopRequested
    ) {
        return false;
    }

    // A completed window can leave result objects allocated inside
    // the BLE scanner. They are no longer needed because decoded RID
    // state already lives in targets[]. Clearing them also prevents
    // accumulation across a long-running handheld session.
    if (!pBLEScan->isScanning()) {
        pBLEScan->clearResults();
    }

    return startRIDScanWindow(false);
}


bool requestStopRIDScan() {

    if (
        pBLEScan == nullptr ||
        !scanSessionActive
    ) {
        return false;
    }

    scanStopRequested = true;

    bool stopped = true;

    if (pBLEScan->isScanning()) {
        stopped = pBLEScan->stop();
    }

    if (stopped) {
        // Bluedroid does not need to fire the completion
        // callback for our firmware to finish the session.
        scanCompletePending = true;
    }
    else {
        // Stop request failed; allow continued
        // scanning and another operator attempt.
        scanStopRequested = false;
    }

    return stopped;
}


void finalizeRIDScan() {

    if (!scanSessionActive) {
        scanCompletePending = false;
        return;
    }

    scanSessionActive = false;
    scanCompletePending = false;

    Serial.println();
    Serial.println(
        "*** FINAL MULTI-TARGET SNAPSHOT ***"
    );

    printAllTargetDetails();

    Serial.println();

    if (scanStopRequested) {
        Serial.println(
            "=== RID CAPTURE STOPPED BY USER ==="
        );
    }
    else {
        // Defensive fallback only. Normal BLE window completion no
        // longer finalizes an operator session in Stage 4B-B2C.
        Serial.println(
            "=== RID CAPTURE ENDED BY CONTROLLER ==="
        );
    }

    Serial.print(
        "Capture elapsed ms: "
    );

    Serial.println(
        millis() - scanStartedMs
    );

    Serial.print(
        "Continuous BLE scan windows started: "
    );

    Serial.println(
        scanWindowCount
    );

    Serial.print(
        "Total BLE advertisement callbacks: "
    );

    Serial.println(
        totalAdvertisements
    );

    Serial.print(
        "Validated Remote ID packets: "
    );

    Serial.println(
        remoteIDPackets
    );

    Serial.print(
        "Targets Tracked: "
    );

    Serial.println(
        getTrackedTargetCount()
    );

    Serial.print(
        "Target Allocation Failures: "
    );

    Serial.println(
        targetAllocationFailures
    );

    Serial.println();
    Serial.println(
        "Remote ID message types observed:"
    );

    for (
        int type = 0;
        type < 16;
        type++
    ) {

        if (
            messageTypeCounts[type] > 0
        ) {

            Serial.print(
                "Type "
            );

            Serial.print(
                type
            );

            Serial.print(
                " ("
            );

            Serial.print(
                messageTypeName(type)
            );

            Serial.print(
                "): "
            );

            Serial.println(
                messageTypeCounts[type]
            );
        }
    }

    Serial.println();
    Serial.println(
        "=== END RID CAPTURE ==="
    );

    pBLEScan->clearResults();

    scanStopRequested = false;
}


void serviceRIDScanState() {

    if (!scanSessionActive) {
        return;
    }

    bool underlyingWindowEnded =
        scanCompletePending ||
        (
            pBLEScan != nullptr &&
            !pBLEScan->isScanning()
        );

    if (!underlyingWindowEnded) {
        return;
    }

    // STOP/HOME owns finalization. It must never be converted into
    // an automatic rollover.
    if (scanStopRequested) {
        finalizeRIDScan();
        return;
    }

    // Some BLE backends may report the completion callback a fraction
    // before isScanning() flips false. Keep the pending flag set and
    // let the next loop iteration perform the rollover safely.
    if (
        pBLEScan != nullptr &&
        pBLEScan->isScanning()
    ) {
        return;
    }

    scanCompletePending = false;

    if (!continueRIDScanAsync()) {
        // Do not silently pretend the receiver is still listening.
        // Preserve captured target data for inspection, but mark the
        // session inactive so the UI accurately reflects the failure.
        scanSessionActive = false;

        Serial.println(
            "[ERROR] Continuous RID rollover failed; scan session stopped."
        );
    }
}


// ============================================================
// STAGE 4B-B2B RESCAN CONTROL
// Privacy mode and selectedTarget are UI state and are deliberately
// preserved. Target/counter data starts fresh.
// ============================================================

bool restartRIDScanFromUI() {
    if (pBLEScan == nullptr) {
        Serial.println("[UI ERROR] BLE scanner is not initialized.");
        return false;
    }

    if (scanSessionActive) {
        bool stopped = true;

        if (pBLEScan->isScanning()) {
            stopped = pBLEScan->stop();
        }

        if (!stopped) {
            Serial.println("[UI ERROR] RESCAN could not stop active scan.");
            return false;
        }

        // RESCAN is not STOP/HOME: retire the old session without printing
        // its final report, then let the BLE host settle briefly.
        scanSessionActive = false;
        scanCompletePending = false;
        scanStopRequested = false;

        delay(50);
        scanCompletePending = false;
    }

    resetRIDCaptureState();

    bool started = startRIDScanAsync();

    if (started) {
        Serial.println("[SCAN] RESCAN started fresh RID capture.");
    }

    return started;
}



#if 0
// ============================================================
// CYD1 UI/HARDWARE BLOCK DISABLED FOR S3 BACKEND PARITY
// ============================================================

// ============================================================
// STAGE 4B-B2A UNIFIED HANDHELD UI
// STAGE 4B-B2B SNAPSHOT / RESCAN
// STAGE 4B-B2C CONTINUOUS OPERATOR-OWNED SCAN
// Validated Stage-2 homography + Stage-3 filtered touch input.
// Real RID backend; synthetic target arrays removed.
// ============================================================

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

const int PRESSURE_MIN = 200;
const int TAP_SAMPLES  = 5;
const int HIT_PADDING  = 5;

// Validated CYD1 touch homography.
const double h11 = 0.178699182386;
const double h12 = 0.047115489245;
const double h13 = -228.554894658;
const double h21 = 0.048093676990;
const double h22 = 0.138642946733;
const double h23 = -227.381646595;
const double h31 = 0.000113546161;
const double h32 = 0.000081218131;

const uint32_t LIVE_UI_REFRESH_MS = 1000;
uint32_t lastLiveUIRefreshMs = 0;
uint32_t scanGeneration = 0;
uint32_t tapCounter = 0;
uint8_t selectedTarget = 0;

enum ScreenState {
    SCREEN_HOME,
    SCREEN_PRIVACY,
    SCREEN_LIVE,
    SCREEN_SNAPSHOT
};

ScreenState currentScreen = SCREEN_HOME;

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
    BTN_TARGET_1,
    BTN_TARGET_2,
    BTN_TARGET_3,
    BTN_TARGET_4,
    BTN_BACK_LIVE
};

struct UIButton {
    int x;
    int y;
    int w;
    int h;
    const char *label;
};

const UIButton buttonBegin = {
    40, 90, 240, 56, "BEGIN TEST"
};
const UIButton buttonPrivate = {
    15, 68, 135, 50, "PRIVATE"
};
const UIButton buttonOpen = {
    170, 68, 135, 50, "OPEN"
};
const UIButton buttonStartScan = {
    40, 138, 240, 48, "START SCAN"
};
const UIButton buttonBackHome = {
    100, 198, 120, 32, "BACK"
};
const UIButton buttonSnapshot = {
    12, 160, 140, 32, "SNAPSHOT"
};
const UIButton buttonRescan = {
    168, 160, 140, 32, "RESCAN"
};
const UIButton buttonMode = {
    12, 200, 140, 32, "MODE"
};
const UIButton buttonStop = {
    168, 200, 140, 32, "STOP"
};
const UIButton buttonBackLive = {
    80, 198, 160, 34, "BACK TO LIVE"
};
const UIButton buttonTarget1 = {
    8, 42, 70, 30, "T01"
};
const UIButton buttonTarget2 = {
    86, 42, 70, 30, "T02"
};
const UIButton buttonTarget3 = {
    164, 42, 70, 30, "T03"
};
const UIButton buttonTarget4 = {
    242, 42, 70, 30, "T04"
};

// Fixed-size UI copy avoids touching Arduino String objects while
// the BLE callback may be updating the real target table.
struct UITargetSnapshot {
    bool allocated = false;

    // Lifecycle frozen when this UI copy is captured:
    // 0 = EMPTY, 1 = ACTIVE, 2 = STALE, 3 = LOST.
    uint8_t stateCode = 0;

    uint8_t uaType = 0;
    uint8_t flightStatus = 0;
    uint8_t protocolVersion = 0;

    bool hasPosition = false;
    double latitude = 0.0;
    double longitude = 0.0;

    bool directionValid = false;
    float direction = 0.0f;

    bool horizontalSpeedValid = false;
    float horizontalSpeed = 0.0f;

    bool verticalSpeedValid = false;
    float verticalSpeed = 0.0f;

    bool barometricAltitudeValid = false;
    float barometricAltitude = 0.0f;

    bool geodeticAltitudeValid = false;
    float geodeticAltitude = 0.0f;

    bool heightValid = false;
    float height = 0.0f;
    bool heightFromGround = false;

    int currentRSSI = 0;
    int filteredRSSI = 0;

    uint32_t packetCount = 0;
    uint32_t basicIDCount = 0;
    uint32_t locationCount = 0;
    uint32_t systemCount = 0;
    uint32_t operatorIDCount = 0;
    uint32_t lastSeenMs = 0;

    bool displayIDValid = false;
    char displayID[32] = {0};
};

// The snapshot screen uses this fixed copy. targets[] continues updating
// in the BLE callback while the on-screen snapshot stays unchanged.
UITargetSnapshot frozenSnapshot;
bool frozenSnapshotCaptured = false;
uint8_t frozenSnapshotTarget = 0;
OutputMode frozenSnapshotMode = MODE_PRIVATE;
uint32_t frozenSnapshotCapturedMs = 0;

// ============================================================
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

void drawSnapshotScreen();
bool restartRIDScanFromUI();

bool captureUITargetSnapshot(
    uint8_t index,
    UITargetSnapshot &snapshot
) {
    snapshot = UITargetSnapshot();

    if (index >= MAX_TARGETS) {
        return false;
    }

    if (targetMutex != nullptr) {
        xSemaphoreTake(targetMutex, portMAX_DELAY);
    }

    RIDTarget &target = targets[index];
    snapshot.allocated = target.allocated;

    if (target.allocated) {
        uint32_t age = millis() - target.lastSeenMs;

        if (age <= TARGET_ACTIVE_MS) {
            snapshot.stateCode = 1;
        }
        else if (age <= TARGET_STALE_MS) {
            snapshot.stateCode = 2;
        }
        else {
            snapshot.stateCode = 3;
        }

        snapshot.uaType = target.uaType;
        snapshot.flightStatus = target.flightStatus;
        snapshot.protocolVersion = target.protocolVersion;

        snapshot.hasPosition = target.hasPosition;
        snapshot.latitude = target.latitude;
        snapshot.longitude = target.longitude;

        snapshot.directionValid = target.directionValid;
        snapshot.direction = target.direction;

        snapshot.horizontalSpeedValid = target.horizontalSpeedValid;
        snapshot.horizontalSpeed = target.horizontalSpeed;

        snapshot.verticalSpeedValid = target.verticalSpeedValid;
        snapshot.verticalSpeed = target.verticalSpeed;

        snapshot.barometricAltitudeValid = target.barometricAltitudeValid;
        snapshot.barometricAltitude = target.barometricAltitude;

        snapshot.geodeticAltitudeValid = target.geodeticAltitudeValid;
        snapshot.geodeticAltitude = target.geodeticAltitude;

        snapshot.heightValid = target.heightValid;
        snapshot.height = target.height;
        snapshot.heightFromGround = target.heightFromGround;

        snapshot.currentRSSI = target.currentRSSI;
        snapshot.filteredRSSI = getFilteredRSSI(target);

        snapshot.packetCount = target.packetCount;
        snapshot.basicIDCount = target.basicIDCount;
        snapshot.locationCount = target.locationCount;
        snapshot.systemCount = target.systemCount;
        snapshot.operatorIDCount = target.operatorIDCount;
        snapshot.lastSeenMs = target.lastSeenMs;

        const char *idText = nullptr;

        if (target.hasSerialID) {
            idText = target.serialID.c_str();
        }
        else if (target.hasRegistrationID) {
            idText = target.registrationID.c_str();
        }
        else if (target.hasOperatorID) {
            idText = target.operatorID.c_str();
        }

        if (idText != nullptr) {
            snprintf(
                snapshot.displayID,
                sizeof(snapshot.displayID),
                "%s",
                idText
            );
            snapshot.displayIDValid = true;
        }
    }

    if (targetMutex != nullptr) {
        xSemaphoreGive(targetMutex);
    }

    return snapshot.allocated;
}

const char *uiTargetStateName(
    const UITargetSnapshot &snapshot
) {
    switch (snapshot.stateCode) {
        case 1: return "ACTIVE";
        case 2: return "STALE";
        case 3: return "LOST";
        default: return "EMPTY";
    }
}

uint16_t uiTargetStateColor(
    const UITargetSnapshot &snapshot
) {
    switch (snapshot.stateCode) {
        case 1: return TFT_GREEN;
        case 2: return TFT_YELLOW;
        case 3: return TFT_RED;
        default: return TFT_WHITE;
    }
}

int medianValues(
    int values[],
    int count
) {
    int temp[TAP_SAMPLES];

    for (int i = 0; i < count; i++) {
        temp[i] = values[i];
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (temp[j] < temp[i]) {
                int swapValue = temp[i];
                temp[i] = temp[j];
                temp[j] = swapValue;
            }
        }
    }

    return temp[count / 2];
}

bool mapTouchToScreen(
    double rawTouchX,
    double rawTouchY,
    double &screenX,
    double &screenY
) {
    double denominator =
        h31 * rawTouchX
        + h32 * rawTouchY
        + 1.0;

    if (fabs(denominator) < 0.000001) {
        return false;
    }

    screenX =
        (
            h11 * rawTouchX
            + h12 * rawTouchY
            + h13
        ) / denominator;

    screenY =
        (
            h21 * rawTouchX
            + h22 * rawTouchY
            + h23
        ) / denominator;

    return true;
}

void waitForRelease() {
    unsigned long startTime = millis();

    while (
        ts.touched()
        && millis() - startTime < 2500
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
    if (!ts.tirqTouched() || !ts.touched()) {
        return false;
    }

    int sampleX[TAP_SAMPLES];
    int sampleY[TAP_SAMPLES];
    int sampleZ[TAP_SAMPLES];
    int validSamples = 0;
    unsigned long captureStart = millis();

    while (
        ts.touched()
        && validSamples < TAP_SAMPLES
        && millis() - captureStart < 400
    ) {
        TS_Point p = ts.getPoint();

        if (p.z >= PRESSURE_MIN) {
            sampleX[validSamples] = p.x;
            sampleY[validSamples] = p.y;
            sampleZ[validSamples] = p.z;
            validSamples++;
        }

        delay(10);
    }

    waitForRelease();

    if (validSamples < 3) {
        return false;
    }

    medianX = medianValues(sampleX, validSamples);
    medianY = medianValues(sampleY, validSamples);
    medianZ = medianValues(sampleZ, validSamples);

    return true;
}

bool pointInside(
    double screenX,
    double screenY,
    const UIButton &button
) {
    return (
        screenX >= button.x - HIT_PADDING
        && screenX <= button.x + button.w + HIT_PADDING
        && screenY >= button.y - HIT_PADDING
        && screenY <= button.y + button.h + HIT_PADDING
    );
}

void drawButton(
    const UIButton &button,
    bool selected = false
) {
    uint16_t fillColor = TFT_DARKGREY;
    uint16_t borderColor = TFT_WHITE;

    if (selected) {
        fillColor = TFT_BLUE;
        borderColor = TFT_CYAN;
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
        TFT_WHITE,
        fillColor
    );

    tft.drawCentreString(
        button.label,
        button.x + button.w / 2,
        button.y + button.h / 2 - 7,
        2
    );
}

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

void drawTargetTab(
    const UIButton &button,
    uint8_t index
) {
    UITargetSnapshot snapshot;
    captureUITargetSnapshot(
        index,
        snapshot
    );

    bool selected =
        selectedTarget == index;

    uint16_t fillColor =
        selected ? TFT_BLUE : TFT_BLACK;

    uint16_t borderColor =
        selected
        ? TFT_CYAN
        : uiTargetStateColor(snapshot);

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

void drawHomeScreen() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("REMOTE ID SENSOR");

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawCentreString(
        "CYD1 HANDHELD",
        160,
        35,
        2
    );

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(
        "Passive RID Detection",
        160,
        60,
        1
    );

    drawButton(buttonBegin);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(
        "SYSTEM READY",
        160,
        170,
        2
    );

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(
        "Touch BEGIN TEST",
        160,
        200,
        1
    );
}

void drawPrivacyScreen() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("SELECT OUTPUT MODE");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(
        "PRIVATE is default",
        160,
        35,
        1
    );

    bool privateSelected =
        outputMode == MODE_PRIVATE;

    drawButton(
        buttonPrivate,
        privateSelected
    );
    drawButton(
        buttonOpen,
        !privateSelected
    );
    drawButton(buttonStartScan);
    drawButton(buttonBackHome);
}

void drawLiveScreen() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("REMOTE ID - LIVE");

    bool privateSelected =
        outputMode == MODE_PRIVATE;

    tft.setTextColor(
        privateSelected ? TFT_CYAN : TFT_YELLOW,
        TFT_BLACK
    );
    tft.drawString(
        privateSelected ? "PRIVATE" : "OPEN",
        10,
        26,
        1
    );

    const char *scanText =
        scanSessionActive
        ? "SCANNING"
        : (
            scanGeneration > 0
            ? "SCAN DONE"
            : "IDLE"
        );

    tft.setTextColor(
        scanSessionActive ? TFT_GREEN : TFT_YELLOW,
        TFT_BLACK
    );
    tft.drawRightString(
        scanText,
        310,
        26,
        1
    );

    drawTargetTab(buttonTarget1, 0);
    drawTargetTab(buttonTarget2, 1);
    drawTargetTab(buttonTarget3, 2);
    drawTargetTab(buttonTarget4, 3);

    UITargetSnapshot snapshot;
    captureUITargetSnapshot(
        selectedTarget,
        snapshot
    );

    String targetName =
        String("T0")
        + String(selectedTarget + 1);

    tft.setTextColor(
        uiTargetStateColor(snapshot),
        TFT_BLACK
    );

    if (!snapshot.allocated) {
        tft.drawString(
            targetName + "  EMPTY",
            10,
            80,
            1
        );

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(
            "WAITING FOR RID TARGET",
            10,
            106,
            1
        );
        tft.drawString(
            "SELECT T01-T04",
            10,
            124,
            1
        );
    }
    else {
        String line1 =
            targetName
            + "  "
            + uiTargetStateName(snapshot)
            + "  "
            + uaTypeName(snapshot.uaType);

        tft.drawString(
            line1,
            10,
            80,
            1
        );

        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        String line2 =
            String("RSSI: ")
            + String(snapshot.filteredRSSI)
            + " dBm   "
            + flightStatusName(snapshot.flightStatus);
        tft.drawString(line2, 10, 96, 1);

        String headingText =
            snapshot.directionValid
            ? String(snapshot.direction, 0)
            : String("UNK");

        String line3 =
            String("POS: ")
            + (snapshot.hasPosition ? "VALID" : "UNKNOWN")
            + "   HDG: "
            + headingText;
        tft.drawString(line3, 10, 112, 1);

        String hSpeedText =
            snapshot.horizontalSpeedValid
            ? String(snapshot.horizontalSpeed, 1)
            : String("UNK");
        String vSpeedText =
            snapshot.verticalSpeedValid
            ? String(snapshot.verticalSpeed, 1)
            : String("UNK");

        String line4 =
            String("HSPD: ")
            + hSpeedText
            + "   VSPD: "
            + vSpeedText;
        tft.drawString(line4, 10, 128, 1);

        String line5;
        if (privateSelected) {
            line5 = "ID: [REDACTED]";
        }
        else if (snapshot.displayIDValid) {
            line5 =
                String("ID: ")
                + snapshot.displayID;
        }
        else {
            line5 = "ID: <not provided>";
        }
        tft.drawString(line5, 10, 144, 1);
    }

    drawButton(buttonSnapshot);
    drawButton(buttonRescan);

    UIButton modeButton = buttonMode;
    modeButton.label =
        privateSelected
        ? "SWITCH OPEN"
        : "SWITCH PRIVATE";
    drawButton(modeButton);
    drawButton(buttonStop);
}

void drawSnapshotScreen() {
    tft.fillScreen(TFT_BLACK);
    drawHeader("TARGET SNAPSHOT");

    bool privateSnapshot =
        frozenSnapshotMode == MODE_PRIVATE;

    String targetName =
        String("T0")
        + String(frozenSnapshotTarget + 1);

    tft.setTextColor(
        uiTargetStateColor(frozenSnapshot),
        TFT_BLACK
    );

    String titleLine =
        targetName
        + "  "
        + uiTargetStateName(frozenSnapshot)
        + "  "
        + (privateSnapshot ? "PRIVATE" : "OPEN");

    tft.drawString(titleLine, 12, 32, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    if (!frozenSnapshotCaptured || !frozenSnapshot.allocated) {
        tft.drawString(
            "NO ALLOCATED TARGET AT SNAPSHOT",
            12,
            82,
            1
        );
        tft.drawString(
            "BACK returns to live receiver",
            12,
            104,
            1
        );
        drawButton(buttonBackLive);
        return;
    }

    tft.drawString(
        String("TYPE: ") + uaTypeName(frozenSnapshot.uaType),
        12, 58, 1
    );

    tft.drawString(
        String("FLIGHT: ")
        + flightStatusName(frozenSnapshot.flightStatus)
        + "  RSSI: "
        + String(frozenSnapshot.filteredRSSI),
        12, 74, 1
    );

    String headingText =
        frozenSnapshot.directionValid
        ? String(frozenSnapshot.direction, 0)
        : String("UNK");
    tft.drawString(
        String("HDG: ") + headingText,
        12, 90, 1
    );

    String hSpeedText =
        frozenSnapshot.horizontalSpeedValid
        ? String(frozenSnapshot.horizontalSpeed, 1)
        : String("UNK");
    String vSpeedText =
        frozenSnapshot.verticalSpeedValid
        ? String(frozenSnapshot.verticalSpeed, 1)
        : String("UNK");
    tft.drawString(
        String("HSPD: ")
        + hSpeedText
        + "  VSPD: "
        + vSpeedText,
        12, 106, 1
    );

    String idLine;
    if (!frozenSnapshot.displayIDValid) {
        idLine = "ID: <not provided>";
    }
    else if (privateSnapshot) {
        idLine = "ID: [REDACTED]";
    }
    else {
        idLine = String("ID: ") + frozenSnapshot.displayID;
    }
    tft.drawString(idLine, 12, 122, 1);

    String positionLine;
    if (!frozenSnapshot.hasPosition) {
        positionLine = "POS: UNKNOWN";
    }
    else if (privateSnapshot) {
        positionLine = "POS: VALID [REDACTED]";
    }
    else {
        positionLine =
            String("POS: ")
            + String(frozenSnapshot.latitude, 6)
            + ","
            + String(frozenSnapshot.longitude, 6);
    }
    tft.drawString(positionLine, 12, 138, 1);

    String altitudeLine = "ALT G:";
    altitudeLine +=
        frozenSnapshot.geodeticAltitudeValid
        ? String(frozenSnapshot.geodeticAltitude, 1)
        : String("UNK");
    altitudeLine += " R:";
    altitudeLine +=
        frozenSnapshot.heightValid
        ? String(frozenSnapshot.height, 1)
        : String("UNK");
    tft.drawString(altitudeLine, 12, 154, 1);

    tft.drawString(
        String("PKTS: ")
        + String(frozenSnapshot.packetCount)
        + "  ODID v"
        + String(frozenSnapshot.protocolVersion),
        12, 170, 1
    );

    drawButton(buttonBackLive);
}

void drawCurrentScreen() {
    switch (currentScreen) {
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

ButtonID hitTestCurrentScreen(
    double x,
    double y
) {
    switch (currentScreen) {
        case SCREEN_HOME:
            if (pointInside(x, y, buttonBegin)) {
                return BTN_BEGIN;
            }
            break;

        case SCREEN_PRIVACY:
            if (pointInside(x, y, buttonPrivate)) {
                return BTN_PRIVATE;
            }
            if (pointInside(x, y, buttonOpen)) {
                return BTN_OPEN;
            }
            if (pointInside(x, y, buttonStartScan)) {
                return BTN_START_SCAN;
            }
            if (pointInside(x, y, buttonBackHome)) {
                return BTN_BACK_HOME;
            }
            break;

        case SCREEN_LIVE:
            if (pointInside(x, y, buttonTarget1)) {
                return BTN_TARGET_1;
            }
            if (pointInside(x, y, buttonTarget2)) {
                return BTN_TARGET_2;
            }
            if (pointInside(x, y, buttonTarget3)) {
                return BTN_TARGET_3;
            }
            if (pointInside(x, y, buttonTarget4)) {
                return BTN_TARGET_4;
            }
            if (pointInside(x, y, buttonSnapshot)) {
                return BTN_SNAPSHOT;
            }
            if (pointInside(x, y, buttonRescan)) {
                return BTN_RESCAN;
            }
            if (pointInside(x, y, buttonMode)) {
                return BTN_MODE_TOGGLE;
            }
            if (pointInside(x, y, buttonStop)) {
                return BTN_STOP;
            }
            break;

        case SCREEN_SNAPSHOT:
            if (pointInside(x, y, buttonBackLive)) {
                return BTN_BACK_LIVE;
            }
            break;
    }

    return BTN_NONE;
}

void logButtonEvent(
    ButtonID button,
    double x,
    double y
) {
    tapCounter++;

    Serial.print("[UI] tap=");
    Serial.print(tapCounter);
    Serial.print(" button=");
    Serial.print(static_cast<int>(button));
    Serial.print(" x=");
    Serial.print(x, 1);
    Serial.print(" y=");
    Serial.print(y, 1);
    Serial.print(" mode=");
    Serial.println(
        outputMode == MODE_PRIVATE
        ? "PRIVATE"
        : "OPEN"
    );
}

void processButton(
    ButtonID button
) {
    bool redraw = true;

    switch (button) {
        case BTN_BEGIN:
            selectedTarget = 0;
            frozenSnapshotCaptured = false;
            outputMode = MODE_PRIVATE;
            currentScreen = SCREEN_PRIVACY;
            Serial.println("ACTION: BEGIN TEST");
            break;

        case BTN_PRIVATE:
            outputMode = MODE_PRIVATE;
            Serial.println("ACTION: PRIVATE");
            break;

        case BTN_OPEN:
            outputMode = MODE_OPEN;
            Serial.println("ACTION: OPEN");
            break;

        case BTN_START_SCAN:
            if (scanSessionActive) {
                Serial.println("[UI] Scan already active.");
                break;
            }

            resetRIDCaptureState();
            frozenSnapshotCaptured = false;

            if (startRIDScanAsync()) {
                scanGeneration++;
                selectedTarget = 0;
                currentScreen = SCREEN_LIVE;
                Serial.println("ACTION: START SCAN");
            }
            else {
                Serial.println("[UI ERROR] START SCAN failed.");
            }
            break;

        case BTN_BACK_HOME:
            currentScreen = SCREEN_HOME;
            Serial.println("ACTION: BACK HOME");
            break;

        case BTN_MODE_TOGGLE:
            outputMode =
                outputMode == MODE_PRIVATE
                ? MODE_OPEN
                : MODE_PRIVATE;
            Serial.println("ACTION: MODE TOGGLE");
            break;

        case BTN_TARGET_1:
            selectedTarget = 0;
            Serial.println("ACTION: SELECT T01");
            break;

        case BTN_TARGET_2:
            selectedTarget = 1;
            Serial.println("ACTION: SELECT T02");
            break;

        case BTN_TARGET_3:
            selectedTarget = 2;
            Serial.println("ACTION: SELECT T03");
            break;

        case BTN_TARGET_4:
            selectedTarget = 3;
            Serial.println("ACTION: SELECT T04");
            break;

        case BTN_STOP:
            if (scanSessionActive) {
                if (!requestStopRIDScan()) {
                    Serial.println(
                        "[UI ERROR] STOP failed; session remains active."
                    );
                    redraw = false;
                    break;
                }
            }
            currentScreen = SCREEN_HOME;
            Serial.println("ACTION: STOP / HOME");
            break;

        case BTN_SNAPSHOT:
            frozenSnapshotTarget = selectedTarget;
            frozenSnapshotMode = outputMode;
            frozenSnapshotCapturedMs = millis();
            frozenSnapshotCaptured =
                captureUITargetSnapshot(
                    selectedTarget,
                    frozenSnapshot
                );
            currentScreen = SCREEN_SNAPSHOT;
            Serial.print("ACTION: SNAPSHOT T0");
            Serial.print(selectedTarget + 1);
            Serial.println(
                frozenSnapshotCaptured
                ? " FROZEN"
                : " EMPTY"
            );
            break;

        case BTN_RESCAN:
            // Preserve selectedTarget and outputMode across the restart.
            frozenSnapshotCaptured = false;

            if (restartRIDScanFromUI()) {
                scanGeneration++;
                Serial.println("ACTION: RESCAN");
            }
            else {
                Serial.println("[UI ERROR] RESCAN failed.");
            }
            break;

        case BTN_BACK_LIVE:
            currentScreen = SCREEN_LIVE;
            Serial.println("ACTION: BACK TO LIVE");
            break;

        default:
            redraw = false;
            Serial.println("ACTION: NONE");
            break;
    }

    if (redraw) {
        drawCurrentScreen();
        lastLiveUIRefreshMs = millis();
    }
}

void serviceTouchUI() {
    int rawX = 0;
    int rawY = 0;
    int rawZ = 0;

    if (!captureFilteredTap(rawX, rawY, rawZ)) {
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
        Serial.println("ERROR: TOUCH MAP");
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

    processButton(button);
}

void serviceLiveUIRefresh() {
    if (currentScreen != SCREEN_LIVE) {
        return;
    }

    if (
        millis() - lastLiveUIRefreshMs
        < LIVE_UI_REFRESH_MS
    ) {
        return;
    }

    drawLiveScreen();
    lastLiveUIRefreshMs = millis();
}


// ====================================================
// SETUP
// ====================================================

void setup() {

    Serial.begin(115200);
    delay(STARTUP_DELAY_MS);

    targetMutex =
        xSemaphoreCreateMutex();

    if (targetMutex == nullptr) {
        Serial.println(
            "[FATAL] target mutex allocation failed."
        );
    }

    // Validated CYD1 touch/display initialization.
    touchSPI.begin(
        XPT2046_CLK,
        XPT2046_MISO,
        XPT2046_MOSI,
        XPT2046_CS
    );
    ts.begin(touchSPI);
    ts.setRotation(1);

    tft.init();
    tft.setRotation(1);

    outputMode = MODE_PRIVATE;
    currentScreen = SCREEN_HOME;

    // BLE is initialized at boot, but scanning is operator-started.
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(false);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setAdvertisedDeviceCallbacks(
        &ridCallbacks,
        true
    );

    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        " CYD1 RID HANDHELD - STAGE 4B-B2C"
    );
    Serial.println(
        " UI + RID + SNAPSHOT + CONTINUOUS SCAN"
    );
    Serial.println(
        "========================================"
    );
    Serial.println(
        "Boot state: HOME / BLE IDLE / PRIVATE"
    );
    Serial.println(
        "Touch START SCAN; operator STOP/HOME ends capture."
    );

    drawCurrentScreen();
}


// ====================================================
// LOOP
// ====================================================

void loop() {

    processSerialCommands();
    serviceRIDScanState();
    serviceTouchUI();
    serviceLiveUIRefresh();

    delay(15);
}


#endif  // CYD1 UI/HARDWARE BLOCK DISABLED


// ============================================================
// WAVESHARE ESP32-S3-TOUCH-LCD-3.5 DISPLAY LAYER
// DISPLAY ONLY — TOUCH NOT YET ENABLED
// ============================================================

#define WS_GFX_BL       6

#define WS_SPI_MISO     2
#define WS_SPI_MOSI     1
#define WS_SPI_SCLK     5

#define WS_LCD_CS      -1
#define WS_LCD_DC       3
#define WS_LCD_RST     -1

#define WS_LCD_HOR_RES 320
#define WS_LCD_VER_RES 480

#define WS_I2C_SDA      8
#define WS_I2C_SCL      7


TCA9554 wsTCA(0x20);

Arduino_DataBus *wsBus = new Arduino_ESP32SPI(
    WS_LCD_DC,
    WS_LCD_CS,
    WS_SPI_SCLK,
    WS_SPI_MOSI,
    WS_SPI_MISO
);

Arduino_GFX *wsGfx = new Arduino_ST7796(
    wsBus,
    WS_LCD_RST,
    0,
    true,
    WS_LCD_HOR_RES,
    WS_LCD_VER_RES
);


// ============================================================
// D3B2 RX1024_RENDER_R4: cooperative display / GNSS servicing.
// The first ordinary loop receive pair enables this experiment.
// Setup drawing keeps the native path; startup is a separate gate.
// No parser task, additional UART consumer, or target-lock nesting.
// ============================================================

// Explicit prototypes avoid Arduino .ino prototype-order ambiguity.
bool ridGnssRenderServiceReady();
void ridGnssRenderCheckpoint();
void ridGnssRenderEnableAfterFirstReceive();
static void wsRenderFillRect(int16_t x, int16_t y, int16_t w,
                             int16_t h, uint16_t color);
static void wsRenderFillScreen(uint16_t color);
static void wsRenderFillRoundRect(int16_t x, int16_t y, int16_t w,
                                  int16_t h, int16_t r, uint16_t color);

static constexpr int16_t RID_RENDER_FILL_ROWS = 8;

static void wsRenderFillRect(int16_t x, int16_t y, int16_t w,
                             int16_t h, uint16_t color) {
    if (!ridGnssRenderServiceReady() || w <= 0 || h <= 0) {
        wsGfx->fillRect(x, y, w, h, color);
        ridGnssRenderCheckpoint();
        return;
    }
    ridGnssRenderCheckpoint();
    // Each native call closes its own graphics transaction before the drain.
    // Native clipping is retained. All current UI rectangles are on-screen;
    // 32-bit intermediate arithmetic avoids signed 16-bit addition overflow.
    for (int32_t offset = 0; offset < h; offset += RID_RENDER_FILL_ROWS) {
        const int16_t rows = (h - offset < RID_RENDER_FILL_ROWS)
            ? static_cast<int16_t>(h - offset) : RID_RENDER_FILL_ROWS;
        wsGfx->fillRect(x, static_cast<int16_t>(static_cast<int32_t>(y) + offset),
                        w, rows, color);
        ridGnssRenderCheckpoint();
    }
}

static void wsRenderFillScreen(uint16_t color) {
    if (!ridGnssRenderServiceReady()) {
        wsGfx->fillScreen(color);
        return;
    }
    wsRenderFillRect(0, 0, wsGfx->width(), wsGfx->height(), color);
}

static void wsRenderFillRoundRect(int16_t x, int16_t y, int16_t w,
                                  int16_t h, int16_t r, uint16_t color) {
    if (!ridGnssRenderServiceReady() || w <= 0 || h <= 0 || r < 0) {
        wsGfx->fillRoundRect(x, y, w, h, r, color);
        ridGnssRenderCheckpoint();
        return;
    }
    const int16_t maxRadius = ((w < h) ? w : h) / 2;
    const int16_t radius = (r > maxRadius) ? maxRadius : r;
    if (radius == 0) {
        wsRenderFillRect(x, y, w, h, color);
        return;
    }
    const int32_t capRows = static_cast<int32_t>(radius) * 2 + 1;
    ridGnssRenderCheckpoint();
    if (h <= capRows) {
        wsGfx->fillRoundRect(x, y, w, h, radius, color);
        ridGnssRenderCheckpoint();
        return;
    }
    // Preserve Arduino_GFX 1.5.5's exact rounded raster using native caps.
    // Their inner halves overlap the full-width middle, so the final union
    // equals the original filled rounded rectangle without custom arc math.
    // Current UI radius is 8: each native cap is only 17 rows high.
    wsGfx->fillRoundRect(x, y, w, static_cast<int16_t>(capRows), radius, color);
    ridGnssRenderCheckpoint();
    wsRenderFillRect(x, static_cast<int16_t>(static_cast<int32_t>(y) + radius),
                     w, static_cast<int16_t>(h - 2 * radius), color);
    wsGfx->fillRoundRect(x,
                         static_cast<int16_t>(static_cast<int32_t>(y) + h - capRows),
                         w, static_cast<int16_t>(capRows), radius, color);
    ridGnssRenderCheckpoint();
}

TouchDrvFT6X36 wsTouch;

uint32_t wsLastTouchPollMs = 0;
const uint32_t WS_TOUCH_POLL_MS = 10;

bool wsTouchWasDown = false;

int16_t wsTouchLastX = -1;
int16_t wsTouchLastY = -1;


void wsLcdReset() {

    wsTCA.write1(1, 1);
    delay(10);

    wsTCA.write1(1, 0);
    delay(10);

    wsTCA.write1(1, 1);
    delay(200);
}


// ============================================================
// WAVESHARE HANDHELD UI SHELL
// STAGE 1B-B — REAL SESSION OWNERSHIP
//
// PRIVATE / OPEN, START SCAN, MODE, and STOP / HOME
// now control the validated RID backend.
//
// SNAPSHOT uses a dedicated frozen-data screen.
// TARGET rendering and RADAR readiness remain isolated for
// later controlled integration.
//
// FINAL DIRECTION:
//   HOME
//     -> REFERENCE SELECT
//     -> OUTPUT MODE
//     -> START SCAN
//     -> TARGETS (initial LIVE view)
//     -> RADAR when geographic prerequisites are ready
//
// TARGETS and RADAR are two views of one capture session.
// ============================================================


enum WSUIScreen {
    WS_SCREEN_NOTICE,
    WS_SCREEN_HOME,
    WS_SCREEN_PRIVACY,
    WS_SCREEN_LIVE,
    WS_SCREEN_SNAPSHOT,
    WS_SCREEN_REFERENCE
};


enum WSLiveView {
    WS_VIEW_TARGETS,
    WS_VIEW_RADAR
};


enum WSButtonID {
    WS_BTN_NONE = -1,
    WS_BTN_ACKNOWLEDGE,

    WS_BTN_BEGIN,

    WS_BTN_PRIVATE,
    WS_BTN_OPEN,
    WS_BTN_START,
    WS_BTN_BACK,

    WS_BTN_TAB_TARGETS,
    WS_BTN_TAB_RADAR,

    WS_BTN_T01,
    WS_BTN_T02,
    WS_BTN_T03,
    WS_BTN_T04,

    WS_BTN_SNAPSHOT,
    WS_BTN_RESCAN,
    WS_BTN_MODE,
    WS_BTN_STOP,

    WS_BTN_BACK_LIVE,

    WS_BTN_USE_GNSS,
    WS_BTN_TARGETS_ONLY
};


struct WSUIButton {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    const char *label;

    uint8_t textSize;
};


// ------------------------------------------------------------
// WAVESHARE FIXED TARGET SNAPSHOT
//
// Hardware-independent copy of RID target state.
// Avoids reading Arduino String objects while the BLE callback
// may be updating the live target table.
// ------------------------------------------------------------

struct WSUITargetSnapshot {

    bool allocated = false;

    // 0 = EMPTY
    // 1 = ACTIVE
    // 2 = STALE
    // 3 = LOST
    uint8_t stateCode = 0;

    uint8_t uaType = 0;
    uint8_t flightStatus = 0;
    uint8_t protocolVersion = 0;

    bool hasPosition = false;
    double latitude = 0.0;
    double longitude = 0.0;

    bool directionValid = false;
    float direction = 0.0f;

    bool horizontalSpeedValid = false;
    float horizontalSpeed = 0.0f;

    bool verticalSpeedValid = false;
    float verticalSpeed = 0.0f;

    bool barometricAltitudeValid = false;
    float barometricAltitude = 0.0f;

    bool geodeticAltitudeValid = false;
    float geodeticAltitude = 0.0f;

    bool heightValid = false;
    float height = 0.0f;
    bool heightFromGround = false;

    int currentRSSI = 0;
    int filteredRSSI = 0;

    uint32_t packetCount = 0;
    uint32_t basicIDCount = 0;
    uint32_t locationCount = 0;
    uint32_t systemCount = 0;
    uint32_t operatorIDCount = 0;
    uint32_t lastSeenMs = 0;

    // Timestamp of the most recent valid geographic
    // Location/Vector position update.
    uint32_t lastPositionSeenMs = 0;

    bool displayIDValid = false;
    char displayID[32] = {0};
};


// ------------------------------------------------------------
// Explicit prototypes.
//
// The .ino preprocessor can otherwise generate prototypes
// before our custom UI types are known.
// ------------------------------------------------------------

bool wsPointInside(
    int16_t x,
    int16_t y,
    const WSUIButton &button
);

void wsDrawButton(
    const WSUIButton &button,
    bool selected,
    uint16_t borderColor
);

WSButtonID wsHitTest(
    int16_t x,
    int16_t y
);

const char *wsButtonName(
    WSButtonID button
);

void wsProcessButton(
    WSButtonID button
);

bool wsCaptureTargetSnapshot(
    uint8_t index,
    WSUITargetSnapshot &snapshot
);

const char *wsSnapshotStateName(
    const WSUITargetSnapshot &snapshot
);

void wsDrawSnapshotScreen();

void wsDrawLiveTargetDataPanel();
void serviceWaveshareLiveTargetRefresh();
void serviceWaveshareRadarReadiness();
void serviceWaveshareRadarRefresh();

// R5 uses primitive types in explicit prototypes to avoid .ino ordering issues.
bool ridGnssD3b2PositionUsable();
uint8_t ridGnssUiStatusCode();
const char *wsReferenceFixName(uint8_t status);
void wsDrawReferenceStatus(uint8_t status);
void wsDrawReferenceScreen();
void serviceWaveshareReferenceRefresh();


// ------------------------------------------------------------
// UI STATE
// ------------------------------------------------------------

WSUIScreen wsCurrentScreen =
    WS_SCREEN_NOTICE;

WSLiveView wsCurrentLiveView =
    WS_VIEW_TARGETS;


// Mirrors the real backend outputMode for the touch UI.

bool wsUiPrivateSelected = true;


// Mirrors operator-owned RID session state for the UI.

bool wsUiSessionStarted = false;

// R5 reference page reports suitability, never geographic coordinates.
// Selection is explicit; a valid fix is not required to begin TARGETS capture.
static constexpr uint8_t WS_GNSS_FIX_ACQUIRING = 0U;
static constexpr uint8_t WS_GNSS_FIX_VALID = 1U;
static constexpr uint8_t WS_GNSS_FIX_STALE = 2U;
static constexpr uint8_t WS_GNSS_FIX_UNAVAILABLE = 3U;
static constexpr uint32_t WS_REFERENCE_REFRESH_MS = 500U;
uint32_t wsLastReferenceRefreshMs = 0U;
uint8_t wsReferenceStatusDrawn = 0xffU;


// ============================================================
// RADAR GEOGRAPHIC ENGINE - D4A
//
// Geographic calculations are based ONLY on:
//   - a valid receiver latitude/longitude reference
//   - a valid target latitude/longitude position
//
// RSSI is never used to infer range or bearing.
//
// The first RADAR implementation is NORTH-UP. True bearing is
// derived mathematically from the two geographic coordinates,
// so a physical compass is not required for north-up plotting.
// ============================================================

constexpr double WS_GEO_PI =
    3.14159265358979323846;

constexpr double WS_EARTH_RADIUS_METERS =
    6371008.8;


// Receiver reference intentionally starts invalid.
//
// Reference source is explicit so the UI and geographic engine
// never confuse a fixed bench location with future live GNSS.
//
// RADAR must remain unavailable until:
//   - the receiver reference is valid
//   - the reference source is known
//   - a target has a fresh valid geographic position

enum WSReceiverReferenceSource {

    WS_REF_NONE = 0,

    WS_REF_MANUAL_FIXED = 1,

    WS_REF_GNSS = 2
};


struct WSReceiverReference {

    bool valid = false;

    WSReceiverReferenceSource source =
        WS_REF_NONE;

    double latitude = 0.0;

    double longitude = 0.0;
};


struct WSGeographicSolution {

    bool valid = false;

    double distanceMeters = 0.0;

    double bearingTrueDegrees = 0.0;
};


// ------------------------------------------------------------
// D4D-B RADAR TARGET SOLUTION
//
// Fixed-size result derived only from:
//   - validated receiver reference coordinates
//   - a fresh validated target geographic position
//
// RSSI is never used for distance or bearing.
// ------------------------------------------------------------

struct WSRadarTargetSolution {

    bool valid = false;

    uint8_t targetIndex = 0;

    double distanceMeters = 0.0;

    double bearingTrueDegrees = 0.0;

    bool heightValid = false;

    float heightMeters = 0.0f;

    bool heightFromGround = false;
};


// ------------------------------------------------------------
// D4D-C NORTH-UP RADAR PLOT POINT
//
// Converts a valid geographic target solution into display
// coordinates. Screen Y increases downward, so north requires
// a negative Y offset.
//
// Targets beyond the selected display range are clamped to the
// outer ring and explicitly marked offScale.
// ------------------------------------------------------------

struct WSRadarPlotPoint {

    bool valid = false;

    int16_t x = 0;

    int16_t y = 0;

    bool offScale = false;

    double plottedDistanceMeters = 0.0;
};


bool wsProjectRadarTarget(
    const WSRadarTargetSolution &solution,
    int16_t centerX,
    int16_t centerY,
    int16_t radiusPixels,
    double rangeMeters,
    WSRadarPlotPoint &point
);


bool wsBuildRadarTargetSolution(
    uint8_t targetIndex,
    const WSUITargetSnapshot &snapshot,
    WSRadarTargetSolution &result
);


bool wsSolveGeographic(
    double receiverLatitude,
    double receiverLongitude,
    double targetLatitude,
    double targetLongitude,
    WSGeographicSolution &solution
);


WSReceiverReference wsReceiverReference;


// ============================================================
// D4D-C NORTH-UP RADAR COORDINATE PROJECTION
// ============================================================

bool wsProjectRadarTarget(
    const WSRadarTargetSolution &solution,
    int16_t centerX,
    int16_t centerY,
    int16_t radiusPixels,
    double rangeMeters,
    WSRadarPlotPoint &point
) {

    point =
        WSRadarPlotPoint();


    if (
        !solution.valid
        ||
        radiusPixels <= 0
        ||
        rangeMeters <= 0.0
        ||
        solution.distanceMeters < 0.0
        ||
        solution.bearingTrueDegrees < 0.0
        ||
        solution.bearingTrueDegrees >= 360.0
    ) {

        return false;
    }


    double plottedDistance =
        solution.distanceMeters;


    if (
        plottedDistance
        > rangeMeters
    ) {

        plottedDistance =
            rangeMeters;

        point.offScale =
            true;
    }


    const double radialFraction =
        plottedDistance
        /
        rangeMeters;


    const double radialPixels =
        radialFraction
        *
        static_cast<double>(
            radiusPixels
        );


    const double bearingRadians =
        wsDegreesToRadians(
            solution.bearingTrueDegrees
        );


    // North-up screen transform:
    //
    // 0 deg   -> up
    // 90 deg  -> right
    // 180 deg -> down
    // 270 deg -> left

    const double xOffset =
        sin(
            bearingRadians
        )
        *
        radialPixels;

    const double yOffset =
        -cos(
            bearingRadians
        )
        *
        radialPixels;


    point.x =
        static_cast<int16_t>(
            lround(
                static_cast<double>(
                    centerX
                )
                +
                xOffset
            )
        );


    point.y =
        static_cast<int16_t>(
            lround(
                static_cast<double>(
                    centerY
                )
                +
                yOffset
            )
        );


    point.plottedDistanceMeters =
        plottedDistance;

    point.valid =
        true;

    return true;
}


bool wsCoordinateValid(
    double latitude,
    double longitude
) {

    return (
        latitude >= -90.0
        &&
        latitude <= 90.0
        &&
        longitude >= -180.0
        &&
        longitude <= 180.0
    );
}


void wsClearReceiverReference() {

    wsReceiverReference.valid =
        false;

    wsReceiverReference.source =
        WS_REF_NONE;

    wsReceiverReference.latitude =
        0.0;

    wsReceiverReference.longitude =
        0.0;
}


bool wsSetManualFixedReceiverReference(
    double latitude,
    double longitude
) {

    if (
        !wsCoordinateValid(
            latitude,
            longitude
        )
    ) {

        wsClearReceiverReference();

        return false;
    }


    wsReceiverReference.latitude =
        latitude;

    wsReceiverReference.longitude =
        longitude;

    wsReceiverReference.source =
        WS_REF_MANUAL_FIXED;

    wsReceiverReference.valid =
        true;

    return true;
}


uint8_t wsReceiverReferenceStatusCode() {

    if (
        !wsReceiverReference.valid
        ||
        wsReceiverReference.source
            == WS_REF_NONE
    ) {

        return 0;
    }


    if (
        wsReceiverReference.source
            == WS_REF_MANUAL_FIXED
    ) {

        return 1;
    }


    if (
        wsReceiverReference.source
            == WS_REF_GNSS
    ) {

        return 2;
    }


    return 0;
}


double wsDegreesToRadians(
    double degrees
) {

    return (
        degrees
        * WS_GEO_PI
        / 180.0
    );
}


double wsRadiansToDegrees(
    double radians
) {

    return (
        radians
        * 180.0
        / WS_GEO_PI
    );
}


bool wsSolveGeographic(
    double receiverLatitude,
    double receiverLongitude,
    double targetLatitude,
    double targetLongitude,
    WSGeographicSolution &solution
) {

    solution.valid = false;

    solution.distanceMeters = 0.0;

    solution.bearingTrueDegrees = 0.0;


    if (
        !wsCoordinateValid(
            receiverLatitude,
            receiverLongitude
        )
        ||
        !wsCoordinateValid(
            targetLatitude,
            targetLongitude
        )
    ) {

        return false;
    }


    const double receiverLatRad =
        wsDegreesToRadians(
            receiverLatitude
        );

    const double targetLatRad =
        wsDegreesToRadians(
            targetLatitude
        );

    const double deltaLatRad =
        wsDegreesToRadians(
            targetLatitude
            - receiverLatitude
        );

    const double deltaLonRad =
        wsDegreesToRadians(
            targetLongitude
            - receiverLongitude
        );


    const double sinHalfDeltaLat =
        sin(
            deltaLatRad
            / 2.0
        );

    const double sinHalfDeltaLon =
        sin(
            deltaLonRad
            / 2.0
        );


    double haversineA =
        (
            sinHalfDeltaLat
            * sinHalfDeltaLat
        )
        +
        (
            cos(
                receiverLatRad
            )
            *
            cos(
                targetLatRad
            )
            *
            sinHalfDeltaLon
            *
            sinHalfDeltaLon
        );


    // Numerical protection near antipodal points.

    if (haversineA < 0.0) {

        haversineA = 0.0;
    }

    else if (haversineA > 1.0) {

        haversineA = 1.0;
    }


    const double centralAngle =
        2.0
        *
        atan2(
            sqrt(
                haversineA
            ),
            sqrt(
                1.0
                - haversineA
            )
        );


    solution.distanceMeters =
        WS_EARTH_RADIUS_METERS
        *
        centralAngle;


    const double bearingY =
        sin(
            deltaLonRad
        )
        *
        cos(
            targetLatRad
        );

    const double bearingX =
        (
            cos(
                receiverLatRad
            )
            *
            sin(
                targetLatRad
            )
        )
        -
        (
            sin(
                receiverLatRad
            )
            *
            cos(
                targetLatRad
            )
            *
            cos(
                deltaLonRad
            )
        );


    double bearingDegrees =
        wsRadiansToDegrees(
            atan2(
                bearingY,
                bearingX
            )
        );


    bearingDegrees =
        fmod(
            bearingDegrees
            + 360.0,
            360.0
        );


    solution.bearingTrueDegrees =
        bearingDegrees;

    solution.valid = true;

    return true;
}


// RADAR is intentionally unavailable at startup.
//
// Later this becomes data-driven using:
//   - valid receiver reference position
//   - fresh valid target geographic position
//   - any additional stability criteria we adopt.
//
// It must NEVER become ready from RSSI alone.

bool wsRadarReady = false;


// Geographic freshness is intentionally based on the timestamp
// of the most recent valid Location/Vector position update,
// NOT the timestamp of arbitrary RID traffic.
//
// Reuse the existing ACTIVE freshness interval so geographic
// readiness follows the same conservative 3-second freshness
// expectation already used elsewhere in the receiver.

const uint32_t WS_RADAR_POSITION_FRESH_MS =
    TARGET_ACTIVE_MS;


// ============================================================
// D4D-B PER-TARGET RADAR GEOGRAPHIC SOLUTION
// ============================================================

bool wsBuildRadarTargetSolution(
    uint8_t targetIndex,
    const WSUITargetSnapshot &snapshot,
    WSRadarTargetSolution &result
) {

    result =
        WSRadarTargetSolution();

    result.targetIndex =
        targetIndex;


    if (targetIndex >= MAX_TARGETS) {

        return false;
    }


    // Receiver reference must be explicitly valid and sourced.

    if (
        !wsReceiverReference.valid
        ||
        wsReceiverReference.source
            == WS_REF_NONE
        ||
        !wsCoordinateValid(
            wsReceiverReference.latitude,
            wsReceiverReference.longitude
        )
    ) {

        return false;
    }


    // Target must contain a real geographic position and a
    // position-specific freshness timestamp.

    if (
        !snapshot.allocated
        ||
        !snapshot.hasPosition
        ||
        snapshot.lastPositionSeenMs == 0
    ) {

        return false;
    }


    if (
        !wsCoordinateValid(
            snapshot.latitude,
            snapshot.longitude
        )
    ) {

        return false;
    }


    const uint32_t positionAge =
        millis()
        -
        snapshot.lastPositionSeenMs;


    if (
        positionAge
        > WS_RADAR_POSITION_FRESH_MS
    ) {

        return false;
    }


    // Reuse the already-validated D4A great-circle solver.

    WSGeographicSolution geographicSolution;


    if (
        !wsSolveGeographic(
            wsReceiverReference.latitude,
            wsReceiverReference.longitude,
            snapshot.latitude,
            snapshot.longitude,
            geographicSolution
        )
        ||
        !geographicSolution.valid
    ) {

        return false;
    }


    result.distanceMeters =
        geographicSolution.distanceMeters;

    result.bearingTrueDegrees =
        geographicSolution.bearingTrueDegrees;


    // Carry the existing OpenDroneID height semantics forward.
    //
    // heightFromGround == true  -> AGL
    // heightFromGround == false -> ATO

    result.heightValid =
        snapshot.heightValid;

    result.heightMeters =
        snapshot.height;

    result.heightFromGround =
        snapshot.heightFromGround;


    result.valid =
        true;

    return true;
}


bool wsEvaluateRadarReady() {

    // RADAR belongs to an operator-owned active capture session.

    if (
        !wsUiSessionStarted
        ||
        !scanSessionActive
    ) {

        return false;
    }


    // A reference must exist, identify its source, and contain
    // valid geographic coordinates.

    if (
        !wsReceiverReference.valid
        ||
        wsReceiverReference.source
            == WS_REF_NONE
        ||
        !wsCoordinateValid(
            wsReceiverReference.latitude,
            wsReceiverReference.longitude
        )
    ) {

        return false;
    }


    const uint32_t now =
        millis();

    bool freshGeographicTarget =
        false;


    if (targetMutex != nullptr) {

        xSemaphoreTake(
            targetMutex,
            portMAX_DELAY
        );
    }


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        const RIDTarget &target =
            targets[i];


        if (
            !target.allocated
            ||
            !target.hasPosition
            ||
            target.lastPositionSeenMs == 0
        ) {

            continue;
        }


        if (
            !wsCoordinateValid(
                target.latitude,
                target.longitude
            )
        ) {

            continue;
        }


        const uint32_t positionAge =
            now
            -
            target.lastPositionSeenMs;


        if (
            positionAge
            <= WS_RADAR_POSITION_FRESH_MS
        ) {

            freshGeographicTarget =
                true;

            break;
        }
    }


    if (targetMutex != nullptr) {

        xSemaphoreGive(
            targetMutex
        );
    }


    return freshGeographicTarget;
}


void serviceWaveshareRadarReadiness() {

    const bool readyNow =
        wsEvaluateRadarReady();


    if (
        readyNow
        == wsRadarReady
    ) {

        return;
    }


    wsRadarReady =
        readyNow;


    if (wsRadarReady) {

        Serial.println(
            "[RADAR] READY: valid receiver reference + fresh geographic target."
        );
    }

    else {

        Serial.println(
            "[RADAR] ACQUIRING: geographic prerequisites unavailable."
        );
    }


    // Update only the tab/header when readiness changes.
    // Do not disturb the live target panel or BLE receiver.

    if (
        wsCurrentScreen
        == WS_SCREEN_LIVE
    ) {

        wsDrawLiveTabs();
    }
}


uint8_t wsUiSelectedTarget = 0;


// ------------------------------------------------------------
// LIVE TARGET DATA REFRESH
//
// Keep display refresh independent from BLE packet arrival.
// Start conservatively at 2 Hz to preserve touch responsiveness
// and avoid unnecessary SPI traffic.
// ------------------------------------------------------------

const uint32_t WS_LIVE_TARGET_REFRESH_MS = 500;

uint32_t wsLastLiveTargetRefreshMs = 0;


// RADAR redraw remains independent from BLE packet arrival.
// Initial 500 m range is a provisional presentation scale,
// not a geographic or RF-derived estimate.

const uint32_t WS_RADAR_REFRESH_MS = 500;

uint32_t wsLastRadarRefreshMs = 0;

double wsRadarDisplayRangeMeters = 500.0;

// R6 RADAR-only layout. North-up, geographic solver and 500 m scale unchanged.
const int16_t WS_RADAR_CENTER_X = 133;
const int16_t WS_RADAR_CENTER_Y = 201;
const int16_t WS_RADAR_RADIUS_PX = 94;

// Fixed corner callouts keep slot labels separate, including co-located targets.
// Labels are NOT positions or touch controls. Leader lines end at true plot dots.
const int16_t WS_RADAR_LABEL_W = 44;
const int16_t WS_RADAR_LABEL_H = 22;
const int16_t WS_RADAR_LABEL_X[4] = {18, 205, 18, 205};
const int16_t WS_RADAR_LABEL_Y[4] = {98, 98, 282, 282};
static_assert(MAX_TARGETS == 4, "R6 callout layout requires the four-slot budget");

// R7 identity accents: RADAR corner callouts, plot markers and their leaders ONLY.
// Right-panel text, lifecycle/warnings, controls and all other screens stay R6.
// Stable per SLOT, not per aircraft across RESCAN. Geometry/draw order unchanged.
// RGB565: cyan, bright blue, violet, near-white light grey. No status colors.
static const uint16_t WS_RADAR_SLOT_COLORS[4] = {
    0x07FF,  // T01 cyan          (RGB888 design value #00FFFF)
    0x64FF,  // T02 bright blue   (RGB888 design value #609FFF)
    0xC41F,  // T03 violet        (RGB888 design value #C080FF)
    0xE71C   // T04 light grey    (RGB888 design value #E0E0E0)
};


// Frozen snapshot state.
//
// The live RID receiver continues updating targets[] while this
// fixed copy remains unchanged.

WSUITargetSnapshot wsFrozenSnapshot;

bool wsFrozenSnapshotCaptured = false;

uint8_t wsFrozenSnapshotTarget = 0;

OutputMode wsFrozenSnapshotMode =
    MODE_PRIVATE;

uint32_t wsFrozenSnapshotCapturedMs = 0;


WSButtonID wsTouchDownButton =
    WS_BTN_NONE;


// ------------------------------------------------------------
// COLORS
// ------------------------------------------------------------

const uint16_t WS_COLOR_DARK_GREY =
    0x2104;

const uint16_t WS_COLOR_GREY =
    0x8410;


// ------------------------------------------------------------
// HOME / PRIVACY BUTTONS
// ------------------------------------------------------------

const WSUIButton wsButtonBegin = {
    90, 125, 300, 78,
    "BEGIN",
    3
};


// R5 reference controls retain separate expanded touch areas.
const WSUIButton wsButtonUseGnss = {
    30, 130, 420, 50,
    "USE GNSS",
    2
};

const WSUIButton wsButtonTargetsOnly = {
    30, 194, 420, 50,
    "TARGETS ONLY",
    2
};


const WSUIButton wsButtonPrivate = {
    30, 82, 200, 72,
    "PRIVATE",
    2
};


const WSUIButton wsButtonOpen = {
    250, 82, 200, 72,
    "OPEN",
    2
};


const WSUIButton wsButtonStart = {
    110, 174, 260, 66,
    "START SCAN",
    2
};


const WSUIButton wsButtonBack = {
    20, 255, 130, 50,
    "BACK",
    2
};


// ------------------------------------------------------------
// LIVE VIEW TABS
// ------------------------------------------------------------

const WSUIButton wsButtonTargetsTab = {
    10, 48, 225, 46,
    "TARGETS",
    2
};


const WSUIButton wsButtonRadarTab = {
    245, 48, 225, 46,
    "RADAR",
    2
};


// R6: RADAR-specific visible geometry and matching hit regions.
// TARGETS keeps its original controls and geometry. Four-pixel expansion remains
// in wsPointInside(); the two bottom hit regions have a three-pixel dead strip.
const WSUIButton wsRadarTargetsTab = {10, 48, 225, 38, "TARGETS", 2};
const WSUIButton wsRadarRadarTab = {245, 48, 225, 38, "RADAR", 2};
const WSUIButton wsRadarModeButton = {267, 260, 95, 50, "MODE", 2};
const WSUIButton wsRadarStopButton = {374, 260, 96, 50, "STOP", 2};


// ------------------------------------------------------------
// TARGET SELECTORS
// ------------------------------------------------------------

const WSUIButton wsButtonT01 = {
    10, 106, 105, 46,
    "T01",
    2
};


const WSUIButton wsButtonT02 = {
    126, 106, 105, 46,
    "T02",
    2
};


const WSUIButton wsButtonT03 = {
    242, 106, 105, 46,
    "T03",
    2
};


const WSUIButton wsButtonT04 = {
    358, 106, 105, 46,
    "T04",
    2
};


// ------------------------------------------------------------
// LIVE BOTTOM CONTROLS
// ------------------------------------------------------------

const WSUIButton wsButtonSnapshot = {
    10, 260, 106, 50,
    "SNAPSHOT",
    2
};


const WSUIButton wsButtonRescan = {
    128, 260, 106, 50,
    "RESCAN",
    2
};


const WSUIButton wsButtonMode = {
    246, 260, 106, 50,
    "MODE",
    2
};


const WSUIButton wsButtonStop = {
    364, 260, 106, 50,
    "STOP",
    2
};


// Snapshot screen uses one deliberately large return control.

const WSUIButton wsButtonBackLive = {
    120, 260, 240, 50,
    "BACK TO LIVE",
    2
};


// ============================================================
// FIXED RID TARGET SNAPSHOT
// ============================================================

bool wsCaptureTargetSnapshot(
    uint8_t index,
    WSUITargetSnapshot &snapshot
) {

    snapshot =
        WSUITargetSnapshot();


    if (index >= MAX_TARGETS) {

        return false;
    }


    if (targetMutex != nullptr) {

        xSemaphoreTake(
            targetMutex,
            portMAX_DELAY
        );
    }


    RIDTarget &target =
        targets[index];


    snapshot.allocated =
        target.allocated;


    if (target.allocated) {

        uint32_t age =
            millis()
            - target.lastSeenMs;


        if (age <= TARGET_ACTIVE_MS) {

            snapshot.stateCode = 1;
        }

        else if (age <= TARGET_STALE_MS) {

            snapshot.stateCode = 2;
        }

        else {

            snapshot.stateCode = 3;
        }


        snapshot.uaType =
            target.uaType;

        snapshot.flightStatus =
            target.flightStatus;

        snapshot.protocolVersion =
            target.protocolVersion;


        snapshot.hasPosition =
            target.hasPosition;

        snapshot.latitude =
            target.latitude;

        snapshot.longitude =
            target.longitude;


        snapshot.directionValid =
            target.directionValid;

        snapshot.direction =
            target.direction;


        snapshot.horizontalSpeedValid =
            target.horizontalSpeedValid;

        snapshot.horizontalSpeed =
            target.horizontalSpeed;


        snapshot.verticalSpeedValid =
            target.verticalSpeedValid;

        snapshot.verticalSpeed =
            target.verticalSpeed;


        snapshot.barometricAltitudeValid =
            target.barometricAltitudeValid;

        snapshot.barometricAltitude =
            target.barometricAltitude;


        snapshot.geodeticAltitudeValid =
            target.geodeticAltitudeValid;

        snapshot.geodeticAltitude =
            target.geodeticAltitude;


        snapshot.heightValid =
            target.heightValid;

        snapshot.height =
            target.height;

        snapshot.heightFromGround =
            target.heightFromGround;


        snapshot.currentRSSI =
            target.currentRSSI;

        snapshot.filteredRSSI =
            getFilteredRSSI(
                target
            );


        snapshot.packetCount =
            target.packetCount;

        snapshot.basicIDCount =
            target.basicIDCount;

        snapshot.locationCount =
            target.locationCount;

        snapshot.systemCount =
            target.systemCount;

        snapshot.operatorIDCount =
            target.operatorIDCount;

        snapshot.lastSeenMs =
            target.lastSeenMs;

        snapshot.lastPositionSeenMs =
            target.lastPositionSeenMs;


        const char *idText =
            nullptr;


        if (target.hasSerialID) {

            idText =
                target.serialID.c_str();
        }

        else if (target.hasRegistrationID) {

            idText =
                target.registrationID.c_str();
        }

        else if (target.hasOperatorID) {

            idText =
                target.operatorID.c_str();
        }


        if (idText != nullptr) {

            snprintf(
                snapshot.displayID,
                sizeof(snapshot.displayID),
                "%s",
                idText
            );

            snapshot.displayIDValid =
                true;
        }
    }


    if (targetMutex != nullptr) {

        xSemaphoreGive(
            targetMutex
        );
    }


    return snapshot.allocated;
}


// ============================================================
// HIT TEST HELPER
// ============================================================

bool wsPointInside(
    int16_t x,
    int16_t y,
    const WSUIButton &button
) {

    // Small invisible expansion helps finger use without
    // creating excessive overlap between adjacent controls.

    const int16_t padding = 4;

    return (
        x >= button.x - padding
        && x <= button.x + button.w + padding
        && y >= button.y - padding
        && y <= button.y + button.h + padding
    );
}


// ============================================================
// GENERIC BUTTON DRAW
// ============================================================

void wsDrawButton(
    const WSUIButton &button,
    bool selected,
    uint16_t borderColor
) {

    uint16_t fillColor =
        selected
        ? CYAN
        : BLACK;

    uint16_t textColor =
        selected
        ? BLACK
        : WHITE;


    wsRenderFillRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        8,
        fillColor
    );


    wsGfx->drawRoundRect(
        button.x,
        button.y,
        button.w,
        button.h,
        8,
        selected
        ? CYAN
        : borderColor
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(
        button.textSize
    );

    wsGfx->setTextColor(
        textColor
    );


    int16_t textWidth =
        strlen(button.label)
        * 6
        * button.textSize;

    int16_t textHeight =
        8
        * button.textSize;


    wsGfx->setCursor(
        button.x
        + (button.w - textWidth) / 2,

        button.y
        + (button.h - textHeight) / 2
    );


    wsGfx->print(
        button.label
    );
    ridGnssRenderCheckpoint();
}


// ============================================================
// HEADER
// ============================================================

static void ridBatD4bRender(bool force);

void wsDrawHeader(
    const char *title,
    bool showMode
) {

    wsRenderFillRect(
        0,
        0,
        480,
        42,
        BLACK
    );


    wsGfx->drawLine(
        0,
        41,
        479,
        41,
        CYAN
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(2);
    wsGfx->setTextColor(WHITE);

    wsGfx->setCursor(
        10,
        12
    );

    wsGfx->print(
        title
    );
    ridGnssRenderCheckpoint();


    if (showMode) {

        wsGfx->setTextSize(1);

        wsGfx->setTextColor(
            wsUiPrivateSelected
            ? CYAN
            : YELLOW
        );

        wsGfx->setCursor(
            292,
            16
        );

        wsGfx->print(
            wsUiPrivateSelected
            ? "PRIVATE"
            : "OPEN"
        );
        ridGnssRenderCheckpoint();
    }
    ridBatD4bRender(true);

}


// ============================================================
// HOME SCREEN
// ============================================================


// ============================================================
// D4H: OPERATOR NOTICE
// ============================================================

void wsDrawBootNotice() {
    wsRenderFillScreen(BLACK);
    wsDrawHeader("RID-01 OPERATOR NOTICE", false);

    static const char *lines[] = {
        "RID-01 receives broadcast Remote ID",
        "information. Signals and positions",
        "may be delayed, missing, inaccurate",
        "or spoofed. Informational aid only;",
        "NOT a certified collision-avoidance",
        "system. Do not rely on it alone",
        "for flight safety decisions.",
        "BATTERY: Cell temperature is NOT",
        "monitored. Charge under supervision",
        "in safe conditions. Stop for abnormal",
        "heat, swelling, odor, or damage."
    };

    wsGfx->setTextSize(2);
    wsGfx->setTextColor(WHITE);

    for (uint8_t i = 0; i < 11; i++) {
        wsGfx->setTextColor(
            (i >= 7) ? YELLOW : WHITE
        );

        int16_t y =
            48 + i * 18 + ((i >= 7) ? 4 : 0);

        wsGfx->setCursor(18, y);
        wsGfx->print(lines[i]);
        ridGnssRenderCheckpoint();
    }

    wsRenderFillRoundRect(
        54, 262, 372, 45, 8, CYAN
    );

    wsGfx->setTextSize(2);
    wsGfx->setTextColor(BLACK);
    wsGfx->setCursor(108, 276);
    wsGfx->print("ACKNOWLEDGE & CONTINUE");
    ridGnssRenderCheckpoint();
}

void wsDrawHomeScreen() {

    wsRenderFillScreen(
        BLACK
    );


    wsDrawHeader(
        "REMOTE ID HANDHELD",
        false
    );


    wsGfx->setTextSize(3);
    wsGfx->setTextColor(CYAN);

    wsGfx->setCursor(
        111,
        73
    );

    wsGfx->print(
        "RID SENSOR"
    );
    ridGnssRenderCheckpoint();


    wsDrawButton(
        wsButtonBegin,
        false,
        WHITE
    );


    wsGfx->setTextSize(2);
    wsGfx->setTextColor(GREEN);

    wsGfx->setCursor(
        155,
        222
    );

    wsGfx->print(
        "SYSTEM READY"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(1);
    wsGfx->setTextColor(WHITE);

    wsGfx->setCursor(
        158,
        290
    );

    wsGfx->print(
        "STAGE 1B CONTROL"
    );
    ridGnssRenderCheckpoint();
}


// ============================================================
// R5 ON-DEVICE RECEIVER REFERENCE SELECTION
// ============================================================

const char *wsReferenceFixName(uint8_t status) {
    switch (status) {
        case WS_GNSS_FIX_VALID: return "VALID";
        case WS_GNSS_FIX_STALE: return "STALE";
        case WS_GNSS_FIX_UNAVAILABLE: return "UNAVAILABLE";
        default: return "ACQUIRING";
    }
}

void wsDrawReferenceStatus(uint8_t status) {
    wsRenderFillRect(28, 58, 424, 26, BLACK);
    wsGfx->setTextSize(2);
    wsGfx->setTextColor(status == WS_GNSS_FIX_VALID ? GREEN :
        (status == WS_GNSS_FIX_UNAVAILABLE ? RED : YELLOW));
    wsGfx->setCursor(30, 62);
    wsGfx->print("GNSS FIX: ");
    ridGnssRenderCheckpoint();
    wsGfx->print(wsReferenceFixName(status));
    ridGnssRenderCheckpoint();
    wsReferenceStatusDrawn = status;
}

void wsDrawReferenceScreen() {
    wsRenderFillScreen(BLACK);
    wsDrawHeader("SELECT REFERENCE", false);
    wsReferenceStatusDrawn = 0xffU;
    wsDrawReferenceStatus(ridGnssUiStatusCode());
    wsLastReferenceRefreshMs = millis();

    wsGfx->setTextSize(1);
    wsGfx->setTextColor(WHITE);
    wsGfx->setCursor(30, 94);
    wsGfx->print("Use onboard GNSS for RADAR position.");
    ridGnssRenderCheckpoint();
    wsGfx->setCursor(30, 108);
    wsGfx->print("TARGETS can scan while GNSS acquires.");
    ridGnssRenderCheckpoint();

    wsDrawButton(wsButtonUseGnss, false, GREEN);
    wsDrawButton(wsButtonTargetsOnly, false, WHITE);
    wsDrawButton(wsButtonBack, false, WHITE);

    wsGfx->setTextSize(1);
    wsGfx->setTextColor(WS_COLOR_GREY);
    wsGfx->setCursor(178, 276);
    wsGfx->print("RADAR also needs a fresh");
    ridGnssRenderCheckpoint();
    wsGfx->setCursor(178, 288);
    wsGfx->print("target position.");
    ridGnssRenderCheckpoint();
}

void serviceWaveshareReferenceRefresh() {
    if (wsCurrentScreen != WS_SCREEN_REFERENCE) return;
    const uint32_t now = millis();
    if ((uint32_t)(now - wsLastReferenceRefreshMs) < WS_REFERENCE_REFRESH_MS) return;
    wsLastReferenceRefreshMs = now;
    const uint8_t status = ridGnssUiStatusCode();
    if (status != wsReferenceStatusDrawn) {
        wsDrawReferenceStatus(status);
    }
}


// ============================================================
// OUTPUT MODE SCREEN
// ============================================================

void wsDrawPrivacyScreen() {

    wsRenderFillScreen(
        BLACK
    );


    wsDrawHeader(
        "SELECT OUTPUT MODE",
        false
    );


    wsGfx->setTextSize(1);
    wsGfx->setTextColor(YELLOW);

    wsGfx->setCursor(
        128,
        57
    );

    wsGfx->print(
        "PRIVATE IS DEFAULT"
    );
    ridGnssRenderCheckpoint();


    wsDrawButton(
        wsButtonPrivate,
        wsUiPrivateSelected,
        WHITE
    );


    wsDrawButton(
        wsButtonOpen,
        !wsUiPrivateSelected,
        WHITE
    );


    wsDrawButton(
        wsButtonStart,
        false,
        GREEN
    );


    wsDrawButton(
        wsButtonBack,
        false,
        WHITE
    );


    wsGfx->setTextSize(1);
    wsGfx->setTextColor(WS_COLOR_GREY);

    wsGfx->setCursor(
        178,
        287
    );

    wsGfx->print(
        "SELECT MODE THEN START"
    );
    ridGnssRenderCheckpoint();
}


// ============================================================
// LIVE TAB BAR
// ============================================================

void wsDrawLiveTabs() {

    // R6: compact RADAR tab drawings and hit regions agree.
    // TARGETS retains its original taller tabs.

    const int16_t wsLiveTabDrawHeight =
        (
            wsCurrentLiveView
            == WS_VIEW_RADAR
        )
        ? wsRadarTargetsTab.h
        : wsButtonTargetsTab.h;


    // Built-in text geometry used by the existing UI:
    // text size 2 = approximately 12 px wide x 16 px tall
    // per character; text size 1 = approximately 6 x 8.
    //
    // RADAR uses the matching compact tab controls in wsHitTest().

    const int16_t wsTargetsTextX =
        wsButtonTargetsTab.x
        +
        (
            wsButtonTargetsTab.w
            -
            84
        )
        /
        2;

    const int16_t wsTargetsTextY =
        wsButtonTargetsTab.y
        +
        (
            wsLiveTabDrawHeight
            -
            16
        )
        /
        2;


    // RADAR uses a two-line centered stack:
    // 16 px title + 2 px gap + 8 px status.

    const int16_t wsRadarTitleX =
        wsButtonRadarTab.x
        +
        (
            wsButtonRadarTab.w
            -
            60
        )
        /
        2;

    const int16_t wsRadarTitleY =
        wsButtonRadarTab.y
        +
        (
            wsLiveTabDrawHeight
            -
            26
        )
        /
        2;

    const int16_t wsRadarStatusY =
        wsRadarTitleY
        +
        18;


    // TARGETS TAB

    bool targetsSelected =
        wsCurrentLiveView
        == WS_VIEW_TARGETS;


    wsRenderFillRoundRect(
        wsButtonTargetsTab.x,
        wsButtonTargetsTab.y,
        wsButtonTargetsTab.w,
        wsLiveTabDrawHeight,
        8,
        targetsSelected
        ? CYAN
        : BLACK
    );


    wsGfx->drawRoundRect(
        wsButtonTargetsTab.x,
        wsButtonTargetsTab.y,
        wsButtonTargetsTab.w,
        wsLiveTabDrawHeight,
        8,
        CYAN
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(2);

    wsGfx->setTextColor(
        targetsSelected
        ? BLACK
        : WHITE
    );


    wsGfx->setCursor(
        wsTargetsTextX,
        wsTargetsTextY
    );

    wsGfx->print(
        "TARGETS"
    );
    ridGnssRenderCheckpoint();


    // RADAR TAB
    //
    // Grey = acquiring / unavailable.
    // Green = geographic prerequisites satisfied.

    uint16_t radarFill =
        wsRadarReady
        ? (
            wsCurrentLiveView == WS_VIEW_RADAR
            ? GREEN
            : BLACK
        )
        : WS_COLOR_DARK_GREY;


    uint16_t radarBorder =
        wsRadarReady
        ? GREEN
        : WS_COLOR_GREY;


    wsRenderFillRoundRect(
        wsButtonRadarTab.x,
        wsButtonRadarTab.y,
        wsButtonRadarTab.w,
        wsLiveTabDrawHeight,
        8,
        radarFill
    );


    wsGfx->drawRoundRect(
        wsButtonRadarTab.x,
        wsButtonRadarTab.y,
        wsButtonRadarTab.w,
        wsLiveTabDrawHeight,
        8,
        radarBorder
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(2);

    wsGfx->setTextColor(
        wsRadarReady
        && wsCurrentLiveView == WS_VIEW_RADAR
        ? BLACK
        : WHITE
    );


    wsGfx->setCursor(
        wsRadarTitleX,
        wsRadarTitleY
    );

    wsGfx->print(
        "RADAR"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(1);

    wsGfx->setTextColor(
        wsRadarReady
        ? (
            wsCurrentLiveView == WS_VIEW_RADAR
            ? BLACK
            : GREEN
        )
        : WS_COLOR_GREY
    );


    wsGfx->setCursor(
        wsRadarReady
        ? (
            wsButtonRadarTab.x
            +
            (
                wsButtonRadarTab.w
                -
                30
            )
            /
            2
        )
        : (
            wsButtonRadarTab.x
            +
            (
                wsButtonRadarTab.w
                -
                54
            )
            /
            2
        ),
        wsRadarStatusY
    );


    wsGfx->print(
        wsRadarReady
        ? "READY"
        : "ACQUIRING"
    );
    ridGnssRenderCheckpoint();
}


// ============================================================
// TARGETS VIEW
// ============================================================

void wsDrawLiveTargetDataPanel() {

    WSUITargetSnapshot snapshot;

    wsCaptureTargetSnapshot(
        wsUiSelectedTarget,
        snapshot
    );


    // --------------------------------------------------------
    // Lifecycle color.
    // --------------------------------------------------------

    uint16_t stateColor =
        WS_COLOR_GREY;


    if (snapshot.allocated) {

        if (snapshot.stateCode == 1) {

            stateColor =
                GREEN;
        }

        else if (snapshot.stateCode == 2) {

            stateColor =
                YELLOW;
        }

        else if (snapshot.stateCode == 3) {

            stateColor =
                RED;
        }
    }


    // --------------------------------------------------------
    // Clear ONLY the live target-data region.
    // --------------------------------------------------------

    wsRenderFillRoundRect(
        10,
        162,
        460,
        86,
        8,
        BLACK
    );


    wsGfx->drawRoundRect(
        10,
        162,
        460,
        86,
        8,
        stateColor
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // Lifecycle header.
    //
    // Target identity is already communicated by the selected
    // T01-T04 button directly above this panel.
    // --------------------------------------------------------

    wsGfx->setTextSize(2);

    wsGfx->setTextColor(
        stateColor
    );

    wsGfx->setCursor(
        24,
        169
    );

    wsGfx->print(
        wsSnapshotStateName(
            snapshot
        )
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // EMPTY target.
    // --------------------------------------------------------

    if (!snapshot.allocated) {

        wsGfx->setTextSize(1);

        wsGfx->setTextColor(
            WHITE
        );

        wsGfx->setCursor(
            24,
            198
        );

        wsGfx->print(
            "NO ALLOCATED TARGET"
        );
        ridGnssRenderCheckpoint();


        wsGfx->setTextColor(
            WS_COLOR_GREY
        );

        wsGfx->setCursor(
            24,
            218
        );

        wsGfx->print(
            "LIVE RECEIVER CONTINUES"
        );
        ridGnssRenderCheckpoint();

        return;
    }


    // --------------------------------------------------------
    // RSSI - upper-right status field.
    // --------------------------------------------------------

    wsGfx->setTextSize(1);

    wsGfx->setTextColor(
        WHITE
    );

    wsGfx->setCursor(
        350,
        174
    );

    wsGfx->print(
        "RSSI:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        snapshot.filteredRSSI
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // TYPE / FLIGHT / PROTOCOL
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        191
    );

    wsGfx->print(
        "TYPE:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        uaTypeName(
            snapshot.uaType
        )
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "  FLT:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        flightStatusName(
            snapshot.flightStatus
        )
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "  ODID v"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        snapshot.protocolVersion
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // IDENTIFIER
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        205
    );

    wsGfx->print(
        "ID:"
    );
    ridGnssRenderCheckpoint();


    if (!snapshot.displayIDValid) {

        wsGfx->print(
            "<not provided>"
        );
        ridGnssRenderCheckpoint();
    }

    else if (
        outputMode == MODE_PRIVATE
    ) {

        wsGfx->print(
            "[REDACTED]"
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            snapshot.displayID
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // POSITION
    //
    // Explicit LAT/LON labels avoid visually merging the comma
    // separator with a negative longitude value.
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        219
    );


    if (!snapshot.hasPosition) {

        wsGfx->print(
            "LAT:UNKNOWN   LON:UNKNOWN"
        );
        ridGnssRenderCheckpoint();
    }

    else if (
        outputMode == MODE_PRIVATE
    ) {

        wsGfx->print(
            "LAT:[REDACTED]   LON:[REDACTED]"
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "LAT:"
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            snapshot.latitude,
            5
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            "   LON:"
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            snapshot.longitude,
            5
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // MOTION / ALTITUDE / PACKET COUNT
    //
    // Compact horizontal instrument row:
    // B/G/R = barometric / geodetic / relative altitude.
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        233
    );

    wsGfx->print(
        "HDG:"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.directionValid) {

        wsGfx->print(
            snapshot.direction,
            0
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        " HSPD:"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.horizontalSpeedValid) {

        wsGfx->print(
            snapshot.horizontalSpeed,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        " VSPD:"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.verticalSpeedValid) {

        wsGfx->print(
            snapshot.verticalSpeed,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        " B/G/R:"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.barometricAltitudeValid) {

        wsGfx->print(
            snapshot.barometricAltitude,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "/"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.geodeticAltitudeValid) {

        wsGfx->print(
            snapshot.geodeticAltitude,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "/"
    );
    ridGnssRenderCheckpoint();


    if (snapshot.heightValid) {

        wsGfx->print(
            snapshot.height,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        " PKTS:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        snapshot.packetCount
    );
    ridGnssRenderCheckpoint();
}


// ============================================================
// TARGETS VIEW
// ============================================================

void wsDrawTargetsView() {

    wsDrawButton(
        wsButtonT01,
        wsUiSelectedTarget == 0,
        WHITE
    );


    wsDrawButton(
        wsButtonT02,
        wsUiSelectedTarget == 1,
        WHITE
    );


    wsDrawButton(
        wsButtonT03,
        wsUiSelectedTarget == 2,
        WHITE
    );


    wsDrawButton(
        wsButtonT04,
        wsUiSelectedTarget == 3,
        WHITE
    );


    wsDrawLiveTargetDataPanel();


    // Full TARGETS redraw just refreshed the panel.
    wsLastLiveTargetRefreshMs =
        millis();
}


// ============================================================
// WAVESHARE LIVE TARGET REFRESH SERVICE
//
// Refresh ONLY the selected-target data panel.
// BLE capture and touch handling remain independent.
// ============================================================

void serviceWaveshareLiveTargetRefresh() {

    if (
        wsCurrentScreen
            != WS_SCREEN_LIVE

        || wsCurrentLiveView
            != WS_VIEW_TARGETS
    ) {

        return;
    }


    uint32_t now =
        millis();


    if (
        now
        -
        wsLastLiveTargetRefreshMs
        <
        WS_LIVE_TARGET_REFRESH_MS
    ) {

        return;
    }


    wsLastLiveTargetRefreshMs =
        now;


    wsDrawLiveTargetDataPanel();
}


// ============================================================
// RADAR VIEW
// ============================================================

void wsDrawRadarView() {

    // ========================================================
    // R6 RADAR READABILITY — presentation only
    //
    // LEFT:
    //   Large north-up geographic scope.
    //
    // RIGHT:
    //   Per-target geographic details.
    //
    // MODE and STOP remain 50 px tall; RADAR-only widths are 95/96 px.
    // Fixed corner callouts never displace target dots. The stored geographic
    // solution and display range are unchanged. Exactly co-located dots may
    // still coincide; distinct labels do not imply spatial separation.
    //
    // SNAPSHOT and RESCAN belong to TARGETS view and are not
    // rendered here.
    // ========================================================


    // --------------------------------------------------------
    // LEFT RADAR PANEL
    // --------------------------------------------------------

    wsRenderFillRect(
        10,
        92,
        247,
        218,
        BLACK
    );


    wsGfx->drawRoundRect(
        10,
        92,
        247,
        218,
        8,
        GREEN
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // RIGHT TARGET-DATA PANEL
    //
    // Stops above MODE / STOP so periodic RADAR refreshes do
    // not erase those controls.
    // --------------------------------------------------------

    wsRenderFillRect(
        267,
        96,
        203,
        154,
        BLACK
    );


    wsGfx->drawRoundRect(
        267,
        96,
        203,
        154,
        8,
        GREEN
    );
    ridGnssRenderCheckpoint();


    const int16_t cx =
        WS_RADAR_CENTER_X;

    const int16_t cy =
        WS_RADAR_CENTER_Y;

    const int16_t radius =
        WS_RADAR_RADIUS_PX;


    // --------------------------------------------------------
    // Geographic scope
    // --------------------------------------------------------

    // --------------------------------------------------------
    // Subdued quadrant reference lines
    //
    // Drawn BEFORE the range rings so they remain background
    // structure rather than competing with target/range data.
    // --------------------------------------------------------

    wsGfx->drawLine(
        cx - radius,
        cy,
        cx + radius,
        cy,
        WS_COLOR_DARK_GREY
    );
    ridGnssRenderCheckpoint();


    wsGfx->drawLine(
        cx,
        cy - radius,
        cx,
        cy + radius,
        WS_COLOR_DARK_GREY
    );
    ridGnssRenderCheckpoint();


    // Proportional range rings:
    //
    //   inner  = 1/3 displayed range
    //   middle = 2/3 displayed range
    //   outer  = full displayed range
    //
    // No distance labels are placed on the scope.
    // RANGE defines the outer-ring scale.

    wsGfx->drawCircle(
        cx,
        cy,
        radius / 3,
        WS_COLOR_GREY
    );
    ridGnssRenderCheckpoint();


    wsGfx->drawCircle(
        cx,
        cy,
        (radius * 2) / 3,
        WS_COLOR_GREY
    );
    ridGnssRenderCheckpoint();


    wsGfx->drawCircle(
        cx,
        cy,
        radius,
        WHITE
    );
    ridGnssRenderCheckpoint();


    // Receiver marker is drawn after leader lines below, so lines do not
    // overwrite marker centers.


    // --------------------------------------------------------
    // Cardinal labels
    // --------------------------------------------------------

    wsGfx->setTextSize(1);
    wsGfx->setTextColor(GREEN);


    wsGfx->setCursor(
        cx - 3,
        cy - radius - 10
    );

    wsGfx->print(
        "N"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setCursor(
        cx + radius + 6,
        cy - 14
    );

    wsGfx->print(
        "E"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setCursor(
        cx - 3,
        cy + radius + 3
    );

    wsGfx->print(
        "S"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setCursor(
        cx - radius - 12,
        cy - 14
    );

    wsGfx->print(
        "W"
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // Right-panel fixed status fields.
    // --------------------------------------------------------

    wsGfx->setTextColor(
        WHITE
    );


    wsGfx->setCursor(
        277,
        103
    );

    wsGfx->print(
        "RANGE:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsRadarDisplayRangeMeters,
        0
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "m"
    );
    ridGnssRenderCheckpoint();


    wsGfx->setCursor(
        365,
        103
    );

    wsGfx->print(
        "REF:"
    );
    ridGnssRenderCheckpoint();


    if (
        wsReceiverReference.valid
        &&
        wsReceiverReference.source
            == WS_REF_MANUAL_FIXED
    ) {

        wsGfx->print(
            "FIXED"
        );
        ridGnssRenderCheckpoint();
    }

    else if (
        wsReceiverReference.valid
        &&
        wsReceiverReference.source
            == WS_REF_GNSS
    ) {

        wsGfx->print(
            "GNSS"
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "NONE"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->drawLine(
        277,
        117,
        460,
        117,
        WS_COLOR_DARK_GREY
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // Build, project and display each target.
    // --------------------------------------------------------

    uint8_t plottedTargetCount =
        0;

    // Bounded stack-only render state. Snapshot locks have been released before
    // any graphics call; no new task, heap allocation or UART reader is added.
    WSRadarPlotPoint plotPoints[MAX_TARGETS];
    uint16_t plotColors[MAX_TARGETS] = {};


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        const int16_t rowY =
            125
            +
            static_cast<int16_t>(i) * 27;


        WSUITargetSnapshot snapshot;


        bool snapshotValid =
            wsCaptureTargetSnapshot(
                i,
                snapshot
            );


        // ----------------------------------------------------
        // Production visibility rule:
        //
        // A slot that has never been allocated does not
        // represent a real detected target and must remain
        // completely invisible in RADAR view.
        //
        // Allocated targets continue below, where a temporary
        // lack of fresh geographic data is shown truthfully as
        // NO FRESH GEO.
        // ----------------------------------------------------

        if (
            !snapshotValid
            ||
            !snapshot.allocated
        ) {

            continue;
        }


        // ----------------------------------------------------
        // Production lifecycle visibility rule:
        //
        // ACTIVE:
        //   Plot when geographic data is fresh.
        //
        // STALE:
        //   Keep the real target identity visible, but do not
        //   plot stale geographic coordinates. The existing
        //   NO FRESH GEO path handles this truthfully.
        //
        // LOST:
        //   Remove the target from RADAR presentation entirely.
        //   The backend slot remains untouched and may still be
        //   reacquired/reused according to existing lifecycle
        //   logic.
        // ----------------------------------------------------

        if (snapshot.stateCode == 3) {

            continue;
        }


        // ----------------------------------------------------
        // RADAR right-panel lifecycle label.
        // UI-only: use existing backend state.
        // No change to geographic plotting or lifecycle.
        // ----------------------------------------------------

        if (
            snapshot.stateCode == 1
            ||
            snapshot.stateCode == 2
        ) {

            wsGfx->setTextColor(
                snapshot.stateCode == 1
                ? GREEN
                : YELLOW
            );

            wsGfx->setCursor(
                424,
                rowY
            );

            wsGfx->print(
                snapshot.stateCode == 1
                ? "ACTIVE"
                : "STALE"
            );
            ridGnssRenderCheckpoint();
        }


        WSRadarTargetSolution solution;


        bool solutionValid =
            wsBuildRadarTargetSolution(
                i,
                snapshot,
                solution
            );


        if (!solutionValid) {

            wsGfx->setTextColor(
                WS_COLOR_GREY
            );

            wsGfx->setCursor(
                277,
                rowY
            );

            wsGfx->print(
                "T0"
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                i + 1
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                "  NO FRESH GEO"
            );
            ridGnssRenderCheckpoint();


            wsGfx->setCursor(
                277,
                rowY + 11
            );

            wsGfx->print(
                "DIST/BRG/HGT --"
            );
            ridGnssRenderCheckpoint();

            continue;
        }


        WSRadarPlotPoint point;


        if (
            !wsProjectRadarTarget(
                solution,
                cx,
                cy,
                radius,
                wsRadarDisplayRangeMeters,
                point
            )
        ) {

            wsGfx->setTextColor(
                WS_COLOR_GREY
            );

            wsGfx->setCursor(
                277,
                rowY
            );

            wsGfx->print(
                "T0"
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                i + 1
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                "  PLOT ERROR"
            );
            ridGnssRenderCheckpoint();

            continue;
        }


        plottedTargetCount++;


        const uint16_t targetColor =
            point.offScale
            ? YELLOW
            : CYAN;


        // Defer dots and labels until every target has a projected point.
        // This also lets us draw leaders first and markers on top of them.
        plotPoints[i] = point;
        plotColors[i] = WS_RADAR_SLOT_COLORS[i];
        wsGfx->setTextColor(targetColor);


        // ----------------------------------------------------
        // Right-side target information
        //
        // Line 1:
        //   Target ID / actual distance / true bearing
        //
        // Line 2:
        //   truthful AGL or ATO height in feet when available
        // ----------------------------------------------------

        const long distanceMetersRounded =
            lround(
                solution.distanceMeters
            );


        int bearingRounded =
            static_cast<int>(
                lround(
                    solution.bearingTrueDegrees
                )
            );


        if (
            bearingRounded
            >= 360
        ) {

            bearingRounded -=
                360;
        }


        wsGfx->setCursor(
            277,
            rowY
        );

        wsGfx->print(
            "T0"
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            i + 1
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            " "
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            distanceMetersRounded
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            "m B"
        );
        ridGnssRenderCheckpoint();


        if (
            bearingRounded
            < 100
        ) {

            wsGfx->print(
                "0"
            );
            ridGnssRenderCheckpoint();
        }


        if (
            bearingRounded
            < 10
        ) {

            wsGfx->print(
                "0"
            );
            ridGnssRenderCheckpoint();
        }


        wsGfx->print(
            bearingRounded
        );
        ridGnssRenderCheckpoint();


        wsGfx->setCursor(
            277,
            rowY + 11
        );


        if (
            solution.heightValid
        ) {

            const long heightFeet =
                lround(
                    static_cast<double>(
                        solution.heightMeters
                    )
                    *
                    3.28084
                );


            wsGfx->print(
                solution.heightFromGround
                ? "AGL "
                : "ATO "
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                heightFeet
            );
            ridGnssRenderCheckpoint();

            wsGfx->print(
                "ft"
            );
            ridGnssRenderCheckpoint();
        }

        else {

            wsGfx->setTextColor(
                WS_COLOR_GREY
            );

            wsGfx->print(
                "HGT UNKNOWN"
            );
            ridGnssRenderCheckpoint();
        }

        if (point.offScale) {
            wsGfx->setTextColor(YELLOW);
            wsGfx->setCursor(424, rowY + 11);
            wsGfx->print("OFF");
            ridGnssRenderCheckpoint();
        }
    }


    // Draw leaders BEFORE dots and labels. Corner boxes lie outside the full
    // geographic circle (including four-pixel markers), and never overlap.
    // Fixed slot placement avoids label jumping when nearby GNSS points jitter.
    for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
        if (!plotPoints[i].valid) continue;
        const int16_t anchorX = WS_RADAR_LABEL_X[i] +
            ((i % 2 == 0) ? WS_RADAR_LABEL_W - 1 : 0);
        const int16_t anchorY = WS_RADAR_LABEL_Y[i] +
            ((i < 2) ? WS_RADAR_LABEL_H - 1 : 0);
        wsGfx->drawLine(plotPoints[i].x, plotPoints[i].y,
                       anchorX, anchorY, plotColors[i]);
        ridGnssRenderCheckpoint();
    }

    wsGfx->fillCircle(cx, cy, 4, WHITE);
    ridGnssRenderCheckpoint();
    for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
        if (!plotPoints[i].valid) continue;
        wsGfx->fillCircle(plotPoints[i].x, plotPoints[i].y, 4, plotColors[i]);
        ridGnssRenderCheckpoint();
    }

    for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
        if (!plotPoints[i].valid) continue;
        const int16_t lx = WS_RADAR_LABEL_X[i];
        const int16_t ly = WS_RADAR_LABEL_Y[i];
        wsRenderFillRoundRect(lx, ly, WS_RADAR_LABEL_W, WS_RADAR_LABEL_H,
                             4, BLACK);
        wsGfx->drawRoundRect(lx, ly, WS_RADAR_LABEL_W, WS_RADAR_LABEL_H,
                            4, plotColors[i]);
        ridGnssRenderCheckpoint();
        wsGfx->setTextSize(2);
        wsGfx->setTextColor(plotColors[i]);
        wsGfx->setCursor(lx + 4, ly + 3);
        wsGfx->print("T0");
        ridGnssRenderCheckpoint();
        wsGfx->print(i + 1);
        ridGnssRenderCheckpoint();
    }
    wsGfx->setTextSize(1);


    // --------------------------------------------------------
    // Plot count / orientation
    // --------------------------------------------------------

    wsGfx->setTextColor(
        WHITE
    );

    wsGfx->setCursor(
        277,
        229
    );

    wsGfx->print(
        "PLOTS:"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        plottedTargetCount
    );
    ridGnssRenderCheckpoint();


    wsGfx->setCursor(
        418,
        229
    );

    wsGfx->print(
        "N-UP"
    );
    ridGnssRenderCheckpoint();


    wsLastRadarRefreshMs =
        millis();
}


// ============================================================
// WAVESHARE LIVE RADAR REFRESH SERVICE
//
// Refresh only the RADAR content region.
// BLE capture, TARGETS view and touch remain independent.
// ============================================================

void serviceWaveshareRadarRefresh() {

    if (
        wsCurrentScreen
            != WS_SCREEN_LIVE

        ||
        wsCurrentLiveView
            != WS_VIEW_RADAR
    ) {

        return;
    }


    const uint32_t now =
        millis();


    if (
        now
        -
        wsLastRadarRefreshMs
        <
        WS_RADAR_REFRESH_MS
    ) {

        return;
    }


    wsLastRadarRefreshMs =
        now;


    wsDrawRadarView();
}


// ============================================================
// LIVE SCREEN
// ============================================================

void wsDrawLiveScreen() {

    wsRenderFillScreen(
        BLACK
    );


    wsDrawHeader(
        "RID LIVE",
        true
    );


    wsDrawLiveTabs();


    if (
        wsCurrentLiveView
        == WS_VIEW_TARGETS
    ) {

        wsDrawTargetsView();
    }

    else {

        wsDrawRadarView();
    }


    if (
        wsCurrentLiveView
        == WS_VIEW_TARGETS
    ) {

        wsDrawButton(
            wsButtonSnapshot,
            false,
            WHITE
        );


        wsDrawButton(
            wsButtonRescan,
            false,
            WHITE
        );
    }


    // MODE and STOP remain available from both live views.

    wsDrawButton(
        wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarModeButton : wsButtonMode,
        false,
        YELLOW
    );


    wsDrawButton(
        wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarStopButton : wsButtonStop,
        false,
        RED
    );
}


// ============================================================
// SNAPSHOT SCREEN
//
// Renders ONLY the frozen fixed-size snapshot captured by D1.
// The live BLE receiver continues running underneath this view.
// ============================================================

const char *wsSnapshotStateName(
    const WSUITargetSnapshot &snapshot
) {

    switch (snapshot.stateCode) {

        case 1:
            return "ACTIVE";

        case 2:
            return "STALE";

        case 3:
            return "LOST";

        default:
            return "EMPTY";
    }
}


void wsDrawSnapshotScreen() {

    wsRenderFillScreen(
        BLACK
    );


    // Do NOT use the live mode indicator here.
    // Snapshot privacy belongs to the frozen capture itself.

    wsDrawHeader(
        "TARGET SNAPSHOT",
        false
    );


    bool privateSnapshot =
        wsFrozenSnapshotMode
        == MODE_PRIVATE;


    wsGfx->setTextSize(1);

    wsGfx->setTextColor(
        privateSnapshot
        ? CYAN
        : YELLOW
    );

    wsGfx->setCursor(
        privateSnapshot
        ? 420
        : 438,
        16
    );

    wsGfx->print(
        privateSnapshot
        ? "PRIVATE"
        : "OPEN"
    );
    ridGnssRenderCheckpoint();


    uint16_t stateColor =
        WS_COLOR_GREY;


    switch (
        wsFrozenSnapshot.stateCode
    ) {

        case 1:
            stateColor = GREEN;
            break;

        case 2:
            stateColor = YELLOW;
            break;

        case 3:
            stateColor = RED;
            break;

        default:
            stateColor = WS_COLOR_GREY;
            break;
    }


    wsGfx->drawRoundRect(
        10,
        50,
        460,
        198,
        8,
        stateColor
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextSize(2);
    wsGfx->setTextColor(
        stateColor
    );

    wsGfx->setCursor(
        24,
        62
    );

    wsGfx->print(
        "T0"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsFrozenSnapshotTarget + 1
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "  "
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsSnapshotStateName(
            wsFrozenSnapshot
        )
    );
    ridGnssRenderCheckpoint();


    if (
        !wsFrozenSnapshotCaptured
        || !wsFrozenSnapshot.allocated
    ) {

        wsGfx->setTextSize(2);
        wsGfx->setTextColor(YELLOW);

        wsGfx->setCursor(
            74,
            112
        );

        wsGfx->print(
            "NO ALLOCATED TARGET"
        );
        ridGnssRenderCheckpoint();


        wsGfx->setTextSize(1);
        wsGfx->setTextColor(WHITE);

        wsGfx->setCursor(
            116,
            151
        );

        wsGfx->print(
            "EMPTY SNAPSHOT - RECEIVER STILL LIVE"
        );
        ridGnssRenderCheckpoint();


        wsDrawButton(
            wsButtonBackLive,
            false,
            CYAN
        );

        return;
    }


    wsGfx->setTextSize(1);
    wsGfx->setTextColor(WHITE);


    // --------------------------------------------------------
    // TYPE / FLIGHT / RSSI
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        93
    );

    wsGfx->print(
        "TYPE: "
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        uaTypeName(
            wsFrozenSnapshot.uaType
        )
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "   FLIGHT: "
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        flightStatusName(
            wsFrozenSnapshot.flightStatus
        )
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "   RSSI: "
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsFrozenSnapshot.filteredRSSI
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        " dBm"
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // HEADING / SPEED
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        113
    );

    wsGfx->print(
        "HDG: "
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.directionValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.direction,
            0
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "   HSPD: "
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.horizontalSpeedValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.horizontalSpeed,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "   VSPD: "
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.verticalSpeedValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.verticalSpeed,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // IDENTIFIER
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        133
    );

    wsGfx->print(
        "ID: "
    );
    ridGnssRenderCheckpoint();


    if (
        !wsFrozenSnapshot.displayIDValid
    ) {

        wsGfx->print(
            "<not provided>"
        );
        ridGnssRenderCheckpoint();
    }

    else if (
        privateSnapshot
    ) {

        wsGfx->print(
            "[REDACTED]"
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            wsFrozenSnapshot.displayID
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // POSITION
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        153
    );

    wsGfx->print(
        "POS: "
    );
    ridGnssRenderCheckpoint();


    if (
        !wsFrozenSnapshot.hasPosition
    ) {

        wsGfx->print(
            "UNKNOWN"
        );
        ridGnssRenderCheckpoint();
    }

    else if (
        privateSnapshot
    ) {

        wsGfx->print(
            "VALID [REDACTED]"
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            wsFrozenSnapshot.latitude,
            6
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            ", "
        );
        ridGnssRenderCheckpoint();

        wsGfx->print(
            wsFrozenSnapshot.longitude,
            6
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // ALTITUDE
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        173
    );

    wsGfx->print(
        "ALT B:"
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.barometricAltitudeValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.barometricAltitude,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "  G:"
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.geodeticAltitudeValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.geodeticAltitude,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    wsGfx->print(
        "  R:"
    );
    ridGnssRenderCheckpoint();

    if (
        wsFrozenSnapshot.heightValid
    ) {

        wsGfx->print(
            wsFrozenSnapshot.height,
            1
        );
        ridGnssRenderCheckpoint();
    }

    else {

        wsGfx->print(
            "UNK"
        );
        ridGnssRenderCheckpoint();
    }


    // --------------------------------------------------------
    // PACKETS / PROTOCOL
    // --------------------------------------------------------

    wsGfx->setCursor(
        24,
        193
    );

    wsGfx->print(
        "PKTS: "
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsFrozenSnapshot.packetCount
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        "   ODID v"
    );
    ridGnssRenderCheckpoint();

    wsGfx->print(
        wsFrozenSnapshot.protocolVersion
    );
    ridGnssRenderCheckpoint();


    // --------------------------------------------------------
    // Explicitly communicate that this is not the live table.
    // --------------------------------------------------------

    wsGfx->setTextColor(
        WS_COLOR_GREY
    );

    wsGfx->setCursor(
        96,
        225
    );

    wsGfx->print(
        "FROZEN COPY - LIVE RECEIVER CONTINUES"
    );
    ridGnssRenderCheckpoint();


    wsDrawButton(
        wsButtonBackLive,
        false,
        CYAN
    );
}


// ============================================================
// CURRENT SCREEN DRAW
// ============================================================

void wsDrawCurrentScreen() {

    switch (
        wsCurrentScreen
    ) {

        case WS_SCREEN_NOTICE:
            wsDrawBootNotice();
            break;

        case WS_SCREEN_HOME:

            wsDrawHomeScreen();
            break;


        case WS_SCREEN_REFERENCE:

            wsDrawReferenceScreen();
            break;


        case WS_SCREEN_PRIVACY:

            wsDrawPrivacyScreen();
            break;


        case WS_SCREEN_LIVE:

            wsDrawLiveScreen();
            break;


        case WS_SCREEN_SNAPSHOT:

            wsDrawSnapshotScreen();
            break;
    }
}


// ============================================================
// TOUCH HIT TEST
// ============================================================

WSButtonID wsHitTest(
    int16_t x,
    int16_t y
) {

    switch (
        wsCurrentScreen
    ) {

        case WS_SCREEN_NOTICE:
            if (x >= 54 && x < 426 &&
                y >= 262 && y < 307) {
                return WS_BTN_ACKNOWLEDGE;
            }
            break;

        case WS_SCREEN_HOME:

            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonBegin
                )
            ) {

                return WS_BTN_BEGIN;
            }

            break;


        case WS_SCREEN_REFERENCE:

            if (wsPointInside(x, y, wsButtonUseGnss)) {
                return WS_BTN_USE_GNSS;
            }
            if (wsPointInside(x, y, wsButtonTargetsOnly)) {
                return WS_BTN_TARGETS_ONLY;
            }
            if (wsPointInside(x, y, wsButtonBack)) {
                return WS_BTN_BACK;
            }
            break;


        case WS_SCREEN_PRIVACY:

            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonPrivate
                )
            ) {

                return WS_BTN_PRIVATE;
            }


            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonOpen
                )
            ) {

                return WS_BTN_OPEN;
            }


            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonStart
                )
            ) {

                return WS_BTN_START;
            }


            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonBack
                )
            ) {

                return WS_BTN_BACK;
            }

            break;


        case WS_SCREEN_LIVE:

            if (
                wsPointInside(
                    x,
                    y,
                    (wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarTargetsTab : wsButtonTargetsTab)
                )
            ) {

                return WS_BTN_TAB_TARGETS;
            }


            if (
                wsPointInside(
                    x,
                    y,
                    (wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarRadarTab : wsButtonRadarTab)
                )
            ) {

                return WS_BTN_TAB_RADAR;
            }


            if (
                wsCurrentLiveView
                == WS_VIEW_TARGETS
            ) {

                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonT01
                    )
                ) {

                    return WS_BTN_T01;
                }


                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonT02
                    )
                ) {

                    return WS_BTN_T02;
                }


                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonT03
                    )
                ) {

                    return WS_BTN_T03;
                }


                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonT04
                    )
                ) {

                    return WS_BTN_T04;
                }
            }


            if (
                wsCurrentLiveView
                == WS_VIEW_TARGETS
            ) {

                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonSnapshot
                    )
                ) {

                    return WS_BTN_SNAPSHOT;
                }


                if (
                    wsPointInside(
                        x,
                        y,
                        wsButtonRescan
                    )
                ) {

                    return WS_BTN_RESCAN;
                }
            }


            if (
                wsPointInside(
                    x,
                    y,
                    (wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarModeButton : wsButtonMode)
                )
            ) {

                return WS_BTN_MODE;
            }


            if (
                wsPointInside(
                    x,
                    y,
                    (wsCurrentLiveView == WS_VIEW_RADAR ? wsRadarStopButton : wsButtonStop)
                )
            ) {

                return WS_BTN_STOP;
            }

            break;


        case WS_SCREEN_SNAPSHOT:

            if (
                wsPointInside(
                    x,
                    y,
                    wsButtonBackLive
                )
            ) {

                return WS_BTN_BACK_LIVE;
            }

            break;
    }


    return WS_BTN_NONE;
}


// ============================================================
// BUTTON NAME
// ============================================================

const char *wsButtonName(
    WSButtonID button
) {

    switch (button) {

        case WS_BTN_ACKNOWLEDGE:
            return "ACKNOWLEDGE";

        case WS_BTN_BEGIN:
            return "BEGIN";

        case WS_BTN_USE_GNSS:
            return "USE_GNSS";

        case WS_BTN_TARGETS_ONLY:
            return "TARGETS_ONLY";

        case WS_BTN_PRIVATE:
            return "PRIVATE";

        case WS_BTN_OPEN:
            return "OPEN";

        case WS_BTN_START:
            return "START";

        case WS_BTN_BACK:
            return "BACK";

        case WS_BTN_TAB_TARGETS:
            return "TAB_TARGETS";

        case WS_BTN_TAB_RADAR:
            return "TAB_RADAR";

        case WS_BTN_T01:
            return "T01";

        case WS_BTN_T02:
            return "T02";

        case WS_BTN_T03:
            return "T03";

        case WS_BTN_T04:
            return "T04";

        case WS_BTN_SNAPSHOT:
            return "SNAPSHOT";

        case WS_BTN_RESCAN:
            return "RESCAN";

        case WS_BTN_MODE:
            return "MODE";

        case WS_BTN_STOP:
            return "STOP";

        case WS_BTN_BACK_LIVE:
            return "BACK_LIVE";

        default:
            return "NONE";
    }
}


// ============================================================
// WAVESHARE BUTTON PROCESSING
//
// Stage 1B-B owns real privacy mode and capture START / STOP.
// RESCAN owns a real fresh backend restart.
// SNAPSHOT freezes a mutex-protected target copy and opens
// a dedicated frozen-data screen while BLE remains active.
// ============================================================

void wsProcessButton(
    WSButtonID button
) {

    if (!ridBootAckAccepted &&
        button != WS_BTN_ACKNOWLEDGE) {
        Serial.println("[ACK] UI control blocked.");
        return;
    }

    bool redraw = true;


    switch (button) {

        case WS_BTN_ACKNOWLEDGE:
            if (wsCurrentScreen != WS_SCREEN_NOTICE) {
                redraw = false;
                break;
            }

            ridAckRtcMagic = RID_ACK_MAGIC;
            ridAckRtcCheck = ~RID_ACK_MAGIC;
            ridBootAckAccepted = true;
            wsCurrentScreen = WS_SCREEN_HOME;

            Serial.println(
                "[ACK] Operator acknowledged notice."
            );
            break;

        case WS_BTN_BEGIN:

            outputMode = MODE_PRIVATE;
            wsUiPrivateSelected = true;
            wsUiSelectedTarget = 0;
            wsUiSessionStarted = false;
            wsFrozenSnapshotCaptured = false;
            wsRadarReady = false;
            wsCurrentLiveView = WS_VIEW_TARGETS;

            wsCurrentScreen =
                WS_SCREEN_REFERENCE;

            Serial.println(
                "[UI] BEGIN -> REFERENCE SELECT / backend PRIVATE / scanner IDLE."
            );

            break;


        case WS_BTN_USE_GNSS:

            if (wsCurrentScreen != WS_SCREEN_REFERENCE || scanSessionActive) {
                redraw = false;
                break;
            }

            // Same explicit selection used by R,GNSS; readiness remains data-driven.
            ridGnssD3b2SetMode(true);
            wsRadarReady = false;
            wsCurrentScreen = WS_SCREEN_PRIVACY;
            Serial.println("[GNSS-UI-R5] reference=GNSS_REQUESTED origin=TOUCH");
            Serial.println("[UI] REFERENCE SELECT -> OUTPUT MODE / scanner IDLE.");
            break;


        case WS_BTN_TARGETS_ONLY:

            if (wsCurrentScreen != WS_SCREEN_REFERENCE || scanSessionActive) {
                redraw = false;
                break;
            }

            ridGnssD3b2SetMode(false);
            wsClearReceiverReference();
            wsRadarReady = false;
            wsCurrentScreen = WS_SCREEN_PRIVACY;
            Serial.println("[GNSS-UI-R5] reference=NONE origin=TOUCH");
            Serial.println("[UI] REFERENCE SELECT -> OUTPUT MODE / scanner IDLE.");
            break;


        case WS_BTN_PRIVATE:

            outputMode = MODE_PRIVATE;
            wsUiPrivateSelected = true;

            Serial.println(
                "[UI] Output mode -> PRIVATE."
            );

            break;


        case WS_BTN_OPEN:

            outputMode = MODE_OPEN;
            wsUiPrivateSelected = false;

            Serial.println(
                "[UI] Output mode -> OPEN."
            );

            break;


        case WS_BTN_START:

            if (scanSessionActive) {

                Serial.println(
                    "[UI] START ignored: RID session already active."
                );

                redraw = false;
                break;
            }


            resetRIDCaptureState();

            wsUiSessionStarted = false;
            wsRadarReady = false;

            wsCurrentLiveView =
                WS_VIEW_TARGETS;


            if (startRIDScanAsync()) {

                wsUiSessionStarted = true;

                wsCurrentScreen =
                    WS_SCREEN_LIVE;

                Serial.println(
                    "[UI] START SCAN -> TARGETS / backend ACTIVE."
                );

                Serial.println(
                    "[UI] RADAR state = ACQUIRING."
                );
            }

            else {

                Serial.println(
                    "[UI ERROR] START SCAN failed; remaining on OUTPUT MODE."
                );
            }

            break;


        case WS_BTN_BACK:

            if (wsCurrentScreen == WS_SCREEN_PRIVACY) {
                wsCurrentScreen = WS_SCREEN_REFERENCE;
                Serial.println("[UI] BACK -> REFERENCE SELECT");
            } else if (wsCurrentScreen == WS_SCREEN_REFERENCE) {
                wsCurrentScreen = WS_SCREEN_HOME;
                Serial.println("[UI-SHELL] BACK -> HOME");
            } else {
                redraw = false;
            }

            break;


        case WS_BTN_BACK_LIVE:

            wsCurrentScreen =
                WS_SCREEN_LIVE;

            Serial.println(
                "[UI] SNAPSHOT -> BACK TO LIVE."
            );

            break;


        case WS_BTN_TAB_TARGETS:

            wsCurrentLiveView =
                WS_VIEW_TARGETS;

            Serial.println(
                "[UI-SHELL] VIEW -> TARGETS"
            );

            break;


        case WS_BTN_TAB_RADAR:

            if (!wsRadarReady) {

                redraw = false;

                Serial.println(
                    "[UI-SHELL] RADAR unavailable: ACQUIRING geographic prerequisites."
                );

                break;
            }


            wsCurrentLiveView =
                WS_VIEW_RADAR;

            Serial.println(
                "[UI-SHELL] VIEW -> RADAR"
            );

            break;


        case WS_BTN_T01:

            wsUiSelectedTarget = 0;

            Serial.println(
                "[UI-SHELL] SELECT T01"
            );

            break;


        case WS_BTN_T02:

            wsUiSelectedTarget = 1;

            Serial.println(
                "[UI-SHELL] SELECT T02"
            );

            break;


        case WS_BTN_T03:

            wsUiSelectedTarget = 2;

            Serial.println(
                "[UI-SHELL] SELECT T03"
            );

            break;


        case WS_BTN_T04:

            wsUiSelectedTarget = 3;

            Serial.println(
                "[UI-SHELL] SELECT T04"
            );

            break;


        case WS_BTN_SNAPSHOT:

            if (!scanSessionActive) {

                Serial.println(
                    "[UI] SNAPSHOT ignored: no active RID session."
                );

                break;
            }


            wsFrozenSnapshotTarget =
                wsUiSelectedTarget;

            wsFrozenSnapshotMode =
                outputMode;

            wsFrozenSnapshotCapturedMs =
                millis();


            wsFrozenSnapshotCaptured =
                wsCaptureTargetSnapshot(
                    wsUiSelectedTarget,
                    wsFrozenSnapshot
                );


            Serial.print(
                "[UI] SNAPSHOT T0"
            );

            Serial.print(
                wsUiSelectedTarget + 1
            );

            Serial.print(
                wsFrozenSnapshotCaptured
                ? " FROZEN / "
                : " EMPTY / "
            );

            Serial.println(
                wsFrozenSnapshotMode
                    == MODE_PRIVATE
                ? "PRIVATE"
                : "OPEN"
            );


            wsCurrentScreen =
                WS_SCREEN_SNAPSHOT;

            break;


        case WS_BTN_RESCAN:

            if (!scanSessionActive) {

                redraw = false;

                Serial.println(
                    "[UI] RESCAN ignored: no active RID session."
                );

                break;
            }


            // A RESCAN creates a new capture generation.
            // Do not carry a frozen target from the prior
            // generation into the fresh session.
            wsFrozenSnapshotCaptured = false;


            wsRadarReady = false;


            if (restartRIDScanFromUI()) {

                wsUiSessionStarted = true;

                Serial.println(
                    "[UI] RESCAN complete / fresh RID capture ACTIVE."
                );

                Serial.println(
                    "[UI] RADAR state = ACQUIRING."
                );
            }

            else {

                // Mirror the backend's actual state.
                // A failed RESCAN may occur before the existing
                // scan session has been successfully retired.
                wsUiSessionStarted =
                    scanSessionActive;

                Serial.println(
                    "[UI ERROR] RESCAN failed."
                );
            }

            break;


        case WS_BTN_MODE:

            outputMode =
                outputMode == MODE_PRIVATE
                ? MODE_OPEN
                : MODE_PRIVATE;

            wsUiPrivateSelected =
                outputMode == MODE_PRIVATE;

            Serial.println(
                outputMode == MODE_PRIVATE
                ? "[UI] Output mode -> PRIVATE."
                : "[UI] Output mode -> OPEN."
            );

            break;


        case WS_BTN_STOP:

            if (scanSessionActive) {

                if (requestStopRIDScan()) {

                    Serial.println(
                        "[UI] STOP / HOME requested."
                    );
                }

                else {

                    Serial.println(
                        "[UI ERROR] STOP request failed."
                    );

                    // Remain on live view after failed STOP.
                    wsUiSessionStarted = scanSessionActive;
                    redraw = false;
                    break;
                }
            }

            else {

                Serial.println(
                    "[UI] STOP / HOME: receiver already idle."
                );
            }


            wsUiSessionStarted = false;
            wsRadarReady = false;

            wsCurrentLiveView =
                WS_VIEW_TARGETS;

            wsCurrentScreen =
                WS_SCREEN_HOME;

            break;


        default:

            redraw = false;
            break;
    }


    if (redraw) {

        wsDrawCurrentScreen();
    }
}


// ============================================================
// CAPACITIVE TOUCH SERVICE
//
// Action requires:
//   DOWN inside a control
//   UP inside the SAME control
//
// This gives us basic protection against accidental drags
// and hand movement.
// ============================================================

void serviceWaveshareTouchUI() {

    if (
        millis() - wsLastTouchPollMs
        < WS_TOUCH_POLL_MS
    ) {

        return;
    }


    wsLastTouchPollMs =
        millis();


    int16_t rawX[1];
    int16_t rawY[1];


    uint8_t touched =
        wsTouch.getPoint(
            rawX,
            rawY,
            1
        );


    if (touched) {

        // Proven Rotation-1 transform:
        //
        // screen X = raw Y
        // screen Y = 319 - raw X

        int16_t screenX =
            constrain(
                rawY[0],
                0,
                wsGfx->width() - 1
            );


        int16_t screenY =
            constrain(
                (WS_LCD_HOR_RES - 1)
                - rawX[0],
                0,
                wsGfx->height() - 1
            );


        wsTouchLastX =
            screenX;

        wsTouchLastY =
            screenY;


        if (!wsTouchWasDown) {

            wsTouchWasDown = true;


            wsTouchDownButton =
                wsHitTest(
                    screenX,
                    screenY
                );


            Serial.printf(
                "[TOUCH-DOWN] x=%d y=%d button=%s\n",
                screenX,
                screenY,
                wsButtonName(
                    wsTouchDownButton
                )
            );
        }


        return;
    }


    if (!wsTouchWasDown) {

        return;
    }


    wsTouchWasDown =
        false;


    WSButtonID releaseButton =
        wsHitTest(
            wsTouchLastX,
            wsTouchLastY
        );


    Serial.printf(
        "[TOUCH-UP] x=%d y=%d button=%s\n",
        wsTouchLastX,
        wsTouchLastY,
        wsButtonName(
            releaseButton
        )
    );


    if (
        wsTouchDownButton
            != WS_BTN_NONE

        && releaseButton
            == wsTouchDownButton
    ) {

        wsProcessButton(
            releaseButton
        );
    }

    else {

        Serial.println(
            "[UI-SHELL] TOUCH CANCELLED"
        );
    }


    wsTouchDownButton =
        WS_BTN_NONE;
}


void drawWaveshareOrientationTest() {

    wsRenderFillScreen(BLACK);

    const int16_t w = wsGfx->width();
    const int16_t h = wsGfx->height();

    wsGfx->drawRect(
        0,
        0,
        w,
        h,
        WHITE
    );
    ridGnssRenderCheckpoint();

    wsGfx->setTextSize(2);
    wsGfx->setTextColor(CYAN);

    wsGfx->setCursor(135, 20);
    wsGfx->println("TOP");
    ridGnssRenderCheckpoint();

    wsGfx->setTextColor(WHITE);

    wsGfx->setCursor(12, 145);
    wsGfx->println("LEFT");
    ridGnssRenderCheckpoint();

    wsGfx->setCursor(w - 75, 145);
    wsGfx->println("RIGHT");
    ridGnssRenderCheckpoint();

    wsGfx->setCursor(115, h - 30);
    wsGfx->println("BOTTOM");
    ridGnssRenderCheckpoint();


    wsGfx->drawLine(
        w / 2 - 20,
        h / 2,
        w / 2 + 20,
        h / 2,
        GREEN
    );
    ridGnssRenderCheckpoint();

    wsGfx->drawLine(
        w / 2,
        h / 2 - 20,
        w / 2,
        h / 2 + 20,
        GREEN
    );
    ridGnssRenderCheckpoint();


    wsGfx->setTextColor(YELLOW);
    wsGfx->setCursor(135, 95);
    wsGfx->println("LANDSCAPE TEST");
    ridGnssRenderCheckpoint();

    wsGfx->setCursor(165, 120);
    wsGfx->println("ROTATION 1");
    ridGnssRenderCheckpoint();


    char dimensions[32];

    snprintf(
        dimensions,
        sizeof(dimensions),
        "%d x %d",
        w,
        h
    );

    wsGfx->setTextColor(GREEN);
    wsGfx->setCursor(190, 195);
    wsGfx->println(dimensions);
    ridGnssRenderCheckpoint();
}


// ============================================================
// GNSS-D3B1: READ-ONLY NMEA POSITION / FIX FRESHNESS GATE
// TEST ONLY — UART1 RX GPIO44 / 115200 baud / TX disabled.
// BLE receiver, touchscreen, manual reference and RADAR unchanged.
// No coordinates, raw NMEA or identifiers are printed or persisted.
// ============================================================

struct RIDGnssD3B1Position {
    bool seen = false;
    bool valid = false;
    double latitude = 0.0;
    double longitude = 0.0;
    uint32_t receivedMs = 0;
    uint8_t satellites = 0;
};

static RIDGnssD3B1Position ridGnssD3b1Gga;
static RIDGnssD3B1Position ridGnssD3b1Rmc;
static bool ridGnssD3b1EverFresh = false;
static uint32_t ridGnssD3b1Bytes = 0;
static uint32_t ridGnssD3b1Lines = 0;
static uint32_t ridGnssD3b1NmeaOk = 0;
static uint32_t ridGnssD3b1ChecksumBad = 0;
static uint32_t ridGnssD3b1Malformed = 0;
static uint32_t ridGnssD3b1TooLong = 0;
static uint32_t ridGnssD3b1GgaCount = 0;
static uint32_t ridGnssD3b1RmcCount = 0;
static uint32_t ridGnssD3b1LastReportMs = 0;
static char ridGnssD3b1Line[160];
static size_t ridGnssD3b1LineLen = 0;
static bool ridGnssD3b1Discard = false;
static constexpr uint32_t RID_GNSS_D3B1_FRESH_MS = 4000U;

static int ridGnssD3b1Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool ridGnssD3b1Unsigned(const char *s, unsigned &result) {
    if (!s || !*s) return false;
    unsigned val = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        val = val * 10U + (unsigned)(*p - '0');
        if (val > 99U) return false;
    }
    result = val;
    return true;
}

static bool ridGnssD3b1Coordinate(const char *text, const char *hem,
                                   bool isLatitude, double &out) {
    if (!text || !*text || !hem || hem[0] == 0 || hem[1] != 0) return false;
    char *end = nullptr;
    const double raw = strtod(text, &end);
    if (end == text || !end || *end != 0 || !isfinite(raw) || raw < 0.0) return false;
    const unsigned degrees = (unsigned)(raw / 100.0);
    const double minutes = raw - (double)degrees * 100.0;
    if (minutes < 0.0 || minutes >= 60.0) return false;
    const double magnitude = (double)degrees + minutes / 60.0;
    if (isLatitude) {
        if (hem[0] != 'N' && hem[0] != 'S') return false;
        if (magnitude > 90.0) return false;
    } else {
        if (hem[0] != 'E' && hem[0] != 'W') return false;
        if (magnitude > 180.0) return false;
    }
    out = (hem[0] == 'S' || hem[0] == 'W') ? -magnitude : magnitude;
    return isfinite(out);
}

static void ridGnssD3b1ProcessLine(char *line) {
    size_t n = strlen(line);
    if (n && line[n - 1] == '\r') line[--n] = 0;
    if (n < 9 || line[0] != '$') {
        ++ridGnssD3b1Malformed;
        return;
    }

    char *star = strrchr(line, '*');
    if (!star || strlen(star) != 3) {
        ++ridGnssD3b1Malformed;
        return;
    }
    const int hi = ridGnssD3b1Hex(star[1]);
    const int lo = ridGnssD3b1Hex(star[2]);
    if (hi < 0 || lo < 0) {
        ++ridGnssD3b1Malformed;
        return;
    }
    uint8_t computed = 0;
    for (const char *p = line + 1; p < star; ++p) {
        if (*p < ' ' || *p > '~' || *p == '*') {
            ++ridGnssD3b1Malformed;
            return;
        }
        computed ^= (uint8_t)*p;
    }
    if (computed != (uint8_t)((hi << 4) | lo)) {
        ++ridGnssD3b1ChecksumBad;
        return;
    }
    ++ridGnssD3b1NmeaOk;
    *star = 0;

    const char *payload = line + 1;
    if (strlen(payload) < 5U) return;
    if (!((payload[0] >= 'A' && payload[0] <= 'Z') &&
          (payload[1] >= 'A' && payload[1] <= 'Z'))) return;
    const bool isGga = strncmp(payload + 2, "GGA", 3) == 0;
    const bool isRmc = strncmp(payload + 2, "RMC", 3) == 0;
    if (!isGga && !isRmc) return;

    // Split fields while preserving empty NMEA fields (strtok would skip them).
    char *fields[20] = {nullptr};
    size_t count = 0;
    fields[count++] = line + 1;
    for (char *p = line + 1; *p; ++p) {
        if (*p == ',') {
            *p = 0;
            if (count >= 20U) {
                ++ridGnssD3b1Malformed;
                return;
            }
            fields[count++] = p + 1;
        }
    }
    const uint32_t now = millis();
    if (isGga) {
        ++ridGnssD3b1GgaCount;
        ridGnssD3b1Gga.seen = true;
        ridGnssD3b1Gga.receivedMs = now;
        ridGnssD3b1Gga.valid = false;
        if (count < 8U) return;
        unsigned quality = 0;
        unsigned sats = 0;
        if (!ridGnssD3b1Unsigned(fields[6], quality) ||
            !ridGnssD3b1Unsigned(fields[7], sats) ||
            quality == 0U || quality > 8U) return;
        double lat = 0.0, lon = 0.0;
        if (!ridGnssD3b1Coordinate(fields[2], fields[3], true, lat) ||
            !ridGnssD3b1Coordinate(fields[4], fields[5], false, lon)) return;
        ridGnssD3b1Gga.latitude = lat;
        ridGnssD3b1Gga.longitude = lon;
        ridGnssD3b1Gga.satellites = (uint8_t)sats;
        ridGnssD3b1Gga.valid = true;
    } else {
        ++ridGnssD3b1RmcCount;
        ridGnssD3b1Rmc.seen = true;
        ridGnssD3b1Rmc.receivedMs = now;
        ridGnssD3b1Rmc.valid = false;
        if (count < 7U || strcmp(fields[2], "A") != 0) return;
        double lat = 0.0, lon = 0.0;
        if (!ridGnssD3b1Coordinate(fields[3], fields[4], true, lat) ||
            !ridGnssD3b1Coordinate(fields[5], fields[6], false, lon)) return;
        ridGnssD3b1Rmc.latitude = lat;
        ridGnssD3b1Rmc.longitude = lon;
        ridGnssD3b1Rmc.valid = true;
    }
}

enum RIDGnssD3B1State {
    RID_GNSS_D3B1_NO_FIX,
    RID_GNSS_D3B1_FRESH,
    RID_GNSS_D3B1_STALE
};

static uint8_t ridGnssD3b1State(uint32_t now) {
    const bool ggaFresh = ridGnssD3b1Gga.seen &&
        (uint32_t)(now - ridGnssD3b1Gga.receivedMs) <= RID_GNSS_D3B1_FRESH_MS;
    const bool rmcFresh = ridGnssD3b1Rmc.seen &&
        (uint32_t)(now - ridGnssD3b1Rmc.receivedMs) <= RID_GNSS_D3B1_FRESH_MS;
    if (!ggaFresh || !rmcFresh) {
        return ridGnssD3b1EverFresh ? RID_GNSS_D3B1_STALE : RID_GNSS_D3B1_NO_FIX;
    }
    const bool positionsAgree = ridGnssD3b1Gga.valid && ridGnssD3b1Rmc.valid &&
        fabs(ridGnssD3b1Gga.latitude - ridGnssD3b1Rmc.latitude) <= 0.005 &&
        fabs(ridGnssD3b1Gga.longitude - ridGnssD3b1Rmc.longitude) <= 0.005;
    if (!positionsAgree) return RID_GNSS_D3B1_NO_FIX;
    ridGnssD3b1EverFresh = true;
    return RID_GNSS_D3B1_FRESH;
}

// ============================================================
// RID-01 D3B2 UART EVENTS R2 - INPUT-ONLY OBSERVATION
// HardwareSerial.onReceiveError on ESP32 Arduino core 3.3.11.
// The UART callback is called from a separate FreeRTOS event task.
// NOTE: counts are UART error NOTIFICATIONS, not NMEA bad checksums.
// ============================================================
// RX1024 R3: one UART configuration experiment with explicit init evidence.
// rx_set is the setter result, not a measurement of usable ring-buffer capacity.
static constexpr size_t RID_GNSS_RX1024_BYTES = 1024U;
static size_t ridGnssRx1024Set = 0U;
static bool ridGnssRx1024InitOk = false;

static portMUX_TYPE ridGnssR2ErrorMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t ridGnssR2ErrBufferFull = 0;
static volatile uint32_t ridGnssR2ErrFifoOverflow = 0;
static volatile uint32_t ridGnssR2ErrFrame = 0;
static volatile uint32_t ridGnssR2ErrParity = 0;
static volatile uint32_t ridGnssR2ErrBreak = 0;
static volatile uint32_t ridGnssR2ErrUnknown = 0;
static uint32_t ridGnssR2ErrLastReportMs = 0;

// Summary is deliberately independent of D3B1's checksum counters.
// The data below never includes coordinates, device IDs or raw NMEA.
void ridGnssR2ReportUartErrorEvents() {
    const uint32_t now = millis();
    if ((uint32_t)(now - ridGnssR2ErrLastReportMs) < 10000U) return;
    ridGnssR2ErrLastReportMs = now;

    uint32_t bufferFull, fifoOverflow, frame, parity, breakCount, unknown;
    portENTER_CRITICAL(&ridGnssR2ErrorMux);
    bufferFull = ridGnssR2ErrBufferFull;
    fifoOverflow = ridGnssR2ErrFifoOverflow;
    frame = ridGnssR2ErrFrame;
    parity = ridGnssR2ErrParity;
    breakCount = ridGnssR2ErrBreak;
    unknown = ridGnssR2ErrUnknown;
    portEXIT_CRITICAL(&ridGnssR2ErrorMux);

    Serial.printf(
        "[GNSS-D3B2-UARTERR] scan=%s buf_full=%lu fifo_ovf=%lu "
        "frame=%lu parity=%lu break=%lu other=%lu "
        "candidate=RX1024_RADAR_COLOR_R7 rx_requested=%lu rx_set=%lu uart_init=%s\n",
        scanSessionActive ? "ACTIVE" : "IDLE",
        (unsigned long)bufferFull, (unsigned long)fifoOverflow,
        (unsigned long)frame, (unsigned long)parity,
        (unsigned long)breakCount, (unsigned long)unknown,
        (unsigned long)RID_GNSS_RX1024_BYTES,
        (unsigned long)ridGnssRx1024Set,
        ridGnssRx1024InitOk ? "OK" : "FAIL"
    );
}

void ridGnssD3b1Begin() {
    ridGnssRx1024InitOk = false;
    ridGnssRx1024Set = Serial1.setRxBufferSize(RID_GNSS_RX1024_BYTES);
    if (ridGnssRx1024Set != RID_GNSS_RX1024_BYTES) {
        Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_RADAR_COLOR_R7 rx_requested=%lu "
                      "rx_set=%lu uart_init=FAIL reason=RX_SIZE\n",
            (unsigned long)RID_GNSS_RX1024_BYTES,
            (unsigned long)ridGnssRx1024Set);
        return;
    }
    Serial1.begin(115200, SERIAL_8N1, 44, -1);
    if (!Serial1) {
        Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_RADAR_COLOR_R7 rx_requested=%lu "
                      "rx_set=%lu uart_init=FAIL reason=UART_INIT\n",
            (unsigned long)RID_GNSS_RX1024_BYTES,
            (unsigned long)ridGnssRx1024Set);
        return;
    }
    // Core 3.3.11's error callback runs in the UART event task, not ISR.
    // Do not print, block, read UART bytes, or alter GNSS state here.
    Serial1.onReceiveError([](hardwareSerial_error_t event) {
        portENTER_CRITICAL(&ridGnssR2ErrorMux);
        switch (event) {
            case UART_BUFFER_FULL_ERROR: ++ridGnssR2ErrBufferFull; break;
            case UART_FIFO_OVF_ERROR: ++ridGnssR2ErrFifoOverflow; break;
            case UART_FRAME_ERROR: ++ridGnssR2ErrFrame; break;
            case UART_PARITY_ERROR: ++ridGnssR2ErrParity; break;
            case UART_BREAK_ERROR: ++ridGnssR2ErrBreak; break;
            case UART_NO_ERROR: break;
            default: ++ridGnssR2ErrUnknown; break;
        }
        portEXIT_CRITICAL(&ridGnssR2ErrorMux);
    });

    // Driver installation was checked above; onReceiveError() returns void.
    // This flag does not independently attest UART event-task allocation.
    ridGnssRx1024InitOk = true;
    Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_RADAR_COLOR_R7 rx_requested=%lu "
                  "rx_set=%lu uart_init=OK\n",
        (unsigned long)RID_GNSS_RX1024_BYTES,
        (unsigned long)ridGnssRx1024Set);

    Serial.println("[GNSS-D3B1] UART1 RX=GPIO44 / 115200 / receive-only");
    Serial.println("[GNSS-D3B1] NMEA checksum + GGA/RMC validity + freshness");
    Serial.println("[GNSS-D3B1] reference provider and RADAR left unchanged");
}

void ridGnssD3b1Tick() {
    // A rejected configuration must not produce a valid GNSS comparison.
    if (!ridGnssRx1024InitOk) return;
    // Bound each receive call; R4 adds calls between display operations.
    uint16_t budget = 512;
    while (budget-- > 0 && Serial1.available() > 0) {
        const int incoming = Serial1.read();
        if (incoming < 0) break;
        ++ridGnssD3b1Bytes;
        const char c = (char)incoming;
        if (c == '$') {
            ridGnssD3b1Line[0] = '$';
            ridGnssD3b1LineLen = 1;
            ridGnssD3b1Discard = false;
        } else if (c == '\n') {
            ++ridGnssD3b1Lines;
            if (!ridGnssD3b1Discard && ridGnssD3b1LineLen > 0) {
                ridGnssD3b1Line[ridGnssD3b1LineLen] = 0;
                ridGnssD3b1ProcessLine(ridGnssD3b1Line);
            }
            ridGnssD3b1LineLen = 0;
            ridGnssD3b1Discard = false;
        } else if (!ridGnssD3b1Discard && ridGnssD3b1LineLen > 0) {
            if (ridGnssD3b1LineLen + 1 >= sizeof(ridGnssD3b1Line)) {
                ++ridGnssD3b1TooLong;
                ridGnssD3b1Discard = true;
            } else {
                ridGnssD3b1Line[ridGnssD3b1LineLen++] = c;
            }
        }
    }
    const uint32_t now = millis();
    if ((uint32_t)(now - ridGnssD3b1LastReportMs) >= 5000U) {
        ridGnssD3b1LastReportMs = now;
        const uint8_t state = ridGnssD3b1State(now);
        const char *stateName = state == RID_GNSS_D3B1_FRESH ? "FRESH" :
            (state == RID_GNSS_D3B1_STALE ? "STALE" : "NO_FIX");
        const unsigned sats = (state == RID_GNSS_D3B1_FRESH)
            ? (unsigned)ridGnssD3b1Gga.satellites : 0U;
        Serial.printf("[GNSS-D3B1] bytes=%lu lines=%lu nmea_ok=%lu checksum_bad=%lu "
                      "malformed=%lu long=%lu GGA=%lu RMC=%lu FIX=%s SATS=%u\n",
            (unsigned long)ridGnssD3b1Bytes,
            (unsigned long)ridGnssD3b1Lines,
            (unsigned long)ridGnssD3b1NmeaOk,
            (unsigned long)ridGnssD3b1ChecksumBad,
            (unsigned long)ridGnssD3b1Malformed,
            (unsigned long)ridGnssD3b1TooLong,
            (unsigned long)ridGnssD3b1GgaCount,
            (unsigned long)ridGnssD3b1RmcCount,
            stateName, sats);
    }
}

// ============================================================
// GNSS-D3B2: EXPLICIT, FAIL-CLOSED GNSS RECEIVER REFERENCE
// Compile-only TEST CANDIDATE. No coordinates ever printed.
// Selection: R,GNSS | R,CLEAR | valid R,<lat>,<lon>.
// ============================================================

static bool ridGnssD3b2Requested = false;
static constexpr uint8_t RID_GNSS_D3B2_MIN_SATS = 4U;
static constexpr double RID_GNSS_D3B2_COORD_DIFF_LIMIT_DEG = 0.0005;

void ridGnssD3b2SetMode(bool enabled) {
    ridGnssD3b2Requested = enabled;
    if (enabled && wsReceiverReference.source == WS_REF_MANUAL_FIXED) {
        // Explicit mode switch: do not retain a manual position under GNSS.
        wsClearReceiverReference();
    }
    if (!enabled && wsReceiverReference.source == WS_REF_GNSS) {
        wsClearReceiverReference();
    }
}

// ============================================================
// GNSS-D3B2 DIAG-R1: PASSIVE UART SERVICE TIMING (BENCH ONLY)
// NO raw NMEA, receiver coordinates or target identifiers.
// Sampling does not claim hardware FIFO overrun detection.
// ============================================================
static uint32_t ridGnssD3b2DiagLastServiceMs = 0;
static uint32_t ridGnssD3b2DiagLastReportMs = 0;
static uint32_t ridGnssD3b2DiagPeakGapMs = 0;
static uint32_t ridGnssD3b2DiagPeakPending = 0;
static uint32_t ridGnssD3b2DiagPendingGe128 = 0;
static uint32_t ridGnssD3b2DiagServiceCalls = 0;

void ridGnssD3b2DiagBeforeReceive() {
    const uint32_t now = millis();
    if (ridGnssD3b2DiagLastServiceMs != 0U) {
        const uint32_t gap = now - ridGnssD3b2DiagLastServiceMs;
        if (gap > ridGnssD3b2DiagPeakGapMs) {
            ridGnssD3b2DiagPeakGapMs = gap;
        }
    }
    ridGnssD3b2DiagLastServiceMs = now;
    const int pending = Serial1.available();
    if (pending >= 0) {
        const uint32_t count = static_cast<uint32_t>(pending);
        if (count > ridGnssD3b2DiagPeakPending) {
            ridGnssD3b2DiagPeakPending = count;
        }
        if (count >= 128U) {
            ++ridGnssD3b2DiagPendingGe128;
        }
    }
    ++ridGnssD3b2DiagServiceCalls;
    // Ten-second interval limits extra serial logging during the scan.
    if (ridGnssD3b2DiagLastReportMs == 0U ||
        (uint32_t)(now - ridGnssD3b2DiagLastReportMs) >= 10000U) {
        ridGnssD3b2DiagLastReportMs = now;
        Serial.printf("[GNSS-D3B2-DIAG] scan=%s gap_peak_ms=%lu "
                      "pending_peak=%lu pending_ge128=%lu calls=%lu\n",
            scanSessionActive ? "ACTIVE" : "IDLE",
            (unsigned long)ridGnssD3b2DiagPeakGapMs,
            (unsigned long)ridGnssD3b2DiagPeakPending,
            (unsigned long)ridGnssD3b2DiagPendingGe128,
            (unsigned long)ridGnssD3b2DiagServiceCalls);
        ridGnssD3b2DiagPeakGapMs = 0U;
        ridGnssD3b2DiagPeakPending = 0U;
        ridGnssD3b2DiagPendingGe128 = 0U;
        ridGnssD3b2DiagServiceCalls = 0U;
    }
}


// R4 checkpoints run only in the existing loop task, after returned native
// drawing calls and outside target-data locks. They never redraw, process
// touch, update receiver-reference state, or recursively invoke the UI.
static bool ridGnssRenderEnabled = false;
static bool ridGnssRenderInService = false;

bool ridGnssRenderServiceReady() {
    return ridGnssRenderEnabled && ridGnssRx1024InitOk;
}

void ridGnssRenderEnableAfterFirstReceive() {
    // Called immediately after the original normal-loop DIAG/Tick pair.
    // Keep the setup receive-without-drain behavior out of this experiment.
    ridGnssRenderEnabled = true;
}

void ridGnssRenderCheckpoint() {
    if (!ridGnssRenderServiceReady() || ridGnssRenderInService) return;
    ridGnssRenderInService = true;
    ridGnssD3b2DiagBeforeReceive();
    ridGnssD3b1Tick();
    ridGnssRenderInService = false;
}

// R5 shares the exact existing provider predicate with the status-only page.
// No UART reads, target locks, rendering or coordinate output occur here.
bool ridGnssD3b2PositionUsable() {
    const uint8_t fix = ridGnssD3b1State(millis());
    return fix == RID_GNSS_D3B1_FRESH &&
        ridGnssD3b1Gga.valid && ridGnssD3b1Rmc.valid &&
        ridGnssD3b1Gga.satellites >= RID_GNSS_D3B2_MIN_SATS &&
        isfinite(ridGnssD3b1Gga.latitude) && isfinite(ridGnssD3b1Gga.longitude) &&
        wsCoordinateValid(ridGnssD3b1Gga.latitude, ridGnssD3b1Gga.longitude) &&
        fabs(ridGnssD3b1Gga.latitude - ridGnssD3b1Rmc.latitude) <= RID_GNSS_D3B2_COORD_DIFF_LIMIT_DEG &&
        fabs(ridGnssD3b1Gga.longitude - ridGnssD3b1Rmc.longitude) <= RID_GNSS_D3B2_COORD_DIFF_LIMIT_DEG;
}

uint8_t ridGnssUiStatusCode() {
    if (!ridGnssRx1024InitOk) return WS_GNSS_FIX_UNAVAILABLE;
    if (ridGnssD3b2PositionUsable()) return WS_GNSS_FIX_VALID;
    return ridGnssD3b1State(millis()) == RID_GNSS_D3B1_STALE
        ? WS_GNSS_FIX_STALE : WS_GNSS_FIX_ACQUIRING;
}

void ridGnssD3b2Service() {
    if (!ridGnssD3b2Requested) return;

    // D3B1 validates checksum, GGA fix quality, RMC status, finite/ranged
    // coordinates, agreement, and freshness (4s).
    const bool usable = ridGnssD3b2PositionUsable();

    if (!usable) {
        if (wsReceiverReference.source == WS_REF_GNSS) {
            // Clear before RADAR readiness/plot refresh in the main loop.
            wsClearReceiverReference();
            Serial.println("[GNSS-D3B2] GNSS reference withdrawn (fix invalid/stale).");
        }
        return;
    }

    // The manual FIXED mode takes precedence unless R,GNSS was explicitly
    // selected again; never write GNSS coordinates into a manual record.
    if (wsReceiverReference.source == WS_REF_MANUAL_FIXED) return;

    const bool becomingValid = !(wsReceiverReference.valid &&
        wsReceiverReference.source == WS_REF_GNSS);
    wsReceiverReference.latitude = ridGnssD3b1Gga.latitude;
    wsReceiverReference.longitude = ridGnssD3b1Gga.longitude;
    wsReceiverReference.source = WS_REF_GNSS;
    wsReceiverReference.valid = true;
    if (becomingValid) {
        Serial.println("[GNSS-D3B2] GNSS receiver reference VALID (coordinates hidden).");
    }
}

// ============================================================
// ESP32-S3 BACKEND + DISPLAY MIGRATION HARNESS
// ============================================================

void setup() {

    Serial.begin(115200);

    // ESP32-S3 native USB HWCDC:
    // Prevent unconsumed Serial TX from stalling the receiver
    // when no serial monitor is actively reading output.
    Serial.setTxTimeoutMs(0);

    ridGnssD3b1Begin();

    delay(STARTUP_DELAY_MS);


    targetMutex =
        xSemaphoreCreateMutex();

    if (targetMutex == nullptr) {
        Serial.println(
            "[FATAL] target mutex allocation failed."
        );

        while (true) {
            delay(1000);
        }
    }

    outputMode = MODE_PRIVATE;

    // D4H: Required after reported cold power-on.
    esp_reset_reason_t resetReason = esp_reset_reason();

    // A fresh power event always requires acknowledgment.
    // Invalid/lost retained state also fails closed.
    const bool freshPower =
        resetReason == ESP_RST_POWERON ||
        resetReason == ESP_RST_BROWNOUT ||
        resetReason == ESP_RST_PWR_GLITCH ||
        resetReason == ESP_RST_UNKNOWN;

    if (freshPower) {
        ridAckRtcMagic = 0;
        ridAckRtcCheck = 0;
    }

    ridBootAckAccepted =
        (ridAckRtcMagic == RID_ACK_MAGIC) &&
        (ridAckRtcCheck == ~RID_ACK_MAGIC);

    wsCurrentScreen = ridBootAckAccepted
        ? WS_SCREEN_HOME
        : WS_SCREEN_NOTICE;

    Serial.printf(
        "[ACK] Reset reason=%d / notice=%s\n",
        (int)resetReason,
        ridBootAckAccepted ? "SKIPPED" : "REQUIRED"
    );



    // ------------------------------------------------
    // Waveshare display bring-up
    // Proven hardware path from Touch_Bringup.
    // Touch is intentionally not enabled yet.
    // ------------------------------------------------

    Wire.begin(
        WS_I2C_SDA,
        WS_I2C_SCL
    );

    wsTCA.begin();
    wsTCA.pinMode1(
        1,
        OUTPUT
    );

    wsLcdReset();

    if (!wsGfx->begin()) {

        Serial.println(
            "[FATAL] Waveshare gfx->begin() failed."
        );

        while (true) {
            delay(1000);
        }
    }

    pinMode(
        WS_GFX_BL,
        OUTPUT
    );

    digitalWrite(
        WS_GFX_BL,
        HIGH
    );

    // First physical-orientation candidate:
    // landscape, rotation 1.
    wsGfx->setRotation(1);

    wsDrawCurrentScreen();

    Serial.printf(
        "[DISPLAY] Rotation 1 / width=%d / height=%d\n",
        wsGfx->width(),
        wsGfx->height()
    );

    if (!wsTouch.begin(
            Wire,
            FT6X36_SLAVE_ADDRESS
        )) {

        Serial.println(
            "[FATAL] FT6X36 touch controller not found."
        );

        while (true) {
            delay(1000);
        }
    }

    Serial.println(
        "[TOUCH] FT6X36 detected / TARGETS-RADAR UI shell active."
    );


    BLEDevice::init("");

    pBLEScan =
        BLEDevice::getScan();

    pBLEScan->setActiveScan(false);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    pBLEScan->setAdvertisedDeviceCallbacks(
        &ridCallbacks,
        true,
        false
    );

    resetRIDCaptureState();

    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        " ESP32-S3 RID BACKEND PARITY HARNESS"
    );
    Serial.println(
        " ACCEPTED CYD1 RID ENGINE / NO UI"
    );
    Serial.println(
        "========================================"
    );
    Serial.println(
        "Boot state: HOME / BLE READY / PRIVATE / IDLE"
    );
    Serial.println(
        "Serial commands P/O/S remain available."
    );
    Serial.println();

    // Stage 1B-A:
    // BLE is initialized and ready, but the receiver remains IDLE
    // until the operator explicitly starts a capture session.
    //
    // Do NOT auto-start RID scanning from setup().
    scanSessionActive = false;
    scanCompletePending = false;
    scanStopRequested = false;

    Serial.println(
        "[SCAN] Receiver idle - awaiting operator START."
    );
}


// ============================================================
// LOOP
// ============================================================


// ============================================================
// BAT-D4A: AXP2101 BATTERY TELEMETRY ENGINE
// ISOLATED CANDIDATE - READ ONLY
// No PMIC configuration writes.
// ============================================================

struct RidBatD4aStatus {
    bool pmicOK = false;
    bool batteryPresent = false;
    bool usbPresent = false;
    bool voltageValid = false;
    bool percentageValid = false;

    uint16_t millivolts = 0;
    uint8_t percentage = 0;
    uint8_t chargePhase = 5;
    uint8_t currentDirection = 0;

    uint32_t sampleTimeMs = 0;
};

static RidBatD4aStatus ridBatD4a;

static bool ridBatD4aRead(
    uint8_t reg,
    uint8_t &value
) {
    const uint8_t address = 0x34;

    Wire.beginTransmission(address);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (
        Wire.requestFrom(
            address,
            (uint8_t)1
        ) != 1
    ) {
        return false;
    }

    value = Wire.read();
    return true;
}

static void ridBatD4aTick() {
    static uint32_t lastPoll = 0;

    uint32_t now = millis();

    if ((uint32_t)(now - lastPoll) < 2500UL) {
        return;
    }

    lastPoll = now;

    RidBatD4aStatus sample;
    sample.sampleTimeMs = now;

    uint8_t pmu = 0;
    uint8_t status = 0;

    bool pmuRead = ridBatD4aRead(0x00, pmu);
    bool statusRead = ridBatD4aRead(0x01, status);

    if (!pmuRead || !statusRead) {
        ridBatD4a = sample;

        if (Serial) {
            Serial.println(
                "[BAT-D4A] PMIC=ERROR BAT=UNKNOWN"
            );
        }
        return;
    }

    sample.pmicOK = true;
    sample.usbPresent = (pmu & 0x20) != 0;
    sample.batteryPresent = (pmu & 0x08) != 0;

    sample.chargePhase = status & 0x07;
    sample.currentDirection = (status >> 5) & 0x03;

    if (sample.batteryPresent) {
        uint8_t adc = 0;
        uint8_t high = 0;
        uint8_t low = 0;
        uint8_t soc = 0;

        bool adcOK = ridBatD4aRead(0x30, adc);
        bool highOK = ridBatD4aRead(0x34, high);
        bool lowOK = ridBatD4aRead(0x35, low);

        if (
            adcOK &&
            (adc & 0x01) &&
            highOK &&
            lowOK
        ) {
            uint16_t raw =
                ((uint16_t)(high & 0x3F) << 8) |
                (uint16_t)low;

            if (raw >= 1500 && raw <= 6000) {
                sample.millivolts = raw;
                sample.voltageValid = true;
            }
        }

        if (ridBatD4aRead(0xA4, soc)) {
            // AXP2101 gauge percentage is not
            // a precision capacity measurement.
            // Reject invalid / out-of-range values.
            if (soc <= 100) {
                sample.percentage = soc;
                sample.percentageValid = true;
            }
        }
    }

    // Replace the entire snapshot. No stale battery
    // value survives a loss of battery presence.
    ridBatD4a = sample;

    if (Serial) {
        if (!sample.batteryPresent) {
            Serial.printf(
                "[BAT-D4A] USB=%u BAT=ABSENT "
                "MV=NA SOC=NA PHASE=%u\n",
                (unsigned)sample.usbPresent,
                (unsigned)sample.chargePhase
            );
        } else {
            int mv = sample.voltageValid ?
                (int)sample.millivolts : -1;

            int pct = sample.percentageValid ?
                (int)sample.percentage : -1;

            Serial.printf(
                "[BAT-D4A] USB=%u BAT=PRESENT "
                "MV=%d SOC=%d PHASE=%u DIR=%u\n",
                (unsigned)sample.usbPresent,
                mv,
                pct,
                (unsigned)sample.chargePhase,
                (unsigned)sample.currentDirection
            );
        }
    }
}

// END BAT-D4A TELEMETRY ENGINE


// ============================================================
// BAT-D4B: OPERATOR POWER HEADER
// HOME, PRIVACY and LIVE; NOTICE/SNAPSHOT excluded.
// BAT-D4D-R2: Provisional AXP2101 EST percentage.
// ============================================================

// ============================================================
// BAT-D4E-3: LOW-VOLTAGE OPERATOR WARNING, TEST-ONLY
// Candidate thresholds; no PMIC writes / no automatic shutdown.
// Actual-voltage decision only. EST% is not involved.
// ============================================================
struct RidBatD4e3Warning {
    bool low = false;
    bool lowPending = false;
    bool clearPending = false;
    uint32_t lowSinceMs = 0;
    uint32_t clearSinceMs = 0;
};

static RidBatD4e3Warning ridBatD4e3Warning;

// Explicit prototype: prevent Arduino from inserting
// its declaration before the custom struct definition.
static void ridBatD4e3Step(
    RidBatD4e3Warning &w,
    const RidBatD4aStatus &s
);

static void ridBatD4e3Step(
    RidBatD4e3Warning &w,
    const RidBatD4aStatus &s
) {
    if (!s.pmicOK || !s.batteryPresent || s.usbPresent ||
        !s.voltageValid) {
        w = RidBatD4e3Warning();
        return;
    }

    if (!w.low) {
        w.clearPending = false;
        if (s.millivolts <= 3700U) {
            if (!w.lowPending) {
                w.lowPending = true;
                w.lowSinceMs = s.sampleTimeMs;
            } else if ((uint32_t)(s.sampleTimeMs - w.lowSinceMs)
                       >= 15000UL) {
                w.low = true;
                w.lowPending = false;
            }
        } else {
            w.lowPending = false;
        }
        return;
    }

    w.lowPending = false;
    if (s.millivolts >= 3800U) {
        if (!w.clearPending) {
            w.clearPending = true;
            w.clearSinceMs = s.sampleTimeMs;
        } else if ((uint32_t)(s.sampleTimeMs - w.clearSinceMs)
                   >= 30000UL) {
            w.low = false;
            w.clearPending = false;
        }
    } else {
        w.clearPending = false;
    }
}

static void ridBatD4e3Tick() {
    static uint32_t lastProcessedSample = 0;
    if (ridBatD4a.sampleTimeMs == 0 ||
        ridBatD4a.sampleTimeMs == lastProcessedSample) {
        return;
    }
    lastProcessedSample = ridBatD4a.sampleTimeMs;
    bool wasLow = ridBatD4e3Warning.low;
    ridBatD4e3Step(ridBatD4e3Warning, ridBatD4a);
    if (Serial && wasLow != ridBatD4e3Warning.low) {
        Serial.printf("[BAT-D4E3] %s voltage=%u mV\n",
            ridBatD4e3Warning.low ? "LOW BATTERY" : "WARNING CLEARED",
            (unsigned)ridBatD4a.millivolts);
    }
}

// END BAT-D4E-3 WARNING CODE

static void ridBatD4bRender(bool force) {
    static int previousScreen = -1;
    static char previousTop[24] = "";
    static char previousBottom[24] = "";

    if (
        wsCurrentScreen != WS_SCREEN_HOME &&
        wsCurrentScreen != WS_SCREEN_REFERENCE &&
        wsCurrentScreen != WS_SCREEN_PRIVACY &&
        wsCurrentScreen != WS_SCREEN_LIVE
    ) {
        previousScreen = -1;
        previousTop[0] = '\0';
        previousBottom[0] = '\0';
        return;
    }

    char top[24] = {0};
    char bottom[24] = {0};

    uint16_t topColor = CYAN;
    uint16_t bottomColor = WHITE;

    if (!ridBatD4a.pmicOK) {
        snprintf(
            top, sizeof(top),
            "BAT UNKNOWN"
        );
        snprintf(
            bottom, sizeof(bottom),
            "STATUS --"
        );
        topColor = YELLOW;
    }

    else if (!ridBatD4a.batteryPresent) {
        snprintf(
            top, sizeof(top),
            "BAT ABSENT"
        );
        snprintf(
            bottom, sizeof(bottom),
            "USB POWER"
        );
        topColor = YELLOW;
    }

    else {
        if (ridBatD4a.voltageValid) {
            unsigned cv =
                ((unsigned)ridBatD4a.millivolts + 5U)
                / 10U;

            snprintf(
                top, sizeof(top),
                "BAT %u.%02uV",
                cv / 100U,
                cv % 100U
            );
        } else {
            snprintf(
                top, sizeof(top),
                "BAT V:--"
            );
            topColor = YELLOW;
        }

        const char *powerState = "BAT";

        if (ridBatD4a.usbPresent) {
            switch (ridBatD4a.chargePhase) {
                case 2:
                case 3:
                    powerState = "CHG";
                    bottomColor = GREEN;
                    break;

                case 4:
                    powerState = "DONE";
                    bottomColor = GREEN;
                    break;

                default:
                    powerState = "USB";
                    break;
            }
        }

        // BAT-D4D-R2 provisional estimated SOC.
        // Require valid voltage and percentage.
        if (
            ridBatD4a.voltageValid &&
            ridBatD4a.percentageValid
        ) {
            snprintf(
                bottom, sizeof(bottom),
                "%s EST:%u%%",
                powerState,
                (unsigned)ridBatD4a.percentage
            );
        } else {
            snprintf(
                bottom, sizeof(bottom),
                "%s EST:--",
                powerState
            );
        }
    }

    // BAT-D4E-3 warning changes rendered text, forcing redraw.
    if (ridBatD4e3Warning.low &&
        ridBatD4a.pmicOK && ridBatD4a.batteryPresent &&
        !ridBatD4a.usbPresent && ridBatD4a.voltageValid) {
        snprintf(bottom, sizeof(bottom), "LOW BATTERY");
        topColor = YELLOW;
        bottomColor = YELLOW;
    }

    int screenID = (int)wsCurrentScreen;

    if (
        !force &&
        previousScreen == screenID &&
        strcmp(previousTop, top) == 0 &&
        strcmp(previousBottom, bottom) == 0
    ) {
        return;
    }

    // Reserve only the rightmost header region.
    // Do not erase the cyan separator at y=41.
    wsRenderFillRect(
        366, 2, 113, 38, BLACK
    );

    wsGfx->setTextSize(1);

    int topX =
        474 - (int)strlen(top) * 6;

    if (topX < 369) {
        topX = 369;
    }

    wsGfx->setTextColor(topColor);
    wsGfx->setCursor(topX, 7);
    wsGfx->print(top);
    ridGnssRenderCheckpoint();

    int bottomX =
        474 - (int)strlen(bottom) * 6;

    if (bottomX < 369) {
        bottomX = 369;
    }

    wsGfx->setTextColor(bottomColor);
    wsGfx->setCursor(bottomX, 23);
    wsGfx->print(bottom);
    ridGnssRenderCheckpoint();

    snprintf(
        previousTop,
        sizeof(previousTop),
        "%s", top
    );

    snprintf(
        previousBottom,
        sizeof(previousBottom),
        "%s", bottom
    );

    previousScreen = screenID;
}

// END BAT-D4B POWER HEADER


void loop() {

    ridBatD4aTick();
    ridBatD4e3Tick();
    ridGnssD3b2DiagBeforeReceive();
    ridGnssD3b1Tick();
    ridGnssRenderEnableAfterFirstReceive();
    ridGnssR2ReportUartErrorEvents();

    processSerialCommands();
    ridGnssD3b2Service();
    serviceRIDScanState();
    serviceWaveshareTouchUI();
    serviceWaveshareReferenceRefresh();
    serviceWaveshareRadarReadiness();
    serviceWaveshareLiveTargetRefresh();
    serviceWaveshareRadarRefresh();
    serviceTargetDashboard();

    ridBatD4bRender(false);

    delay(15);
}
