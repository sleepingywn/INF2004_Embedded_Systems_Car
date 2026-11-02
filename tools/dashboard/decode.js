"use strict";
// Shared by the Node-RED receiver and host protocol checks.
function decode(payload) {
    if (!Buffer.isBuffer(payload) || payload.length !== 80)
        throw new Error("Expected a 80-byte RoboCar telemetry frame");
    if (payload[0] !== 82 || payload[1] !== 67 || payload[2] !== 1)
        throw new Error("Unsupported RoboCar protocol");
    if ((payload[3] & 0xc0) || payload[59] !== 0 || payload[66] || payload[67])
        throw new Error("Reserved protocol bits are set");
    const u32 = n => payload.readUInt32LE(n);
    const flags = payload[3];
    return {
        version: 1, bootId: u32(4), sequence: u32(8), publishedMs: u32(12),
        motionMs: u32(16), motionEpoch: u32(20), motionSequence: u32(24),
        leftCount: flags & 1 ? u32(28) : null,
        rightCount: flags & 1 ? u32(32) : null,
        irMs: u32(36), terrainAgeMs: u32(40),
        leftSpeed: (flags & 3) === 3 ? payload.readInt16LE(44) : null,
        rightSpeed: (flags & 3) === 3 ? payload.readInt16LE(46) : null,
        lineError: flags & 4 ? payload.readInt16LE(48) : null,
        pitch: flags & 8 ? payload.readInt8(50) / 2 : null,
        roll: flags & 8 ? payload.readInt8(51) / 2 : null,
        state: flags & 16 ? payload[52] : null, faults: flags & 16 ? payload[53] : null,
        result: flags & 16 ? payload[54] : null, ready: !!(flags & 32),
        currentPeakMm: flags & 8 ? payload[55] : null,
        highestPeakMm: flags & 8 ? payload[56] : null,
        humpCount: flags & 8 ? payload[57] : null,
        motionClass: flags & 8 ? payload[58] : null,
        droppedEvents: u32(60), valid: flags,
        eventType: payload[64], eventSource: payload[65], eventMs: u32(68),
        eventCount: u32(72), eventValue: payload.readInt32LE(76)
    };
}
module.exports = {decode};
