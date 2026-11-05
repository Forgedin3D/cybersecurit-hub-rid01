#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>


// ====================================================
// CYD1 REMOTE ID MULTI-TARGET ENGINE
// STAGE 7C
// ====================================================

BLEScan *pBLEScan;

const uint32_t STARTUP_DELAY_MS = 10000;
const uint32_t SCAN_TIME_SECONDS = 60;

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
};


RIDTarget targets[MAX_TARGETS];


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

    Serial.println();

    Serial.println(
        "========== RID TARGET SUMMARY =========="
    );


    Serial.print(
        "Output Mode: "
    );


    if (
        outputMode == MODE_PRIVATE
    ) {

        Serial.println(
            "PRIVATE"
        );
    }

    else {

        Serial.println(
            "OPEN"
        );
    }


    Serial.print(
        "Targets Tracked: "
    );

    Serial.println(
        getTrackedTargetCount()
    );


    Serial.print(
        "Targets Active: "
    );

    Serial.println(
        getActiveTargetCount()
    );


    Serial.println();


    for (
        uint8_t i = 0;
        i < MAX_TARGETS;
        i++
    ) {

        RIDTarget &target =
            targets[i];


        if (
            !target.allocated
        ) {

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


        Serial.print(
            targetStateName(
                target
            )
        );


        Serial.print(
            "  "
        );


        Serial.println(
            uaTypeName(
                target.uaType
            )
        );


        Serial.print(
            "State: "
        );

        Serial.println(
            flightStatusName(
                target.flightStatus
            )
        );


        Serial.print(
            "Position: "
        );


        if (
            target.hasPosition
        ) {

            Serial.println(
                "VALID"
            );
        }

        else {

            Serial.println(
                "UNKNOWN"
            );
        }


        Serial.print(
            "RSSI: "
        );

        Serial.print(
            getFilteredRSSI(
                target
            )
        );

        Serial.println(
            " dBm"
        );


        Serial.print(
            "Packets: "
        );

        Serial.println(
            target.packetCount
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


        Serial.println();
    }


    Serial.println(
        "========================================"
    );
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

void processSerialCommands() {

    while (
        Serial.available() > 0
    ) {

        char command =
            Serial.read();


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


            printAllTargetDetails();
        }
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


        // Commands are checked on all BLE traffic,
        // not only Remote ID packets.
        processSerialCommands();


        int dataCount =
            device.getServiceDataCount();


        int uuidCount =
            device.getServiceDataUUIDCount();


        int entryCount =
            dataCount;


        if (
            uuidCount > entryCount
        ) {

            entryCount =
                uuidCount;
        }


        for (
            int j = 0;
            j < entryCount;
            j++
        ) {

            String serviceUUIDText =
                "";


            if (
                j < uuidCount
            ) {

                serviceUUIDText =
                    device
                    .getServiceDataUUID(j)
                    .toString()
                    .c_str();
            }


            if (
                j >= dataCount
            ) {

                continue;
            }


            String serviceData =
                device.getServiceData(j);


            size_t serviceDataLength =
                serviceData.length();


            bool isRemoteIDUUID =

                serviceUUIDText
                .equalsIgnoreCase(
                    "0000fffa-0000-1000-8000-00805f9b34fb"
                )

                ||

                serviceUUIDText
                .equalsIgnoreCase(
                    "fffa"
                );


            bool hasOpenDroneIDCode =

                serviceDataLength >= 1

                &&

                getByte(
                    serviceData,
                    0
                ) == 0x0D;


            bool hasExpectedRemoteIDLength =

                serviceDataLength == 27;


            if (
                !isRemoteIDUUID
                ||
                !hasOpenDroneIDCode
                ||
                !hasExpectedRemoteIDLength
            ) {

                continue;
            }


            remoteIDPackets++;


            String sourceAddress =
                device
                .getAddress()
                .toString()
                .c_str();


            int targetIndex =
                findOrCreateTarget(
                    sourceAddress
                );


            if (
                targetIndex < 0
            ) {

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
                & 0x0F;


            uint8_t protocolVersion =
                odidHeader
                & 0x0F;


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
        }


        // ---------------------------------------------
        // Compact dashboard approximately once/second.
        // ---------------------------------------------

        if (
            millis()
            -
            lastDashboardPrintMs
            >=
            DASHBOARD_INTERVAL_MS
        ) {

            printTargetSummary();


            lastDashboardPrintMs =
                millis();
        }
    }
};


RemoteIDCallbacks ridCallbacks;


// ====================================================
// SETUP
// ====================================================

void setup() {

    Serial.begin(
        115200
    );


    delay(
        STARTUP_DELAY_MS
    );


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        " CYD1 REMOTE ID MULTI-TARGET ENGINE"
    );

    Serial.println(
        " STAGE 7C"
    );

    Serial.println(
        "========================================"
    );


    Serial.println(
        "Mode: PASSIVE BLE RECEIVE"
    );


    Serial.print(
        "Maximum RID targets: "
    );

    Serial.println(
        MAX_TARGETS
    );


    Serial.println(
        "Capture window: 60 seconds"
    );


    Serial.println();

    Serial.println(
        "Association method: BLE advertiser address"
    );


    Serial.println(
        "Known limitation: address rotation may create a new target."
    );


    Serial.println();

    Serial.println(
        "Output Mode: PRIVATE"
    );


    Serial.println(
        "Coordinates and identifiers redacted."
    );


    Serial.println();

    Serial.println(
        "Commands:"
    );


    Serial.println(
        "  P = PRIVATE output"
    );


    Serial.println(
        "  O = OPEN output"
    );


    Serial.println(
        "  S = Full multi-target snapshot"
    );


    Serial.println();


    BLEDevice::init(
        ""
    );


    pBLEScan =
        BLEDevice::getScan();


    pBLEScan->setActiveScan(
        false
    );


    pBLEScan->setInterval(
        100
    );


    pBLEScan->setWindow(
        99
    );


    pBLEScan
        ->setAdvertisedDeviceCallbacks(
            &ridCallbacks,
            true
        );


    Serial.println(
        "Starting multi-target RID capture..."
    );


    Serial.println();


    pBLEScan->start(
        SCAN_TIME_SECONDS,
        false
    );


    Serial.println();

    Serial.println(
        "*** FINAL MULTI-TARGET SNAPSHOT ***"
    );


    printAllTargetDetails();


    Serial.println();

    Serial.println(
        "=== STAGE 7C CAPTURE COMPLETE ==="
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
            messageTypeCounts[
                type
            ] > 0
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
                messageTypeName(
                    type
                )
            );


            Serial.print(
                "): "
            );


            Serial.println(
                messageTypeCounts[
                    type
                ]
            );
        }
    }


    Serial.println();

    Serial.println(
        "=== END OF STAGE 7C ==="
    );


    pBLEScan->clearResults();
}


// ====================================================
// LOOP
// ====================================================

void loop() {

    processSerialCommands();

    delay(
        50
    );
}
